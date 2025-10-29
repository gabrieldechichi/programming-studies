#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <execinfo.h>
#include <dlfcn.h>
#include <dispatch/dispatch.h>
#include <libgen.h>
#include <limits.h>

#include "os.h"

#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)

b32 os_is_mobile() { return false; }
void os_lock_mouse(b32 lock) { UNUSED(lock); }

OSThermalState os_get_thermal_state(void) { return OS_THERMAL_STATE_UNKNOWN; }

struct OsThread {
  pthread_t handle;
  OsThreadFunc func;
  void *arg;
};

struct OsMutex {
  pthread_mutex_t mutex;
};

static void *thread_wrapper(void *arg) {
  OsThread *thread = (OsThread *)arg;
  return thread->func(thread->arg);
}

OsThread *os_thread_create(OsThreadFunc func, void *arg) {
  OsThread *thread = (OsThread *)malloc(sizeof(OsThread));
  if (!thread)
    return NULL;

  thread->func = func;
  thread->arg = arg;

  if (pthread_create(&thread->handle, NULL, thread_wrapper, thread) != 0) {
    free(thread);
    return NULL;
  }

  return thread;
}

void os_thread_join(OsThread *thread) {
  if (thread) {
    pthread_join(thread->handle, NULL);
  }
}

void os_thread_destroy(OsThread *thread) {
  if (thread) {
    free(thread);
  }
}

OsMutex *os_mutex_create(void) {
  OsMutex *mutex = (OsMutex *)malloc(sizeof(OsMutex));
  if (!mutex)
    return NULL;

  if (pthread_mutex_init(&mutex->mutex, NULL) != 0) {
    free(mutex);
    return NULL;
  }
  return mutex;
}

void os_mutex_lock(OsMutex *mutex) {
  if (mutex) {
    pthread_mutex_lock(&mutex->mutex);
  }
}

void os_mutex_unlock(OsMutex *mutex) {
  if (mutex) {
    pthread_mutex_unlock(&mutex->mutex);
  }
}

void os_mutex_destroy(OsMutex *mutex) {
  if (mutex) {
    pthread_mutex_destroy(&mutex->mutex);
    free(mutex);
  }
}

#define WORK_QUEUE_ENTRIES_MAX 256

typedef struct {
  OsWorkQueueCallback callback;
  void *data;
} WorkQueueEntry;

struct OsWorkQueue {
  WorkQueueEntry entries[WORK_QUEUE_ENTRIES_MAX];

  volatile i32 next_entry_to_write;
  volatile i32 next_entry_to_read;

  volatile i32 completion_goal;
  volatile i32 completion_count;

  dispatch_semaphore_t semaphore;
  pthread_t *worker_threads;
  i32 thread_count;
  volatile i32 should_quit;
  pthread_mutex_t mutex;
};

static void *WorkerThreadProc(void *lpParam) {
  OsWorkQueue *queue = (OsWorkQueue *)lpParam;

  while (!queue->should_quit) {
    i32 original_next_read = queue->next_entry_to_read;
    i32 new_next_read = (original_next_read + 1) % WORK_QUEUE_ENTRIES_MAX;

    if (original_next_read != queue->next_entry_to_write) {
      if (__sync_bool_compare_and_swap(&queue->next_entry_to_read, original_next_read, new_next_read)) {
        WorkQueueEntry entry = queue->entries[original_next_read];
        entry.callback(entry.data);
        __sync_add_and_fetch(&queue->completion_count, 1);
      }
    } else {
      dispatch_semaphore_wait(queue->semaphore, DISPATCH_TIME_FOREVER);
    }
  }

  return NULL;
}

OsWorkQueue *os_work_queue_create(i32 thread_count) {
  OsWorkQueue *queue = (OsWorkQueue *)malloc(sizeof(OsWorkQueue));
  if (!queue) {
    return NULL;
  }

  memset(queue, 0, sizeof(OsWorkQueue));

  queue->thread_count = thread_count;
  queue->semaphore = dispatch_semaphore_create(0);
  if (!queue->semaphore) {
    free(queue);
    return NULL;
  }

  pthread_mutex_init(&queue->mutex, NULL);

  queue->worker_threads = (pthread_t *)malloc(sizeof(pthread_t) * thread_count);
  if (!queue->worker_threads) {
    dispatch_release(queue->semaphore);
    pthread_mutex_destroy(&queue->mutex);
    free(queue);
    return NULL;
  }

  for (i32 i = 0; i < thread_count; i++) {
    if (pthread_create(&queue->worker_threads[i], NULL, WorkerThreadProc, queue) != 0) {
      queue->should_quit = 1;
      for (i32 j = 0; j < i; j++) {
        dispatch_semaphore_signal(queue->semaphore);
      }
      for (i32 j = 0; j < i; j++) {
        pthread_join(queue->worker_threads[j], NULL);
      }
      free(queue->worker_threads);
      dispatch_release(queue->semaphore);
      pthread_mutex_destroy(&queue->mutex);
      free(queue);
      return NULL;
    }
  }

  return queue;
}

void os_work_queue_destroy(OsWorkQueue *queue) {
  if (!queue) {
    return;
  }

  queue->should_quit = 1;

  for (i32 i = 0; i < queue->thread_count; i++) {
    dispatch_semaphore_signal(queue->semaphore);
  }

  for (i32 i = 0; i < queue->thread_count; i++) {
    pthread_join(queue->worker_threads[i], NULL);
  }

  free(queue->worker_threads);
  dispatch_release(queue->semaphore);
  pthread_mutex_destroy(&queue->mutex);
  free(queue);
}

void os_add_work_entry(OsWorkQueue *queue, OsWorkQueueCallback callback, void *data) {
  i32 new_next_write = (queue->next_entry_to_write + 1) % WORK_QUEUE_ENTRIES_MAX;
  assert_msg(new_next_write != queue->next_entry_to_read, "Work queue is full!");

  WorkQueueEntry *entry = &queue->entries[queue->next_entry_to_write];
  entry->callback = callback;
  entry->data = data;

  __sync_add_and_fetch(&queue->completion_goal, 1);

  __sync_synchronize();

  queue->next_entry_to_write = new_next_write;

  dispatch_semaphore_signal(queue->semaphore);
}

void os_complete_all_work(OsWorkQueue *queue) {
  while (queue->completion_count != queue->completion_goal) {
    i32 original_next_read = queue->next_entry_to_read;
    i32 new_next_read = (original_next_read + 1) % WORK_QUEUE_ENTRIES_MAX;

    if (original_next_read != queue->next_entry_to_write) {
      if (__sync_bool_compare_and_swap(&queue->next_entry_to_read, original_next_read, new_next_read)) {
        WorkQueueEntry entry = queue->entries[original_next_read];
        entry.callback(entry.data);
        __sync_add_and_fetch(&queue->completion_count, 1);
      }
    }
  }

  queue->completion_goal = 0;
  queue->completion_count = 0;
}

#define MAX_STACK_FRAMES 50
#define CRASH_DUMP_DIR "crashes"

static OsMutex *g_stack_trace_mutex = NULL;

static void ensure_crash_dir_exists(void) {
  os_create_dir(CRASH_DUMP_DIR);
}

static void capture_and_save_stacktrace(FILE *output, int skip_frames) {
  if (!g_stack_trace_mutex) {
    g_stack_trace_mutex = os_mutex_create();
  }

  os_mutex_lock(g_stack_trace_mutex);

  void *stack_frames[MAX_STACK_FRAMES];
  int frame_count = backtrace(stack_frames, MAX_STACK_FRAMES);

  if (frame_count <= skip_frames) {
    os_mutex_unlock(g_stack_trace_mutex);
    return;
  }

  fprintf(output, "\n===== STACK TRACE =====\n");

  char **symbols = backtrace_symbols(stack_frames, frame_count);
  if (symbols) {
    for (int i = skip_frames; i < frame_count; i++) {
      fprintf(output, "  [%2d] %s\n", i - skip_frames, symbols[i]);
    }
    free(symbols);
  } else {
    for (int i = skip_frames; i < frame_count; i++) {
      fprintf(output, "  [%2d] %p\n", i - skip_frames, stack_frames[i]);
    }
  }

  fprintf(output, "=======================\n");
  fflush(output);

  ensure_crash_dir_exists();

  time_t now;
  time(&now);
  struct tm *timeinfo = localtime(&now);

  char crash_filename[256];
  snprintf(crash_filename, sizeof(crash_filename),
           "%s/crash_%04d%02d%02d_%02d%02d%02d.txt", CRASH_DUMP_DIR,
           timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
           timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

  FILE *crash_file = fopen(crash_filename, "w");
  if (crash_file) {
    fprintf(crash_file, "Crash dump generated at %s", asctime(timeinfo));

    frame_count = backtrace(stack_frames, MAX_STACK_FRAMES);
    symbols = backtrace_symbols(stack_frames, frame_count);
    if (symbols) {
      for (int i = skip_frames; i < frame_count; i++) {
        fprintf(crash_file, "  [%2d] %s\n", i - skip_frames, symbols[i]);
      }
      free(symbols);
    }

    fclose(crash_file);
    fprintf(output, "Stack trace saved to: %s\n", crash_filename);
  }

  os_mutex_unlock(g_stack_trace_mutex);
}

static void signal_handler(int sig) {
  const char *sig_name;
  switch (sig) {
    case SIGSEGV: sig_name = "SIGSEGV (Segmentation fault)"; break;
    case SIGBUS: sig_name = "SIGBUS (Bus error)"; break;
    case SIGABRT: sig_name = "SIGABRT (Abort)"; break;
    case SIGILL: sig_name = "SIGILL (Illegal instruction)"; break;
    case SIGFPE: sig_name = "SIGFPE (Floating point exception)"; break;
    default: sig_name = "Unknown signal"; break;
  }

  fprintf(stderr, "\n===== FATAL SIGNAL =====\n");
  fprintf(stderr, "Signal: %s (%d)\n", sig_name, sig);

  capture_and_save_stacktrace(stderr, 2);

  fprintf(stderr, "========================\n");

  signal(sig, SIG_DFL);
  raise(sig);
}

void os_install_crash_handler(void) {
  signal(SIGSEGV, signal_handler);
  signal(SIGBUS, signal_handler);
  signal(SIGABRT, signal_handler);
  signal(SIGILL, signal_handler);
  signal(SIGFPE, signal_handler);
}

void assert_log(u8 log_level, const char *fmt, const FmtArgs *args,
                const char *file_name, uint32 line_number) {
  os_log(log_level, fmt, args, file_name, line_number);
}

void os_log(LogLevel log_level, const char *fmt, const FmtArgs *args,
            const char *file_name, uint32 line_number) {
  char buffer[1024];
  fmt_string(buffer, sizeof(buffer), fmt, args);

  const char *level_str;
  const char *color_start = "";
  const char *color_end = "";
  FILE *output;

  b32 use_color = false;

  switch (log_level) {
  case LOGLEVEL_INFO:
    level_str = "INFO";
    output = stdout;
    use_color = isatty(fileno(stdout));
    break;
  case LOGLEVEL_WARN:
    level_str = "WARN";
    output = stderr;
    use_color = isatty(fileno(stderr));
    if (use_color) {
      color_start = "\033[33m";
      color_end = "\033[0m";
    }
    break;
  case LOGLEVEL_ERROR:
    level_str = "ERROR";
    output = stderr;
    use_color = isatty(fileno(stderr));
    if (use_color) {
      color_start = "\033[31m";
      color_end = "\033[0m";
    }
    break;
  default:
    level_str = "UNKNOWN";
    output = stderr;
    use_color = isatty(fileno(stderr));
    break;
  }

  fprintf(output, "%s[%s] %s:%u: %s%s\n", color_start, level_str, file_name,
          line_number, buffer, color_end);

  if (log_level == LOGLEVEL_ERROR) {
    capture_and_save_stacktrace(output, 2);
  }

  fflush(output);
}

bool32 os_write_file(const char *file_path, u8 *buffer, size_t buffer_len) {
  FILE *file = fopen(file_path, "wb");
  if (file == NULL) {
    LOG_ERROR("Error opening file for writing: %", FMT_STR(file_path));
    return false;
  }

  size_t written = fwrite(buffer, 1, buffer_len, file);
  if (written != buffer_len) {
    LOG_ERROR("Error writing to file: %", FMT_STR(file_path));
    fclose(file);
    return false;
  }

  fclose(file);
  return true;
}

bool32 os_create_dir(const char *dir_path) {
  struct stat st;
  if (stat(dir_path, &st) == 0 && S_ISDIR(st.st_mode)) {
    return true;
  }

  char temp_path[PATH_MAX];
  strncpy(temp_path, dir_path, sizeof(temp_path) - 1);
  temp_path[sizeof(temp_path) - 1] = '\0';

  for (char *p = temp_path + 1; *p; p++) {
    if (*p == '/') {
      *p = '\0';

      if (stat(temp_path, &st) != 0) {
        if (mkdir(temp_path, 0755) != 0 && errno != EEXIST) {
          LOG_ERROR("Failed to create directory: %", FMT_STR(temp_path));
          return false;
        }
      }

      *p = '/';
    }
  }

  if (mkdir(temp_path, 0755) != 0 && errno != EEXIST) {
    if (stat(temp_path, &st) == 0 && S_ISDIR(st.st_mode)) {
      return true;
    }
    LOG_ERROR("Failed to create directory: %", FMT_STR(dir_path));
    return false;
  }

  return true;
}

PlatformFileData os_read_file(const char *file_path, Allocator *allocator) {
  PlatformFileData result = {0};

  FILE *file = fopen(file_path, "rb");
  if (!file) {
    LOG_ERROR("Failed to open file: %", FMT_STR(file_path));
    return result;
  }

  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  fseek(file, 0, SEEK_SET);

  if (file_size < 0) {
    LOG_ERROR("Failed to get file size: %", FMT_STR(file_path));
    fclose(file);
    return result;
  }

  result.buffer = ALLOC_ARRAY(allocator, uint8, file_size);
  if (!result.buffer) {
    LOG_ERROR("Failed to allocate memory for file: %", FMT_STR(file_path));
    fclose(file);
    return result;
  }

  size_t bytes_read = fread(result.buffer, 1, file_size, file);
  fclose(file);

  if (bytes_read != (size_t)file_size) {
    LOG_ERROR("Failed to read file completely: %", FMT_STR(file_path));
    return result;
  }

  result.buffer_len = (uint32)file_size;
  result.success = true;
  return result;
}

OsFileReadOp os_start_read_file(const char *file_path) {
  UNUSED(file_path);
  assert_msg(false, "Async file read not supported on native platforms");
  return -1;
}

OsFileReadState os_check_read_file(OsFileReadOp op_id) {
  UNUSED(op_id);
  assert_msg(false, "Async file read not supported on native platforms");
  return OS_FILE_READ_STATE_ERROR;
}

int32 os_get_file_size(OsFileReadOp op_id) {
  UNUSED(op_id);
  assert_msg(false, "Async file read not supported on native platforms");
  return -1;
}

bool32 os_get_file_data(OsFileReadOp op_id, _out_ PlatformFileData *data,
                        Allocator *allocator) {
  UNUSED(op_id);
  UNUSED(data);
  UNUSED(allocator);
  assert_msg(false, "Async file read not supported on native platforms");
  return false;
}

OsDynLib os_dynlib_load(const char *path) {
  OsDynLib lib = dlopen(path, RTLD_NOW);
  if (!lib) {
    LOG_ERROR("os_dynlib_load failed: %", FMT_STR(dlerror()));
  }
  return lib;
}

void os_dynlib_unload(OsDynLib lib) {
  if (lib) {
    dlclose(lib);
  }
}

OsDynSymbol os_dynlib_get_symbol(OsDynLib lib, const char *symbol_name) {
  if (!lib)
    return NULL;
  return dlsym(lib, symbol_name);
}

OsFileInfo os_file_info(const char *path) {
  OsFileInfo info = {0};
  struct stat file_stat;
  if (stat(path, &file_stat) == 0) {
    info.modification_time = file_stat.st_mtime;
    info.exists = true;
  } else {
    info.exists = false;
  }
  return info;
}

b32 os_file_copy(const char *src_path, const char *dst_path) {
  FILE *src = fopen(src_path, "rb");
  if (!src) return false;

  FILE *dst = fopen(dst_path, "wb");
  if (!dst) {
    fclose(src);
    return false;
  }

  char buffer[4096];
  size_t bytes;
  while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
    if (fwrite(buffer, 1, bytes, dst) != bytes) {
      fclose(src);
      fclose(dst);
      return false;
    }
  }

  fclose(src);
  fclose(dst);
  return true;
}

b32 os_file_remove(const char *path) {
  return unlink(path) == 0;
}

b32 os_file_exists(const char *path) {
  struct stat st;
  return stat(path, &st) == 0;
}

static b32 copy_directory_recursive(const char *src_path, const char *dst_path) {
  if (!os_create_dir(dst_path)) {
    return false;
  }

  DIR *dir = opendir(src_path);
  if (!dir) {
    return false;
  }

  struct dirent *entry;
  b32 success = true;

  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    char src_full[PATH_MAX];
    char dst_full[PATH_MAX];
    snprintf(src_full, sizeof(src_full), "%s/%s", src_path, entry->d_name);
    snprintf(dst_full, sizeof(dst_full), "%s/%s", dst_path, entry->d_name);

    struct stat st;
    if (stat(src_full, &st) != 0) {
      success = false;
      break;
    }

    if (S_ISDIR(st.st_mode)) {
      if (!copy_directory_recursive(src_full, dst_full)) {
        success = false;
        break;
      }
    } else {
      if (!os_file_copy(src_full, dst_full)) {
        success = false;
        break;
      }
    }
  }

  closedir(dir);
  return success;
}

b32 os_directory_copy(const char *src_path, const char *dst_path) {
  return copy_directory_recursive(src_path, dst_path);
}

static b32 remove_directory_recursive(const char *path) {
  DIR *dir = opendir(path);
  if (!dir) {
    return false;
  }

  struct dirent *entry;
  b32 success = true;

  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

    struct stat st;
    if (stat(full_path, &st) != 0) {
      success = false;
      break;
    }

    if (S_ISDIR(st.st_mode)) {
      if (!remove_directory_recursive(full_path)) {
        success = false;
        break;
      }
    } else {
      if (unlink(full_path) != 0) {
        success = false;
        break;
      }
    }
  }

  closedir(dir);

  if (success) {
    return rmdir(path) == 0;
  }
  return false;
}

b32 os_directory_remove(const char *path) {
  return remove_directory_recursive(path);
}

b32 os_system(const char *command) {
  return system(command) == 0;
}

OsFileList os_list_files(const char *directory, const char *extension,
                         Allocator *allocator) {
  OsFileList result = {0};

  DIR *dir = opendir(directory);
  if (!dir) {
    return result;
  }

  int count = 0;
  int capacity = 256;
  char **paths = ALLOC_ARRAY(allocator, char *, capacity);

  struct dirent *entry;
  size_t ext_len = strlen(extension);

  while ((entry = readdir(dir)) != NULL && count < capacity) {
    size_t name_len = strlen(entry->d_name);

    if (entry->d_type == DT_REG && name_len >= ext_len) {
      if (strcmp(entry->d_name + name_len - ext_len, extension) == 0) {
        size_t path_len = strlen(directory) + name_len + 2;
        char *full_path = allocator->alloc_alloc(allocator->ctx, path_len, 1);
        if (full_path) {
          snprintf(full_path, path_len, "%s/%s", directory, entry->d_name);
          paths[count++] = full_path;
        }
      }
    }
  }

  closedir(dir);

  result.paths = paths;
  result.count = count;
  return result;
}

b32 os_file_set_executable(const char *path) {
  return chmod(path, 0755) == 0;
}

char *os_cwd(char *buffer, u32 buffer_size) {
  return getcwd(buffer, buffer_size);
}

i32 os_get_processor_count(void) {
  return (i32)sysconf(_SC_NPROCESSORS_ONLN);
}

u8 *os_allocate_memory(size_t size) {
  void *memory = mmap(NULL, size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (memory == MAP_FAILED) {
    LOG_ERROR("mmap failed. Size: %, Error: %", FMT_UINT(size), FMT_UINT(errno));
    return NULL;
  }
  return memory;
}

void os_free_memory(void *ptr, size_t size) {
  if (ptr) {
    if (munmap(ptr, size) != 0) {
      LOG_ERROR("munmap failed. Error: %", FMT_UINT(errno));
    }
  }
}

OsWebPLoadOp os_start_webp_texture_load(const char *file_path,
                                         u32 file_path_len,
                                         u32 texture_handle_idx,
                                         u32 texture_handle_gen) {
  UNUSED(file_path);
  UNUSED(file_path_len);
  UNUSED(texture_handle_idx);
  UNUSED(texture_handle_gen);
  return -1;
}

OsFileReadState os_check_webp_texture_load(OsWebPLoadOp op_id) {
  UNUSED(op_id);
  return OS_FILE_READ_STATE_ERROR;
}