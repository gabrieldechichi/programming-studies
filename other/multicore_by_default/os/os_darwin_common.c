#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdio.h>

struct OsThread {
  pthread_t thread;
  OsThreadFunc func;
  void* arg;
  b32 joinable;
};

struct OsMutex {
  pthread_mutex_t mutex;
};

OsThread* os_thread_create(OsThreadFunc func, void* arg) {
  OsThread* thread = (OsThread*)malloc(sizeof(OsThread));
  if (!thread) return NULL;

  thread->func = func;
  thread->arg = arg;
  thread->joinable = true;

  if (pthread_create(&thread->thread, NULL, func, arg) != 0) {
    free(thread);
    return NULL;
  }

  return thread;
}

void os_thread_join(OsThread* thread) {
  if (thread && thread->joinable) {
    pthread_join(thread->thread, NULL);
    thread->joinable = false;
  }
}

void os_thread_destroy(OsThread* thread) {
  if (thread) {
    if (thread->joinable) {
      pthread_join(thread->thread, NULL);
    }
    free(thread);
  }
}

OsMutex* os_mutex_create(void) {
  OsMutex* mutex = (OsMutex*)malloc(sizeof(OsMutex));
  if (!mutex) return NULL;

  if (pthread_mutex_init(&mutex->mutex, NULL) != 0) {
    free(mutex);
    return NULL;
  }

  return mutex;
}

void os_mutex_lock(OsMutex* mutex) {
  if (mutex) {
    pthread_mutex_lock(&mutex->mutex);
  }
}

void os_mutex_unlock(OsMutex* mutex) {
  if (mutex) {
    pthread_mutex_unlock(&mutex->mutex);
  }
}

void os_mutex_destroy(OsMutex* mutex) {
  if (mutex) {
    pthread_mutex_destroy(&mutex->mutex);
    free(mutex);
  }
}

typedef struct {
  OsWorkQueueCallback callback;
  void *data;
} WorkEntry;

typedef struct OsWorkQueue {
  OsThread **threads;
  i32 thread_count;

  WorkEntry *entries;
  i32 entry_count;
  i32 entry_capacity;
  i32 next_entry_to_do;
  i32 completion_count;

  OsMutex *mutex;
  pthread_cond_t work_available;
  pthread_cond_t work_completed;
  b32 should_quit;
} OsWorkQueue;

static void *work_queue_thread_proc(void *arg) {
  OsWorkQueue *queue = (OsWorkQueue *)arg;

  while (true) {
    os_mutex_lock(queue->mutex);

    while (queue->next_entry_to_do >= queue->entry_count && !queue->should_quit) {
      pthread_cond_wait(&queue->work_available, &((OsMutex *)queue->mutex)->mutex);
    }

    if (queue->should_quit) {
      os_mutex_unlock(queue->mutex);
      break;
    }

    i32 entry_index = queue->next_entry_to_do++;
    WorkEntry entry = queue->entries[entry_index];

    os_mutex_unlock(queue->mutex);

    if (entry.callback) {
      entry.callback(entry.data);
    }

    os_mutex_lock(queue->mutex);
    queue->completion_count++;
    pthread_cond_broadcast(&queue->work_completed);
    os_mutex_unlock(queue->mutex);
  }

  return NULL;
}

OsWorkQueue *os_work_queue_create(i32 thread_count) {
  OsWorkQueue *queue = malloc(sizeof(OsWorkQueue));
  if (!queue) return NULL;

  memset(queue, 0, sizeof(OsWorkQueue));

  queue->thread_count = thread_count;
  queue->entry_capacity = 256;
  queue->entries = malloc(sizeof(WorkEntry) * queue->entry_capacity);
  queue->threads = malloc(sizeof(OsThread *) * thread_count);
  queue->mutex = os_mutex_create();

  pthread_cond_init(&queue->work_available, NULL);
  pthread_cond_init(&queue->work_completed, NULL);

  for (i32 i = 0; i < thread_count; i++) {
    queue->threads[i] = os_thread_create(work_queue_thread_proc, queue);
  }

  return queue;
}

void os_work_queue_destroy(OsWorkQueue *queue) {
  if (!queue) return;

  os_mutex_lock(queue->mutex);
  queue->should_quit = true;
  pthread_cond_broadcast(&queue->work_available);
  os_mutex_unlock(queue->mutex);

  for (i32 i = 0; i < queue->thread_count; i++) {
    os_thread_join(queue->threads[i]);
    os_thread_destroy(queue->threads[i]);
  }

  pthread_cond_destroy(&queue->work_available);
  pthread_cond_destroy(&queue->work_completed);

  os_mutex_destroy(queue->mutex);
  free(queue->entries);
  free(queue->threads);
  free(queue);
}

void os_add_work_entry(OsWorkQueue *queue, OsWorkQueueCallback callback, void *data) {
  os_mutex_lock(queue->mutex);

  if (queue->entry_count >= queue->entry_capacity) {
    queue->entry_capacity *= 2;
    queue->entries = realloc(queue->entries, sizeof(WorkEntry) * queue->entry_capacity);
  }

  WorkEntry *entry = &queue->entries[queue->entry_count++];
  entry->callback = callback;
  entry->data = data;

  pthread_cond_signal(&queue->work_available);
  os_mutex_unlock(queue->mutex);
}

void os_complete_all_work(OsWorkQueue *queue) {
  os_mutex_lock(queue->mutex);

  while (queue->completion_count < queue->entry_count) {
    pthread_cond_wait(&queue->work_completed, &((OsMutex *)queue->mutex)->mutex);
  }

  queue->entry_count = 0;
  queue->next_entry_to_do = 0;
  queue->completion_count = 0;

  os_mutex_unlock(queue->mutex);
}

i32 os_get_processor_count(void) {
  return (i32)sysconf(_SC_NPROCESSORS_ONLN);
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
  FILE* pipe = popen(command, "r");
  if (!pipe) return false;

  int status = pclose(pipe);
  return status == 0;
}
