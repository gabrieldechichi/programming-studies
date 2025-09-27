#define _POSIX_C_SOURCE 199309L
#define _DEFAULT_SOURCE

#include "lib/assert.h"
#include "lib/fmt.h"
#include "os.h"
#include "string.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

void platform_init(void) {
  // No initialization needed for Linux
}

/*!
 * Platform log
 * */
void platform_log(LogLevel log_level, const char *fmt, const FmtArgs *args,
                  const char *file_name, uint32 line_number) {
  char buffer[1024];
  fmt_string(buffer, sizeof(buffer), fmt, args);

  const char *level_str;
  FILE *output;
  switch (log_level) {
  case LOGLEVEL_INFO:
    level_str = "INFO";
    output = stdout;
    break;
  case LOGLEVEL_WARN:
    level_str = "WARN";
    output = stderr;
    break;
  case LOGLEVEL_ERROR:
    level_str = "ERROR";
    output = stderr;
    break;
  default:
    level_str = "UNKNOWN";
    output = stderr;
    break;
  }

  fprintf(output, "[%s] %s:%u: %s\n", level_str, file_name, line_number,
          buffer);

  fflush(output);
}

/*!
 * File IO
 * */
bool32 platform_write_file(const char *file_path, u8 *buffer, u32 buffer_len) {
  FILE *file = fopen(file_path, "wb");
  if (file == NULL) {
    LOG_ERROR("Error opening file for writing: %", FMT_STR(file_path));
    return false;
  }

  size_t written = fwrite(buffer, 1, buffer_len, file);
  if (written != buffer_len) {
    LOG_ERROR("Error writing to file: %", FMT_STR(file_path));
    fclose(file);
    return false; // or handle the error appropriately
  }

  fclose(file);
  return true;
}

bool32 platform_create_dir(const char *dir_path) {
#ifdef WIN64
  if (mkdir(dir_path) == 0) {
    return true;
  }
#else
  if (mkdir(dir_path, 0755) == 0) {
    return true;
  }
#endif

  struct stat st;
  if (stat(dir_path, &st) == 0 && S_ISDIR(st.st_mode)) {
    return true;
  }

  LOG_ERROR("Failed to create directory: %", FMT_STR(dir_path));
  return false;
}

/*!
 * File IO async
 */
#define MAX_FILE_OPS 64

typedef struct {
  PlatformReadFileState state;
  char *file_path;
  pthread_t thread;
  uint8 *buffer;
  uint32 buffer_len;
  bool32 thread_joinable;
} FileOperation;

static FileOperation g_file_ops[MAX_FILE_OPS];
static pthread_mutex_t g_file_ops_mutex = PTHREAD_MUTEX_INITIALIZER;
static int32 g_next_op_id = 1;

static void *file_read_thread(void *arg) {
  int32 op_id = (int32)(uintptr_t)arg;
  FileOperation *op = &g_file_ops[op_id];

  FILE *file = fopen(op->file_path, "rb");
  if (!file) {
    pthread_mutex_lock(&g_file_ops_mutex);
    op->state = FREADSTATE_ERROR;
    pthread_mutex_unlock(&g_file_ops_mutex);
    return NULL;
  }

  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  fseek(file, 0, SEEK_SET);

  if (file_size < 0) {
    fclose(file);
    pthread_mutex_lock(&g_file_ops_mutex);
    op->state = FREADSTATE_ERROR;
    pthread_mutex_unlock(&g_file_ops_mutex);
    return NULL;
  }

  uint8 *buffer = malloc(file_size);
  if (!buffer) {
    fclose(file);
    pthread_mutex_lock(&g_file_ops_mutex);
    op->state = FREADSTATE_ERROR;
    pthread_mutex_unlock(&g_file_ops_mutex);
    return NULL;
  }

  size_t bytes_read = fread(buffer, 1, file_size, file);
  fclose(file);

  pthread_mutex_lock(&g_file_ops_mutex);
  if (bytes_read == (size_t)file_size) {
    op->buffer = buffer;
    op->buffer_len = file_size;
    op->state = FREADSTATE_COMPLETED;
  } else {
    free(buffer);
    op->state = FREADSTATE_ERROR;
  }
  pthread_mutex_unlock(&g_file_ops_mutex);

  return NULL;
}

PlatformReadFileOp platform_start_read_file(char *file_name) {
  pthread_mutex_lock(&g_file_ops_mutex);

  int32 op_id = -1;
  for (int32 i = 0; i < MAX_FILE_OPS; i++) {
    if (g_file_ops[i].state == FREADSTATE_NONE) {
      op_id = i;
      break;
    }
  }

  if (op_id == -1) {
    pthread_mutex_unlock(&g_file_ops_mutex);
    LOG_ERROR("No available file operation slots%", FMT_STR(""));
    return -1;
  }

  FileOperation *op = &g_file_ops[op_id];
  op->state = FREADSTATE_IN_PROGRESS;

  size_t path_len = strlen(file_name);
  op->file_path = malloc(path_len + 1);
  if (!op->file_path) {
    op->state = FREADSTATE_NONE;
    pthread_mutex_unlock(&g_file_ops_mutex);
    LOG_ERROR("Failed to allocate memory for file path%", FMT_STR(""));
    return -1;
  }
  strcpy(op->file_path, file_name);

  op->buffer = NULL;
  op->buffer_len = 0;
  op->thread_joinable = 0;

  if (pthread_create(&op->thread, NULL, file_read_thread,
                     (void *)(uintptr_t)op_id) != 0) {
    free(op->file_path);
    op->state = FREADSTATE_NONE;
    pthread_mutex_unlock(&g_file_ops_mutex);
    LOG_ERROR("Failed to create file read thread%", FMT_STR(""));
    return -1;
  }

  op->thread_joinable = 1;
  pthread_mutex_unlock(&g_file_ops_mutex);

  return op_id;
}

PlatformReadFileState platform_check_read_file(PlatformReadFileOp op_id) {
  if (op_id < 0 || op_id >= MAX_FILE_OPS) {
    return FREADSTATE_ERROR;
  }

  pthread_mutex_lock(&g_file_ops_mutex);
  PlatformReadFileState state = g_file_ops[op_id].state;
  pthread_mutex_unlock(&g_file_ops_mutex);

  return state;
}

bool32 platform_get_file_data(PlatformReadFileOp op_id,
                              _out_ PlatformFileData *data,
                              Allocator *allocator) {
  if (op_id < 0 || op_id >= MAX_FILE_OPS) {
    return false;
  }

  pthread_mutex_lock(&g_file_ops_mutex);
  FileOperation *op = &g_file_ops[op_id];

  if (op->state != FREADSTATE_COMPLETED) {
    pthread_mutex_unlock(&g_file_ops_mutex);
    return false;
  }

  data->buffer_len = op->buffer_len;
  data->buffer = ALLOC_ARRAY(allocator, uint8, op->buffer_len);
  if (!data->buffer) {
    pthread_mutex_unlock(&g_file_ops_mutex);
    return false;
  }

  memcpy(data->buffer, op->buffer, op->buffer_len);

  if (op->thread_joinable) {
    pthread_join(op->thread, NULL);
    op->thread_joinable = 0;
  }

  free(op->buffer);
  free(op->file_path);
  op->state = FREADSTATE_NONE;
  op->buffer = NULL;
  op->buffer_len = 0;
  op->file_path = NULL;

  pthread_mutex_unlock(&g_file_ops_mutex);

  return true;
}

PlatformWebPLoadOp platform_start_webp_texture_load(const char *file_path,
                                                    u32 file_path_len,
                                                    Handle texture_handle) {
  UNUSED(file_path);
  UNUSED(file_path_len);
  UNUSED(texture_handle);
  assert_msg(false, "WebP textures are not supported on desktop%", FMT_STR(""));
  return -1;
}

PlatformReadFileState
platform_check_webp_texture_load(PlatformWebPLoadOp op_id) {
  UNUSED(op_id);
  assert_msg(false, "WebP textures are not supported on desktop%", FMT_STR(""));
  return FREADSTATE_ERROR;
}

/*!
 * Platform timing
 * */
u64 platform_time_now(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (u64)ts.tv_sec * 1000000000ULL + (u64)ts.tv_nsec;
}

u64 platform_time_diff(u64 new_ticks, u64 old_ticks) {
  return new_ticks - old_ticks;
}

f64 platform_ticks_to_ms(u64 ticks) {
  return (f64)ticks / 1000000.0; // Convert nanoseconds to milliseconds
}

f64 platform_ticks_to_us(u64 ticks) {
  return (f64)ticks / 1000.0; // Convert nanoseconds to microseconds
}

f64 platform_ticks_to_ns(u64 ticks) {
  return (f64)ticks; // Already in nanoseconds
}

/*OS Audio - MOCK implementation*/
void platform_sleep_us(u32 microseconds) { usleep(microseconds); }

void platform_audio_init(void) {}
void platform_audio_shutdown(void) {}
void platform_audio_update(void) {}
// void platform_audio_write_samples(f32 *samples, i32 sample_count) {
//   UNUSED(samples);
//   UNUSED(sample_count);
// }
// i32 platform_audio_get_sample_rate() { return 44100; }
/*END - OS Audio*/