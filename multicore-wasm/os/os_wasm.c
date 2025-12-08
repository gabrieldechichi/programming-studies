#include "lib/typedefs.h"
#include "lib/memory.h"

// Opaque handles (same as win32)
typedef struct {
  u64 v[1];
} Thread;
typedef struct {
  u64 v[1];
} Barrier;
typedef void (*ThreadFunc)(void *);

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

typedef struct {
  i32 thread_flags[MAX_THREADS]; // one i32 per thread for JS Atomics
  BarrierSlot barrier_slots[MAX_BARRIERS];
  OsWasmEntity entities[OS_WASM_ENTITY_POOL_SIZE];
  OsWasmEntity *entity_free;
  u32 next_entity_idx;
  u32 next_barrier_slot;
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

// Getter functions to export addresses to JS
WASM_EXPORT(get_thread_flags_ptr)
i32 *get_thread_flags_ptr(void) { return os_wasm_state.thread_flags; }

WASM_EXPORT(get_barrier_data_ptr)
BarrierSlot *get_barrier_data_ptr(void) { return os_wasm_state.barrier_slots; }

// JS imports
WASM_IMPORT(js_log) extern void js_log(const char *str, int len);
WASM_IMPORT(js_thread_spawn) extern int js_thread_spawn(void *func, void *arg);
WASM_IMPORT(js_thread_join) extern void js_thread_join(int thread_id);
WASM_IMPORT(js_barrier_wait) extern void js_barrier_wait(u32 barrier_id);

// Simple print - just logs the string, no format parsing
void print(const char *str) {
  int len = 0;
  while (str[len])
    len++;
  js_log(str, len);
}

// Threads
Thread os_thread_launch(ThreadFunc fn, void *arg) {
  Thread result = {0};
  OsWasmEntity *entity = os_wasm_entity_alloc(OS_WASM_ENTITY_THREAD);
  if (!entity)
    return result;

  entity->thread.js_thread_id = js_thread_spawn((void *)fn, arg);
  result.v[0] = (u64)entity;
  return result;
}

b32 os_thread_join(Thread t, u64 timeout_us) {
  (void)timeout_us;
  if (t.v[0] == 0)
    return 0;

  OsWasmEntity *entity = (OsWasmEntity *)t.v[0];
  js_thread_join(entity->thread.js_thread_id);
  os_wasm_entity_release(entity);
  return 1;
}

i32 os_get_processor_count(void) {
  return 1;
  // todo: js
}

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

extern unsigned char __heap_base;

WASM_EXPORT(os_get_heap_base) void *os_get_heap_base(void) {
#ifdef DEBUG
  // add 1MB padding on debug builds to support hot reload
  return (&__heap_base + KB(1024));
#else
  return &__heap_base;
#endif
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
