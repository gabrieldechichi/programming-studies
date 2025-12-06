#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

#include "lib/pthread_barrier.h"
#include "lib/thread.h"
#include "lib/task.h"

#ifndef internal
#define internal static
#endif

#ifdef IOS
extern const char* ios_get_bundle_resource_path(const char *relative_path);
#endif

typedef struct {
  pthread_t thread;
  ThreadFunc func;
  void *arg;
} OsDarwinThread;

typedef struct {
  pthread_mutex_t mutex;
} OsDarwinMutex;

typedef struct {
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  i32 count;
} OsDarwinSemaphore;

typedef struct {
  pthread_rwlock_t lock;
} OsDarwinRWMutex;

typedef struct {
  pthread_cond_t cond;
} OsDarwinCondVar;

typedef struct {
  pthread_barrier_t barrier;
} OsDarwinBarrier;

internal void *os_darwin_thread_wrapper(void *arg) {
  OsDarwinThread *thread = (OsDarwinThread *)arg;
  thread->func(thread->arg);
  return NULL;
}

Thread os_thread_launch(ThreadFunc func, void *arg) {
  Thread result = {0};
  OsDarwinThread *thread = (OsDarwinThread *)os_allocate_memory(sizeof(OsDarwinThread));
  if (!thread)
    return result;

  thread->func = func;
  thread->arg = arg;

  if (pthread_create(&thread->thread, NULL, os_darwin_thread_wrapper, thread) != 0) {
    os_free_memory(thread, sizeof(OsDarwinThread));
    return result;
  }

  result.v[0] = (u64)thread;
  return result;
}

b32 os_thread_join(Thread t, u64 timeout_us) {
  if (t.v[0] == 0)
    return false;
  OsDarwinThread *thread = (OsDarwinThread *)t.v[0];
  if (timeout_us == 0) {
    pthread_join(thread->thread, NULL);
    os_free_memory(thread, sizeof(OsDarwinThread));
    return true;
  }
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_sec += timeout_us / 1000000;
  ts.tv_nsec += (timeout_us % 1000000) * 1000;
  if (ts.tv_nsec >= 1000000000) {
    ts.tv_sec++;
    ts.tv_nsec -= 1000000000;
  }
#ifdef __APPLE__
  int result = pthread_join(thread->thread, NULL);
#else
  int result = pthread_timedjoin_np(thread->thread, NULL, &ts);
#endif
  if (result == 0) {
    os_free_memory(thread, sizeof(OsDarwinThread));
    return true;
  }
  return false;
}

void os_thread_detach(Thread t) {
  if (t.v[0] == 0)
    return;
  OsDarwinThread *thread = (OsDarwinThread *)t.v[0];
  pthread_detach(thread->thread);
  os_free_memory(thread, sizeof(OsDarwinThread));
}

Mutex os_mutex_alloc(void) {
  Mutex result = {0};
  OsDarwinMutex *mutex = (OsDarwinMutex *)os_allocate_memory(sizeof(OsDarwinMutex));
  if (!mutex)
    return result;

  if (pthread_mutex_init(&mutex->mutex, NULL) != 0) {
    os_free_memory(mutex, sizeof(OsDarwinMutex));
    return result;
  }

  result.v[0] = (u64)mutex;
  return result;
}

void os_mutex_release(Mutex m) {
  if (m.v[0] == 0)
    return;
  OsDarwinMutex *mutex = (OsDarwinMutex *)m.v[0];
  pthread_mutex_destroy(&mutex->mutex);
  os_free_memory(mutex, sizeof(OsDarwinMutex));
}

void os_mutex_take(Mutex m) {
  if (m.v[0] == 0)
    return;
  OsDarwinMutex *mutex = (OsDarwinMutex *)m.v[0];
  pthread_mutex_lock(&mutex->mutex);
}

void os_mutex_drop(Mutex m) {
  if (m.v[0] == 0)
    return;
  OsDarwinMutex *mutex = (OsDarwinMutex *)m.v[0];
  pthread_mutex_unlock(&mutex->mutex);
}

Semaphore os_semaphore_alloc(i32 initial_count) {
  Semaphore result = {0};
  OsDarwinSemaphore *sem = (OsDarwinSemaphore *)os_allocate_memory(sizeof(OsDarwinSemaphore));
  if (!sem)
    return result;

  if (pthread_mutex_init(&sem->mutex, NULL) != 0) {
    os_free_memory(sem, sizeof(OsDarwinSemaphore));
    return result;
  }
  if (pthread_cond_init(&sem->cond, NULL) != 0) {
    pthread_mutex_destroy(&sem->mutex);
    os_free_memory(sem, sizeof(OsDarwinSemaphore));
    return result;
  }
  sem->count = initial_count;
  result.v[0] = (u64)sem;
  return result;
}

void os_semaphore_release(Semaphore s) {
  if (s.v[0] == 0)
    return;
  OsDarwinSemaphore *sem = (OsDarwinSemaphore *)s.v[0];
  pthread_mutex_destroy(&sem->mutex);
  pthread_cond_destroy(&sem->cond);
  os_free_memory(sem, sizeof(OsDarwinSemaphore));
}

void os_semaphore_take(Semaphore s) {
  if (s.v[0] == 0)
    return;
  OsDarwinSemaphore *sem = (OsDarwinSemaphore *)s.v[0];
  pthread_mutex_lock(&sem->mutex);
  while (sem->count <= 0) {
    pthread_cond_wait(&sem->cond, &sem->mutex);
  }
  sem->count--;
  pthread_mutex_unlock(&sem->mutex);
}

void os_semaphore_drop(Semaphore s) {
  if (s.v[0] == 0)
    return;
  OsDarwinSemaphore *sem = (OsDarwinSemaphore *)s.v[0];
  pthread_mutex_lock(&sem->mutex);
  sem->count++;
  pthread_mutex_unlock(&sem->mutex);
  pthread_cond_signal(&sem->cond);
}

RWMutex os_rw_mutex_alloc(void) {
  RWMutex result = {0};
  OsDarwinRWMutex *rw = (OsDarwinRWMutex *)os_allocate_memory(sizeof(OsDarwinRWMutex));
  if (!rw)
    return result;

  if (pthread_rwlock_init(&rw->lock, NULL) != 0) {
    os_free_memory(rw, sizeof(OsDarwinRWMutex));
    return result;
  }

  result.v[0] = (u64)rw;
  return result;
}

void os_rw_mutex_release(RWMutex m) {
  if (m.v[0] == 0)
    return;
  OsDarwinRWMutex *rw = (OsDarwinRWMutex *)m.v[0];
  pthread_rwlock_destroy(&rw->lock);
  os_free_memory(rw, sizeof(OsDarwinRWMutex));
}

void os_rw_mutex_take_r(RWMutex m) {
  if (m.v[0] == 0)
    return;
  OsDarwinRWMutex *rw = (OsDarwinRWMutex *)m.v[0];
  pthread_rwlock_rdlock(&rw->lock);
}

void os_rw_mutex_drop_r(RWMutex m) {
  if (m.v[0] == 0)
    return;
  OsDarwinRWMutex *rw = (OsDarwinRWMutex *)m.v[0];
  pthread_rwlock_unlock(&rw->lock);
}

void os_rw_mutex_take_w(RWMutex m) {
  if (m.v[0] == 0)
    return;
  OsDarwinRWMutex *rw = (OsDarwinRWMutex *)m.v[0];
  pthread_rwlock_wrlock(&rw->lock);
}

void os_rw_mutex_drop_w(RWMutex m) {
  if (m.v[0] == 0)
    return;
  OsDarwinRWMutex *rw = (OsDarwinRWMutex *)m.v[0];
  pthread_rwlock_unlock(&rw->lock);
}

CondVar os_cond_var_alloc(void) {
  CondVar result = {0};
  OsDarwinCondVar *cv = (OsDarwinCondVar *)os_allocate_memory(sizeof(OsDarwinCondVar));
  if (!cv)
    return result;

  if (pthread_cond_init(&cv->cond, NULL) != 0) {
    os_free_memory(cv, sizeof(OsDarwinCondVar));
    return result;
  }

  result.v[0] = (u64)cv;
  return result;
}

void os_cond_var_release(CondVar c) {
  if (c.v[0] == 0)
    return;
  OsDarwinCondVar *cv = (OsDarwinCondVar *)c.v[0];
  pthread_cond_destroy(&cv->cond);
  os_free_memory(cv, sizeof(OsDarwinCondVar));
}

b32 os_cond_var_wait(CondVar c, Mutex m, u64 timeout_us) {
  if (c.v[0] == 0 || m.v[0] == 0)
    return false;
  OsDarwinCondVar *cv = (OsDarwinCondVar *)c.v[0];
  OsDarwinMutex *mutex = (OsDarwinMutex *)m.v[0];
  if (timeout_us == 0) {
    return pthread_cond_wait(&cv->cond, &mutex->mutex) == 0;
  }
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_sec += timeout_us / 1000000;
  ts.tv_nsec += (timeout_us % 1000000) * 1000;
  if (ts.tv_nsec >= 1000000000) {
    ts.tv_sec++;
    ts.tv_nsec -= 1000000000;
  }
  return pthread_cond_timedwait(&cv->cond, &mutex->mutex, &ts) == 0;
}

void os_cond_var_signal(CondVar c) {
  if (c.v[0] == 0)
    return;
  OsDarwinCondVar *cv = (OsDarwinCondVar *)c.v[0];
  pthread_cond_signal(&cv->cond);
}

void os_cond_var_broadcast(CondVar c) {
  if (c.v[0] == 0)
    return;
  OsDarwinCondVar *cv = (OsDarwinCondVar *)c.v[0];
  pthread_cond_broadcast(&cv->cond);
}

Barrier os_barrier_alloc(u32 count) {
  Barrier result = {0};
  if (count == 0)
    return result;
  OsDarwinBarrier *barrier = (OsDarwinBarrier *)os_allocate_memory(sizeof(OsDarwinBarrier));
  if (!barrier)
    return result;

  if (pthread_barrier_init(&barrier->barrier, NULL, count) != 0) {
    os_free_memory(barrier, sizeof(OsDarwinBarrier));
    return result;
  }

  result.v[0] = (u64)barrier;
  return result;
}

void os_barrier_release(Barrier b) {
  if (b.v[0] == 0)
    return;
  OsDarwinBarrier *barrier = (OsDarwinBarrier *)b.v[0];
  pthread_barrier_destroy(&barrier->barrier);
  os_free_memory(barrier, sizeof(OsDarwinBarrier));
}

void os_barrier_wait(Barrier b) {
  if (b.v[0] == 0)
    return;
  OsDarwinBarrier *barrier = (OsDarwinBarrier *)b.v[0];
  pthread_barrier_wait(&barrier->barrier);
}

i32 os_get_processor_count(void) {
  return (i32)sysconf(_SC_NPROCESSORS_ONLN);
}

void os_sleep(u64 microseconds) {
  usleep((useconds_t)microseconds);
}

u8 *os_allocate_memory(size_t size) {
  void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (ptr == MAP_FAILED) {
    return NULL;
  }
  return (u8 *)ptr;
}

void os_free_memory(void *ptr, size_t size) {
  if (ptr) {
    munmap(ptr, size);
  }
}

b32 os_file_copy(const char* src_path, const char* dst_path) {
  FILE *src_file = fopen(src_path, "rb");
  if (!src_file) return false;

  FILE *dst_file = fopen(dst_path, "wb");
  if (!dst_file) {
    fclose(src_file);
    return false;
  }

  char buffer[4096];
  size_t bytes;
  while ((bytes = fread(buffer, 1, sizeof(buffer), src_file)) > 0) {
    if (fwrite(buffer, 1, bytes, dst_file) != bytes) {
      fclose(src_file);
      fclose(dst_file);
      return false;
    }
  }

  fclose(src_file);
  fclose(dst_file);
  return true;
}

b32 os_file_remove(const char* path) {
  return remove(path) == 0;
}

b32 os_file_set_executable(const char* path) {
  return chmod(path, 0755) == 0;
}

char* os_cwd(char* buffer, u32 buffer_size) {
  return getcwd(buffer, buffer_size);
}

b32 os_system(const char* command) {
#if defined(MACOS) || defined(BUILD_SYSTEM)
  return system(command) == 0;
#else
  return false;
#endif
}

b32 os_symlink(const char *target_path, const char *link_path) {
  unlink(link_path);
  return symlink(target_path, link_path) == 0;
}

typedef struct {
  volatile OsFileReadState state;
  char *file_path;
  u8 *buffer;
  u32 buffer_len;
  b32 in_use;
} FileReadOp;

#define MAX_FILE_OPS 64
static FileReadOp g_file_ops[MAX_FILE_OPS];
static Mutex g_file_ops_mutex = {0};
static b32 g_file_ops_initialized = false;

internal void file_ops_init(void) {
  if (g_file_ops_initialized) return;

  g_file_ops_mutex = os_mutex_alloc();
  for (i32 i = 0; i < MAX_FILE_OPS; i++) {
    g_file_ops[i].in_use = false;
    g_file_ops[i].state = OS_FILE_READ_STATE_NONE;
  }
  g_file_ops_initialized = true;
}

internal i32 file_ops_allocate(void) {
  os_mutex_take(g_file_ops_mutex);
  for (i32 i = 0; i < MAX_FILE_OPS; i++) {
    if (!g_file_ops[i].in_use) {
      g_file_ops[i].in_use = true;
      g_file_ops[i].state = OS_FILE_READ_STATE_IN_PROGRESS;
      g_file_ops[i].buffer = NULL;
      g_file_ops[i].buffer_len = 0;
      g_file_ops[i].file_path = NULL;
      os_mutex_drop(g_file_ops_mutex);
      return i;
    }
  }
  os_mutex_drop(g_file_ops_mutex);
  return -1;
}

internal void file_read_worker(void *data) {
  i32 op_id = (i32)(intptr_t)data;
  FileReadOp *op = &g_file_ops[op_id];

#ifdef IOS
  const char *file_path = ios_get_bundle_resource_path(op->file_path);
#else
  const char *file_path = op->file_path;
#endif

  FILE *file = fopen(file_path, "rb");
  if (!file) {
    ins_atomic_store_release(&op->state, OS_FILE_READ_STATE_ERROR);
    return;
  }

  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  fseek(file, 0, SEEK_SET);

  if (file_size < 0) {
    fclose(file);
    ins_atomic_store_release(&op->state, OS_FILE_READ_STATE_ERROR);
    return;
  }

  u8 *buffer = (u8 *)malloc(file_size);
  if (!buffer) {
    fclose(file);
    ins_atomic_store_release(&op->state, OS_FILE_READ_STATE_ERROR);
    return;
  }

  size_t bytes_read = fread(buffer, 1, file_size, file);
  fclose(file);

  if (bytes_read == (size_t)file_size) {
    op->buffer = buffer;
    op->buffer_len = (u32)file_size;
    ins_atomic_store_release(&op->state, OS_FILE_READ_STATE_COMPLETED);
  } else {
    free(buffer);
    ins_atomic_store_release(&op->state, OS_FILE_READ_STATE_ERROR);
  }
}

OsFileReadOp os_start_read_file(const char *file_path, TaskSystem *task_system) {
  if (!g_file_ops_initialized) {
    file_ops_init();
  }

  i32 op_id = file_ops_allocate();
  if (op_id == -1) {
    return -1;
  }

  FileReadOp *op = &g_file_ops[op_id];

  size_t path_len = strlen(file_path) + 1;
  op->file_path = (char *)malloc(path_len);
  if (!op->file_path) {
    os_mutex_take(g_file_ops_mutex);
    op->in_use = false;
    os_mutex_drop(g_file_ops_mutex);
    return -1;
  }
  memcpy(op->file_path, file_path, path_len);

  if (task_system) {
    task_schedule(task_system, file_read_worker, (void *)(intptr_t)op_id);
  } else {
    ins_atomic_store_release(&op->state, OS_FILE_READ_STATE_ERROR);
    return -1;
  }

  return op_id;
}

OsFileReadState os_check_read_file(OsFileReadOp op_id) {
  if (op_id < 0 || op_id >= MAX_FILE_OPS) {
    return OS_FILE_READ_STATE_ERROR;
  }

  FileReadOp *op = &g_file_ops[op_id];
  return ins_atomic_load_acquire(&op->state);
}

i32 os_get_file_size(OsFileReadOp op_id) {
  if (op_id < 0 || op_id >= MAX_FILE_OPS) {
    return -1;
  }

  FileReadOp *op = &g_file_ops[op_id];
  OsFileReadState state = ins_atomic_load_acquire(&op->state);
  return (state == OS_FILE_READ_STATE_COMPLETED) ? (i32)op->buffer_len : -1;
}

b32 os_get_file_data(OsFileReadOp op_id, _out_ PlatformFileData *data, Allocator *allocator) {
  if (op_id < 0 || op_id >= MAX_FILE_OPS) {
    return false;
  }

  FileReadOp *op = &g_file_ops[op_id];
  OsFileReadState state = ins_atomic_load_acquire(&op->state);

  if (state != OS_FILE_READ_STATE_COMPLETED || !op->buffer) {
    return false;
  }

  data->buffer_len = op->buffer_len;
  data->buffer = ALLOC_ARRAY(allocator, u8, op->buffer_len);
  if (!data->buffer) {
    return false;
  }

  memcpy(data->buffer, op->buffer, op->buffer_len);
  data->success = true;

  free(op->buffer);
  free(op->file_path);
  op->buffer = NULL;
  op->file_path = NULL;
  ins_atomic_store_release(&op->state, OS_FILE_READ_STATE_NONE);

  os_mutex_take(g_file_ops_mutex);
  op->in_use = false;
  os_mutex_drop(g_file_ops_mutex);

  return true;
}
