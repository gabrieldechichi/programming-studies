#include "os.h"
#include "lib/typedefs.h"
#include "lib/memory.h"

// Entity system (mirrors win32 pattern)
typedef enum {
  OS_WASM_ENTITY_NULL,
  OS_WASM_ENTITY_THREAD,
  OS_WASM_ENTITY_BARRIER,
} OsWasmEntityKind;

typedef struct OsWasmEntity OsWasmEntity;
struct OsWasmEntity {
  OsWasmEntity *next; // for free list
  OsWasmEntityKind kind;
  union {
    struct {
      i32 js_thread_id;
      u32 stack_slot_idx; // index into stack_slots
    } thread;
    struct {
      u32 slot_id; // index into barrier_slots
    } barrier;
  };
};

// Barrier slot - contiguous memory for JS Atomics access
typedef struct {
  i32 count;      // number of threads that must arrive
  i32 generation; // toggles 0/1 to handle barrier reuse
  i32 arrived;    // atomic counter of threads that have arrived
} BarrierSlot;

// Global state
#define MAX_THREADS 32
#define MAX_BARRIERS 4
#define OS_WASM_ENTITY_POOL_SIZE 64
#define THREAD_STACK_SIZE (64 * 1024) // 64KB per thread stack

// Stack slot for thread - tracks if in use and its top address
typedef struct {
  b32 in_use;
  u32 stack_top; // top of stack (stack grows down from here)
} ThreadStackSlot;

typedef struct {
  i32 thread_flags[MAX_THREADS]; // one i32 per thread for JS Atomics
  BarrierSlot barrier_slots[MAX_BARRIERS];
  OsWasmEntity entities[OS_WASM_ENTITY_POOL_SIZE];
  OsWasmEntity *entity_free;
  u32 next_entity_idx;
  u32 next_barrier_slot;
  ThreadStackSlot stack_slots[MAX_THREADS];
  b32 stacks_initialized;
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

// Getter functions to export addresses to JS
WASM_EXPORT(get_thread_flags_ptr)
i32 *get_thread_flags_ptr(void) { return os_wasm_state.thread_flags; }

WASM_EXPORT(get_barrier_data_ptr)
BarrierSlot *get_barrier_data_ptr(void) { return os_wasm_state.barrier_slots; }

// JS imports
WASM_IMPORT(js_log) extern void js_log(const char *str, int len);
WASM_IMPORT(js_get_core_count) extern u32 js_get_core_count(void);
WASM_IMPORT(js_thread_spawn)
extern int js_thread_spawn(void *func, void *arg, u32 stack_top);
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

  entity->thread.stack_slot_idx = (u32)stack_slot;
  u32 stack_top = os_wasm_stack_get_top((u32)stack_slot);
  entity->thread.js_thread_id = js_thread_spawn((void *)fn, arg, stack_top);
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
  // Heap starts after reserved thread stack space
  u8 *base = &__heap_base + (MAX_THREADS * THREAD_STACK_SIZE);
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
