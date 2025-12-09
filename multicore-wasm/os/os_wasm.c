#include "os.h"
#include "lib/typedefs.h"
#include "lib/memory.h"

// Entity system (mirrors win32 pattern)
typedef enum {
  OS_WASM_ENTITY_NULL,
  OS_WASM_ENTITY_THREAD,
  OS_WASM_ENTITY_BARRIER,
  OS_WASM_ENTITY_MUTEX,
  OS_WASM_ENTITY_CONDVAR,
  OS_WASM_ENTITY_SEMAPHORE,
} OsWasmEntityKind;

typedef struct OsWasmEntity OsWasmEntity;
struct OsWasmEntity {
  OsWasmEntity *next; // for free list
  OsWasmEntityKind kind;
  union {
    struct {
      i32 js_thread_id;
      u32 stack_slot_idx; // index into stack_slots
      u32 tls_slot_idx;   // index into tls_slots
    } thread;
    struct {
      u32 slot_id; // index into barrier_slots
    } barrier;
    struct {
      u32 slot_id; // index into mutex_slots
    } mutex;
    struct {
      u32 slot_id; // index into condvar_slots
    } condvar;
    struct {
      u32 slot_id; // index into semaphore_slots
    } semaphore;
  };
};

// Barrier slot - contiguous memory for JS Atomics access
typedef struct {
  i32 count;      // number of threads that must arrive
  i32 generation; // toggles 0/1 to handle barrier reuse
  i32 arrived;    // atomic counter of threads that have arrived
} BarrierSlot;

// Mutex slot - single i32 for futex-based locking
// State: 0 = unlocked, 1 = locked (no waiters), 2 = locked (with waiters)
typedef struct {
  i32 state;  // accessed atomically via __c11_atomic_* functions
} MutexSlot;

// CondVar slot - sequence-based condition variable
// Waiters capture seq before sleeping, signal/broadcast increment it
typedef struct {
  i32 seq;      // sequence number, incremented on signal/broadcast
  i32 waiters;  // count of waiting threads (optimization to skip wake)
} CondVarSlot;

// Semaphore slot - futex-based counting semaphore (musl pattern)
// count: low 31 bits = semaphore value, high bit (0x80000000) = "has waiters" flag
// waiters: number of threads waiting (optimization to skip wake when 0)
#define SEM_VALUE_MAX 0x7FFFFFFF
#define SEM_WAITER_FLAG 0x80000000
typedef struct {
  i32 count;    // atomic: value | waiter_flag
  i32 waiters;  // atomic: waiter count
} SemaphoreSlot;

// Global state
#define MAX_THREADS 32
#define MAX_BARRIERS 4
#define MAX_MUTEXES 32
#define MAX_CONDVARS 32
#define MAX_SEMAPHORES 32
#define OS_WASM_ENTITY_POOL_SIZE 64
#define THREAD_STACK_SIZE (64 * 1024) // 64KB per thread stack

// Stack slot for thread - tracks if in use and its top address
typedef struct {
  b32 in_use;
  u32 stack_top; // top of stack (stack grows down from here)
} ThreadStackSlot;

// TLS slot for thread - tracks if in use and its base address
typedef struct {
  b32 in_use;
  u32 tls_base; // base address of this thread's TLS block
} ThreadTlsSlot;

typedef struct {
  i32 thread_flags[MAX_THREADS]; // one i32 per thread for JS Atomics
  BarrierSlot barrier_slots[MAX_BARRIERS];
  MutexSlot mutex_slots[MAX_MUTEXES];
  CondVarSlot condvar_slots[MAX_CONDVARS];
  SemaphoreSlot semaphore_slots[MAX_SEMAPHORES];
  OsWasmEntity entities[OS_WASM_ENTITY_POOL_SIZE];
  OsWasmEntity *entity_free;
  u32 next_entity_idx;
  u32 next_barrier_slot;
  u32 next_mutex_slot;
  u32 next_condvar_slot;
  u32 next_semaphore_slot;
  ThreadStackSlot stack_slots[MAX_THREADS];
  b32 stacks_initialized;
  ThreadTlsSlot tls_slots[MAX_THREADS];
  b32 tls_initialized;
} OsWasmState;

global OsWasmState os_wasm_state = {0};

// Entity allocation
internal OsWasmEntity *os_wasm_entity_alloc(OsWasmEntityKind kind) {
  OsWasmEntity *result = 0;
  if (os_wasm_state.entity_free) {
    result = os_wasm_state.entity_free;
    os_wasm_state.entity_free = result->next;
  } else if (os_wasm_state.next_entity_idx < OS_WASM_ENTITY_POOL_SIZE) {
    result = &os_wasm_state.entities[os_wasm_state.next_entity_idx++];
  }
  if (result) {
    memset(result, 0, sizeof(OsWasmEntity));
    result->kind = kind;
  }
  return result;
}

internal void os_wasm_entity_release(OsWasmEntity *entity) {
  if (!entity)
    return;
  entity->kind = OS_WASM_ENTITY_NULL;
  entity->next = os_wasm_state.entity_free;
  os_wasm_state.entity_free = entity;
}

// Stack slot management
extern unsigned char __heap_base;

internal void os_wasm_init_stacks(void) {
  if (os_wasm_state.stacks_initialized)
    return;
  u32 stacks_base = (u32)(uintptr)&__heap_base;
  for (u32 i = 0; i < MAX_THREADS; i++) {
    // Stack grows down, so stack_top is at the END of each region
    os_wasm_state.stack_slots[i].stack_top =
        stacks_base + (i + 1) * THREAD_STACK_SIZE;
    os_wasm_state.stack_slots[i].in_use = 0;
  }
  os_wasm_state.stacks_initialized = 1;
}

internal i32 os_wasm_stack_alloc(void) {
  os_wasm_init_stacks();
  for (u32 i = 0; i < MAX_THREADS; i++) {
    if (!os_wasm_state.stack_slots[i].in_use) {
      os_wasm_state.stack_slots[i].in_use = 1;
      return (i32)i;
    }
  }
  return -1; // no free slots
}

internal void os_wasm_stack_free(u32 slot_idx) {
  if (slot_idx < MAX_THREADS) {
    os_wasm_state.stack_slots[slot_idx].in_use = 0;
  }
}

internal u32 os_wasm_stack_get_top(u32 slot_idx) {
  return os_wasm_state.stack_slots[slot_idx].stack_top;
}

// TLS slot management
// Use compiler intrinsics to get TLS info (linker provides these as WASM
// globals)
internal u32 os_wasm_get_tls_size(void) { return __builtin_wasm_tls_size(); }

internal u32 os_wasm_get_tls_align(void) { return __builtin_wasm_tls_align(); }

internal u32 os_wasm_get_tls_region_base(void) {
  // TLS region starts right after thread stacks
  return (u32)(uintptr)&__heap_base + (MAX_THREADS * THREAD_STACK_SIZE);
}

internal void os_wasm_init_tls(void) {
  if (os_wasm_state.tls_initialized)
    return;

  u32 tls_size = os_wasm_get_tls_size();
  u32 tls_align = os_wasm_get_tls_align();
  if (tls_size == 0) {
    os_wasm_state.tls_initialized = 1;
    return;
  }

  // Calculate aligned TLS block size
  u32 aligned_tls_size = (tls_size + tls_align - 1) & ~(tls_align - 1);

  u32 tls_region_base = os_wasm_get_tls_region_base();
  for (u32 i = 0; i < MAX_THREADS; i++) {
    os_wasm_state.tls_slots[i].tls_base =
        tls_region_base + (i * aligned_tls_size);
    os_wasm_state.tls_slots[i].in_use = 0;
  }
  // Reserve slot 0 for main thread (initialized by JS before spawning workers)
  os_wasm_state.tls_slots[0].in_use = 1;
  os_wasm_state.tls_initialized = 1;
}

internal i32 os_wasm_tls_alloc(void) {
  os_wasm_init_tls();
  for (u32 i = 0; i < MAX_THREADS; i++) {
    if (!os_wasm_state.tls_slots[i].in_use) {
      os_wasm_state.tls_slots[i].in_use = 1;
      return (i32)i;
    }
  }
  return -1; // no free slots
}

internal void os_wasm_tls_free(u32 slot_idx) {
  if (slot_idx < MAX_THREADS) {
    os_wasm_state.tls_slots[slot_idx].in_use = 0;
  }
}

internal u32 os_wasm_tls_get_base(u32 slot_idx) {
  return os_wasm_state.tls_slots[slot_idx].tls_base;
}

// Export TLS info to JS
WASM_EXPORT(get_tls_slot_base)
u32 get_tls_slot_base(u32 slot_idx) {
  os_wasm_init_tls();
  if (slot_idx >= MAX_THREADS)
    return 0;
  return os_wasm_state.tls_slots[slot_idx].tls_base;
}

// Getter functions to export addresses to JS
WASM_EXPORT(get_thread_flags_ptr)
i32 *get_thread_flags_ptr(void) { return os_wasm_state.thread_flags; }

WASM_EXPORT(get_barrier_data_ptr)
BarrierSlot *get_barrier_data_ptr(void) { return os_wasm_state.barrier_slots; }

// JS imports
WASM_IMPORT(js_log) extern void js_log(const char *str, int len);
WASM_IMPORT(js_get_core_count) extern u32 js_get_core_count(void);
WASM_IMPORT(js_thread_spawn)
extern int js_thread_spawn(void *func, void *arg, u32 stack_top, u32 tls_base);
WASM_IMPORT(js_thread_join) extern void js_thread_join(int thread_id);
WASM_IMPORT(js_barrier_wait) extern void js_barrier_wait(u32 barrier_id);

WASM_IMPORT(_os_log_info)
void _os_log_info(const char *message, int length, const char *file_name,
                  int file_name_len, int line_num);
WASM_IMPORT(_os_log_warn)
void _os_log_warn(const char *message, int length, const char *file_name,
                  int file_name_len, int line_num);
WASM_IMPORT(_os_log_error)
void _os_log_error(const char *message, int length, const char *file_name,
                   int file_name_len, int line_num);

// Simple print - just logs the string, no format parsing
void print(const char *str) {
  int len = 0;
  while (str[len])
    len++;
  js_log(str, len);
}

void os_log(LogLevel log_level, const char *fmt, const FmtArgs *args,
            const char *file_name, uint32 line_number) {
  char buffer[1024 * 8];
  size_t msg_len = fmt_string(buffer, sizeof(buffer), fmt, args);
  const size_t file_name_len = str_len(file_name);
  switch (log_level) {
  case LOGLEVEL_INFO:
    _os_log_info(buffer, msg_len, file_name, file_name_len, line_number);
    break;
  case LOGLEVEL_WARN:
    _os_log_warn(buffer, msg_len, file_name, file_name_len, line_number);
    break;
  case LOGLEVEL_ERROR:
    _os_log_error(buffer, msg_len, file_name, file_name_len, line_number);
    break;
  default:
    break;
  }
}

void assert_log(u8 log_level, const char *fmt, const FmtArgs *args,
                const char *file_name, uint32 line_number) {
  os_log(log_level, fmt, args, file_name, line_number);
}

// Print string followed by integer
void print_int(const char *prefix, i32 value) {
  char buffer[128];
  int pos = 0;

  // Copy prefix
  while (prefix[pos] && pos < 100) {
    buffer[pos] = prefix[pos];
    pos++;
  }

  // Convert integer to string
  if (value == 0) {
    buffer[pos++] = '0';
  } else {
    b32 negative = value < 0;
    if (negative)
      value = -value;

    char digits[16];
    int digit_count = 0;
    while (value > 0) {
      digits[digit_count++] = '0' + (value % 10);
      value /= 10;
    }

    if (negative)
      buffer[pos++] = '-';

    // Reverse digits into buffer
    while (digit_count > 0) {
      buffer[pos++] = digits[--digit_count];
    }
  }

  js_log(buffer, pos);
}

// Threads
Thread os_thread_launch(ThreadFunc fn, void *arg) {
  Thread result = {0};
  OsWasmEntity *entity = os_wasm_entity_alloc(OS_WASM_ENTITY_THREAD);
  if (!entity)
    return result;

  // Allocate a stack slot for this thread
  i32 stack_slot = os_wasm_stack_alloc();
  if (stack_slot < 0) {
    os_wasm_entity_release(entity);
    return result;
  }

  // Allocate a TLS slot for this thread
  i32 tls_slot = os_wasm_tls_alloc();
  if (tls_slot < 0) {
    os_wasm_stack_free((u32)stack_slot);
    os_wasm_entity_release(entity);
    return result;
  }

  entity->thread.stack_slot_idx = (u32)stack_slot;
  entity->thread.tls_slot_idx = (u32)tls_slot;
  u32 stack_top = os_wasm_stack_get_top((u32)stack_slot);
  u32 tls_base = os_wasm_tls_get_base((u32)tls_slot);
  entity->thread.js_thread_id =
      js_thread_spawn((void *)fn, arg, stack_top, tls_base);
  result.v[0] = (u64)entity;
  return result;
}

b32 os_thread_join(Thread t, u64 timeout_us) {
  (void)timeout_us;
  if (t.v[0] == 0)
    return 0;

  OsWasmEntity *entity = (OsWasmEntity *)t.v[0];
  js_thread_join(entity->thread.js_thread_id);
  os_wasm_stack_free(entity->thread.stack_slot_idx);
  os_wasm_tls_free(entity->thread.tls_slot_idx);
  os_wasm_entity_release(entity);
  return 1;
}

void os_thread_detach(Thread t) {
  (void)t;
  print("os_thread_detach: not implemented");
}

void os_thread_set_name(Thread t, const char *name) {
  (void)t;
  (void)name;
  // No-op: Web Workers don't have a standard way to set thread names
}

i32 os_get_processor_count(void) { return js_get_core_count(); }

// Barriers
Barrier os_barrier_alloc(u32 count) {
  Barrier result = {0};
  OsWasmEntity *entity = os_wasm_entity_alloc(OS_WASM_ENTITY_BARRIER);
  if (!entity)
    return result;

  u32 slot_id = os_wasm_state.next_barrier_slot++;
  entity->barrier.slot_id = slot_id;

  os_wasm_state.barrier_slots[slot_id] = (BarrierSlot){
      .count = (i32)count,
      .generation = 0,
      .arrived = 0,
  };

  result.v[0] = (u64)entity;
  return result;
}

void os_barrier_wait(Barrier b) {
  if (b.v[0] == 0)
    return;
  OsWasmEntity *entity = (OsWasmEntity *)b.v[0];
  js_barrier_wait(entity->barrier.slot_id);
}

void os_barrier_release(Barrier b) {
  if (b.v[0] == 0)
    return;
  OsWasmEntity *entity = (OsWasmEntity *)b.v[0];
  os_wasm_entity_release(entity);
}

// Mutex implementation using futex-style locking
// State: 0 = unlocked, 1 = locked (no waiters), 2 = locked (with waiters)
//
// Algorithm (based on Ulrich Drepper's "Futexes Are Tricky"):
// Lock:
//   1. Fast path: CAS 0 -> 1, if success we have the lock
//   2. Spin briefly for short critical sections
//   3. Mark waiters (state = 2) and sleep via futex
// Unlock:
//   1. Atomically set state to 0
//   2. If old state was 2 (had waiters), wake one thread

#define MUTEX_UNLOCKED 0
#define MUTEX_LOCKED 1
#define MUTEX_LOCKED_WITH_WAITERS 2
#define MUTEX_SPIN_COUNT 100

// Atomic compare-and-swap: if *p == expected, set *p = desired, return old value
force_inline i32 atomic_cas_i32(i32 *p, i32 expected, i32 desired) {
  __c11_atomic_compare_exchange_strong((_Atomic i32 *)p, &expected, desired,
                                       __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
  return expected;
}

// Atomic exchange: set *p = val, return old value
force_inline i32 atomic_swap_i32(i32 *p, i32 val) {
  return __c11_atomic_exchange((_Atomic i32 *)p, val, __ATOMIC_SEQ_CST);
}

// Atomic load
force_inline i32 atomic_load_i32(i32 *p) {
  return __c11_atomic_load((_Atomic i32 *)p, __ATOMIC_SEQ_CST);
}

Mutex os_mutex_alloc(void) {
  Mutex result = {0};
  OsWasmEntity *entity = os_wasm_entity_alloc(OS_WASM_ENTITY_MUTEX);
  if (!entity)
    return result;

  u32 slot_id = os_wasm_state.next_mutex_slot++;
  entity->mutex.slot_id = slot_id;

  os_wasm_state.mutex_slots[slot_id].state = MUTEX_UNLOCKED;

  result.v[0] = (u64)entity;
  return result;
}

void os_mutex_release(Mutex m) {
  if (m.v[0] == 0)
    return;
  OsWasmEntity *entity = (OsWasmEntity *)m.v[0];
  os_wasm_entity_release(entity);
}

void os_mutex_take(Mutex m) {
  if (m.v[0] == 0)
    return;

  OsWasmEntity *entity = (OsWasmEntity *)m.v[0];
  MutexSlot *slot = &os_wasm_state.mutex_slots[entity->mutex.slot_id];

  // Fast path: try to acquire uncontended lock (CAS 0 -> 1)
  if (atomic_cas_i32(&slot->state, MUTEX_UNLOCKED, MUTEX_LOCKED) == MUTEX_UNLOCKED) {
    return; // Got the lock
  }

  // Spin loop: good for short critical sections, avoids expensive futex syscall
  for (i32 i = 0; i < MUTEX_SPIN_COUNT; i++) {
    i32 state = atomic_load_i32(&slot->state);
    if (state == MUTEX_UNLOCKED &&
        atomic_cas_i32(&slot->state, MUTEX_UNLOCKED, MUTEX_LOCKED) == MUTEX_UNLOCKED) {
      return; // Got the lock while spinning
    }
  }

  // Slow path: mark waiters and sleep
  // Use swap instead of CAS - simpler and we need state=2 anyway
  while (atomic_swap_i32(&slot->state, MUTEX_LOCKED_WITH_WAITERS) != MUTEX_UNLOCKED) {
    // Wait until state changes from LOCKED_WITH_WAITERS
    // timeout = -1 means wait indefinitely
    __builtin_wasm_memory_atomic_wait32(&slot->state, MUTEX_LOCKED_WITH_WAITERS, -1);
  }
}

void os_mutex_drop(Mutex m) {
  if (m.v[0] == 0)
    return;

  OsWasmEntity *entity = (OsWasmEntity *)m.v[0];
  MutexSlot *slot = &os_wasm_state.mutex_slots[entity->mutex.slot_id];

  // Atomically release the lock
  i32 old_state = atomic_swap_i32(&slot->state, MUTEX_UNLOCKED);

  // If there were waiters, wake one up
  if (old_state == MUTEX_LOCKED_WITH_WAITERS) {
    __builtin_wasm_memory_atomic_notify(&slot->state, 1);
  }
}

// CondVar implementation using sequence-based futex pattern
// Based on musl's "shared" condvar mode - simple and correct for WASM

CondVar os_cond_var_alloc(void) {
  CondVar result = {0};
  OsWasmEntity *entity = os_wasm_entity_alloc(OS_WASM_ENTITY_CONDVAR);
  if (!entity)
    return result;

  u32 slot_id = os_wasm_state.next_condvar_slot++;
  entity->condvar.slot_id = slot_id;

  os_wasm_state.condvar_slots[slot_id].seq = 0;
  os_wasm_state.condvar_slots[slot_id].waiters = 0;

  result.v[0] = (u64)entity;
  return result;
}

void os_cond_var_release(CondVar cv) {
  if (cv.v[0] == 0)
    return;
  OsWasmEntity *entity = (OsWasmEntity *)cv.v[0];
  os_wasm_entity_release(entity);
}

b32 os_cond_var_wait(CondVar cv, Mutex m, u64 timeout_us) {
  if (cv.v[0] == 0 || m.v[0] == 0)
    return 0;

  OsWasmEntity *cv_entity = (OsWasmEntity *)cv.v[0];
  CondVarSlot *slot = &os_wasm_state.condvar_slots[cv_entity->condvar.slot_id];

  // Increment waiter count
  __c11_atomic_fetch_add((_Atomic i32 *)&slot->waiters, 1, __ATOMIC_SEQ_CST);

  // Capture current sequence before releasing mutex
  i32 seq = atomic_load_i32(&slot->seq);

  // Release the mutex (this is the atomic "unlock and sleep" part)
  os_mutex_drop(m);

  // Wait until seq changes (signal/broadcast increments it)
  // timeout_us: 0 = infinite wait, >0 = timeout in microseconds
  i64 timeout_ns = (timeout_us == 0) ? -1 : (i64)(timeout_us * 1000);
  __builtin_wasm_memory_atomic_wait32(&slot->seq, seq, timeout_ns);

  // Decrement waiter count
  __c11_atomic_fetch_sub((_Atomic i32 *)&slot->waiters, 1, __ATOMIC_SEQ_CST);

  // Re-acquire the mutex before returning
  os_mutex_take(m);

  return 1;
}

void os_cond_var_signal(CondVar cv) {
  if (cv.v[0] == 0)
    return;

  OsWasmEntity *entity = (OsWasmEntity *)cv.v[0];
  CondVarSlot *slot = &os_wasm_state.condvar_slots[entity->condvar.slot_id];

  // Only wake if there are waiters (optimization)
  if (atomic_load_i32(&slot->waiters) > 0) {
    __c11_atomic_fetch_add((_Atomic i32 *)&slot->seq, 1, __ATOMIC_SEQ_CST);
    __builtin_wasm_memory_atomic_notify(&slot->seq, 1);
  }
}

void os_cond_var_broadcast(CondVar cv) {
  if (cv.v[0] == 0)
    return;

  OsWasmEntity *entity = (OsWasmEntity *)cv.v[0];
  CondVarSlot *slot = &os_wasm_state.condvar_slots[entity->condvar.slot_id];

  // Only wake if there are waiters (optimization)
  if (atomic_load_i32(&slot->waiters) > 0) {
    __c11_atomic_fetch_add((_Atomic i32 *)&slot->seq, 1, __ATOMIC_SEQ_CST);
    // Wake all waiters (use large number, WASM doesn't have INT_MAX issues here)
    __builtin_wasm_memory_atomic_notify(&slot->seq, 0x7FFFFFFF);
  }
}

// Semaphore implementation - futex-based counting semaphore (musl pattern)
// Fast path: single atomic CAS for uncontended case
// Slow path: spin briefly, then futex_wait

#define SEM_SPIN_COUNT 100

Semaphore os_semaphore_alloc(i32 initial_count) {
  Semaphore result = {0};
  OsWasmEntity *entity = os_wasm_entity_alloc(OS_WASM_ENTITY_SEMAPHORE);
  if (!entity)
    return result;

  u32 slot_id = os_wasm_state.next_semaphore_slot++;
  entity->semaphore.slot_id = slot_id;

  os_wasm_state.semaphore_slots[slot_id].count = initial_count;
  os_wasm_state.semaphore_slots[slot_id].waiters = 0;

  result.v[0] = (u64)entity;
  return result;
}

void os_semaphore_release(Semaphore s) {
  if (s.v[0] == 0)
    return;
  OsWasmEntity *entity = (OsWasmEntity *)s.v[0];
  os_wasm_entity_release(entity);
}

// sem_trywait: non-blocking decrement
// Returns 1 on success (acquired), 0 if would block
internal b32 os_semaphore_trywait(SemaphoreSlot *slot) {
  i32 val;
  while ((val = atomic_load_i32(&slot->count)) & SEM_VALUE_MAX) {
    // count > 0, try to decrement
    if (atomic_cas_i32(&slot->count, val, val - 1) == val) {
      return 1; // acquired
    }
  }
  return 0; // would block
}

void os_semaphore_take(Semaphore s) {
  if (s.v[0] == 0)
    return;

  OsWasmEntity *entity = (OsWasmEntity *)s.v[0];
  SemaphoreSlot *slot = &os_wasm_state.semaphore_slots[entity->semaphore.slot_id];

  // Fast path: try non-blocking acquire
  if (os_semaphore_trywait(slot))
    return;

  // Spin briefly - good for short critical sections
  for (i32 i = 0; i < SEM_SPIN_COUNT; i++) {
    if ((atomic_load_i32(&slot->count) & SEM_VALUE_MAX) && os_semaphore_trywait(slot))
      return;
  }

  // Slow path: register as waiter and sleep
  for (;;) {
    // Increment waiter count
    __c11_atomic_fetch_add((_Atomic i32 *)&slot->waiters, 1, __ATOMIC_SEQ_CST);

    // Set the "has waiters" flag via CAS(0, 0x80000000)
    atomic_cas_i32(&slot->count, 0, SEM_WAITER_FLAG);

    // Wait until count changes from the "waiters only" state
    i32 current = atomic_load_i32(&slot->count);
    if (!(current & SEM_VALUE_MAX)) {
      // count == 0 (only waiter flag may be set), sleep
      __builtin_wasm_memory_atomic_wait32(&slot->count, current, -1);
    }

    // Decrement waiter count
    __c11_atomic_fetch_sub((_Atomic i32 *)&slot->waiters, 1, __ATOMIC_SEQ_CST);

    // Try to acquire again
    if (os_semaphore_trywait(slot))
      return;
  }
}

void os_semaphore_drop(Semaphore s) {
  if (s.v[0] == 0)
    return;

  OsWasmEntity *entity = (OsWasmEntity *)s.v[0];
  SemaphoreSlot *slot = &os_wasm_state.semaphore_slots[entity->semaphore.slot_id];

  // Atomically increment count
  i32 val, new_val;
  do {
    val = atomic_load_i32(&slot->count);
    new_val = (val & SEM_VALUE_MAX) + 1;
    // Clear waiter flag if only one waiter left
    if (atomic_load_i32(&slot->waiters) <= 1) {
      new_val &= ~SEM_WAITER_FLAG;
    } else {
      new_val |= (val & SEM_WAITER_FLAG); // preserve waiter flag
    }
  } while (atomic_cas_i32(&slot->count, val, new_val) != val);

  // If there were waiters (old value had waiter flag or was 0), wake one
  if ((val & SEM_WAITER_FLAG) || atomic_load_i32(&slot->waiters) > 0) {
    __builtin_wasm_memory_atomic_notify(&slot->count, 1);
  }
}

// Set the __stack_pointer global - used by workers to set their own stack
WASM_EXPORT(set_stack_pointer)
void set_stack_pointer(u32 sp) {
  __asm__("local.get %0\n"
          "global.set __stack_pointer\n"
          :
          : "r"(sp));
}

WASM_EXPORT(os_get_heap_base)
void *os_get_heap_base(void) {
  // Memory layout after __heap_base:
  // [Thread Stacks: MAX_THREADS * THREAD_STACK_SIZE]
  // [TLS Region: MAX_THREADS * aligned_tls_size]
  // [Application Heap starts here]

  u32 tls_size = os_wasm_get_tls_size();
  u32 tls_align = os_wasm_get_tls_align();
  u32 aligned_tls_size =
      tls_size > 0 ? (tls_size + tls_align - 1) & ~(tls_align - 1) : 0;
  u32 tls_region_size = MAX_THREADS * aligned_tls_size;

  u8 *base = &__heap_base + (MAX_THREADS * THREAD_STACK_SIZE) + tls_region_size;
#ifdef DEBUG
  // add 1MB padding on debug builds to support hot reload
  base += KB(1024);
#endif
  return base;
}

// Memory
// void *os_allocate_memory(size_t size) { return malloc(size); }
//
// void os_free_memory(void *ptr, size_t size) {
//   UNUSED(size);
//   free(ptr);
// }
//
// u8 *os_reserve_memory(size_t size) {
//   // In WASM, we don't have virtual memory - just allocate directly
//   return (u8 *)malloc(size);
// }
//
// b32 os_commit_memory(void *ptr, size_t size) {
//   // In WASM, memory is already committed when allocated
//   UNUSED(ptr);
//   UNUSED(size);
//   return true;
// }
//
// // Threads
// typedef struct {
//   ThreadFunc fn;
//   void *arg;
// } ThreadStartData;
//
// static void *thread_start_wrapper(void *arg) {
//   ThreadStartData *data = (ThreadStartData *)arg;
//   ThreadFunc fn = data->fn;
//   void *user_arg = data->arg;
//   free(data);
//   fn(user_arg);
//   return NULL;
// }
//
// Thread os_thread_launch(ThreadFunc fn, void *arg) {
//   pthread_t thread;
//   ThreadStartData *data = (ThreadStartData *)malloc(sizeof(ThreadStartData));
//   data->fn = fn;
//   data->arg = arg;
//   pthread_create(&thread, NULL, thread_start_wrapper, data);
//   Thread t = {.v = {(u64)thread}};
//   return t;
// }
//
// b32 os_thread_join(Thread t, u64 timeout_us) {
//   UNUSED(timeout_us);
//   pthread_join((pthread_t)t.v[0], NULL);
//   return true;
// }
//
// void os_thread_detach(Thread t) { pthread_detach((pthread_t)t.v[0]); }
//
// void os_thread_set_name(Thread t, const char *name) {
//   UNUSED(t);
//   UNUSED(name);
//   // Not easily supported in WASM pthreads
// }
//
// // Mutex
// Mutex os_mutex_alloc(void) {
//   pthread_mutex_t *m = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
//   pthread_mutex_init(m, NULL);
//   Mutex mutex = {.v = {(u64)m}};
//   return mutex;
// }
//
// void os_mutex_release(Mutex m) {
//   pthread_mutex_t *pm = (pthread_mutex_t *)m.v[0];
//   pthread_mutex_destroy(pm);
//   free(pm);
// }
//
// void os_mutex_take(Mutex m) { pthread_mutex_lock((pthread_mutex_t *)m.v[0]);
// }
//
// void os_mutex_drop(Mutex m) { pthread_mutex_unlock((pthread_mutex_t
// *)m.v[0]); }
//
// // RWMutex
// RWMutex os_rw_mutex_alloc(void) {
//   pthread_rwlock_t *rw = (pthread_rwlock_t
//   *)malloc(sizeof(pthread_rwlock_t)); pthread_rwlock_init(rw, NULL); RWMutex
//   mutex = {.v = {(u64)rw}}; return mutex;
// }
//
// void os_rw_mutex_release(RWMutex m) {
//   pthread_rwlock_t *rw = (pthread_rwlock_t *)m.v[0];
//   pthread_rwlock_destroy(rw);
//   free(rw);
// }
//
// void os_rw_mutex_take_r(RWMutex m) {
//   pthread_rwlock_rdlock((pthread_rwlock_t *)m.v[0]);
// }
//
// void os_rw_mutex_drop_r(RWMutex m) {
//   pthread_rwlock_unlock((pthread_rwlock_t *)m.v[0]);
// }
//
// void os_rw_mutex_take_w(RWMutex m) {
//   pthread_rwlock_wrlock((pthread_rwlock_t *)m.v[0]);
// }
//
// void os_rw_mutex_drop_w(RWMutex m) {
//   pthread_rwlock_unlock((pthread_rwlock_t *)m.v[0]);
// }
//
// // CondVar
// CondVar os_cond_var_alloc(void) {
//   pthread_cond_t *cv = (pthread_cond_t *)malloc(sizeof(pthread_cond_t));
//   pthread_cond_init(cv, NULL);
//   CondVar cond = {.v = {(u64)cv}};
//   return cond;
// }
//
// void os_cond_var_release(CondVar cv) {
//   pthread_cond_t *pcv = (pthread_cond_t *)cv.v[0];
//   pthread_cond_destroy(pcv);
//   free(pcv);
// }
//
// b32 os_cond_var_wait(CondVar cv, Mutex m, u64 timeout_us) {
//   UNUSED(timeout_us);
//   pthread_cond_wait((pthread_cond_t *)cv.v[0], (pthread_mutex_t *)m.v[0]);
//   return true;
// }
//
// void os_cond_var_signal(CondVar cv) {
//   pthread_cond_signal((pthread_cond_t *)cv.v[0]);
// }
//
// void os_cond_var_broadcast(CondVar cv) {
//   pthread_cond_broadcast((pthread_cond_t *)cv.v[0]);
// }
//
// // Semaphore (emulated with mutex + condvar)
// typedef struct {
//   pthread_mutex_t mutex;
//   pthread_cond_t cond;
//   i32 count;
// } WasmSemaphore;
//
// Semaphore os_semaphore_alloc(i32 initial_count) {
//   WasmSemaphore *sem = (WasmSemaphore *)malloc(sizeof(WasmSemaphore));
//   pthread_mutex_init(&sem->mutex, NULL);
//   pthread_cond_init(&sem->cond, NULL);
//   sem->count = initial_count;
//   Semaphore s = {.v = {(u64)sem}};
//   return s;
// }
//
// void os_semaphore_release(Semaphore s) {
//   WasmSemaphore *sem = (WasmSemaphore *)s.v[0];
//   pthread_mutex_destroy(&sem->mutex);
//   pthread_cond_destroy(&sem->cond);
//   free(sem);
// }
//
// void os_semaphore_take(Semaphore s) {
//   WasmSemaphore *sem = (WasmSemaphore *)s.v[0];
//   pthread_mutex_lock(&sem->mutex);
//   while (sem->count <= 0) {
//     pthread_cond_wait(&sem->cond, &sem->mutex);
//   }
//   sem->count--;
//   pthread_mutex_unlock(&sem->mutex);
// }
//
// void os_semaphore_drop(Semaphore s) {
//   WasmSemaphore *sem = (WasmSemaphore *)s.v[0];
//   pthread_mutex_lock(&sem->mutex);
//   sem->count++;
//   pthread_cond_signal(&sem->cond);
//   pthread_mutex_unlock(&sem->mutex);
// }
//
// // Barrier
// Barrier os_barrier_alloc(u32 count) {
//   pthread_barrier_t *b = (pthread_barrier_t
//   *)malloc(sizeof(pthread_barrier_t)); pthread_barrier_init(b, NULL, count);
//   Barrier barrier = {.v = {(u64)b}};
//   return barrier;
// }
//
// void os_barrier_release(Barrier b) {
//   pthread_barrier_t *pb = (pthread_barrier_t *)b.v[0];
//   pthread_barrier_destroy(pb);
//   free(pb);
// }
//
// void os_barrier_wait(Barrier b) {
//   pthread_barrier_wait((pthread_barrier_t *)b.v[0]);
// }
