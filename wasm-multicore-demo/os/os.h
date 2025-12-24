#ifndef OS_H
#define OS_H

#include "lib/typedefs.h"
#include "lib/fmt.h"
#include "lib/thread.h"
#include "lib/memory.h"

b32 os_is_mobile(void);

typedef enum {
  OS_THERMAL_STATE_UNKNOWN = 0,
  OS_THERMAL_STATE_NOMINAL,
  OS_THERMAL_STATE_FAIR,
  OS_THERMAL_STATE_SERIOUS,
  OS_THERMAL_STATE_CRITICAL
} OSThermalState;

OSThermalState os_get_thermal_state(void);

typedef enum { LOGLEVEL_INFO, LOGLEVEL_WARN, LOGLEVEL_ERROR } LogLevel;
void os_log(LogLevel log_level, const char *fmt,
            const FmtArgs *args, const char *file_name,
            uint32 line_number);

#define PLATFORM_LOG(level, fmt, ...)                                          \
  do {                                                                         \
    FmtArg args[] = {(FmtArg){.type = 0}, ##__VA_ARGS__};                      \
    size_t _count = (sizeof(args) / sizeof(FmtArg)) - 1;                       \
    FmtArgs fmtArgs = {args + 1, (u8)_count};                                  \
    os_log(level, fmt, &fmtArgs, __FILE_NAME__, __LINE__);                     \
  } while (0)

#define LOG_INFO(fmt, ...) PLATFORM_LOG(LOGLEVEL_INFO, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) PLATFORM_LOG(LOGLEVEL_WARN, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) PLATFORM_LOG(LOGLEVEL_ERROR, fmt, ##__VA_ARGS__)

bool32 os_write_file(const char *file_path, u8 *buffer, size_t buffer_len);
bool32 os_create_dir(const char *dir_path);

typedef struct {
  uint32 buffer_len;
  uint8 *buffer;
  bool32 success;
} PlatformFileData;

PlatformFileData os_read_file(const char *file_path, Allocator *allocator);

typedef enum {
  OS_FILE_READ_STATE_NONE = 0,
  OS_FILE_READ_STATE_IN_PROGRESS = 1,
  OS_FILE_READ_STATE_COMPLETED = 2,
  OS_FILE_READ_STATE_ERROR = 3
} OsFileReadState;

typedef struct OsFileOp OsFileOp;

typedef struct {
  f32 x;
  f32 y;
  f32 width;
  f32 height;
} OsKeyboardRect;

OsKeyboardRect os_get_keyboard_rect(f32 time);

typedef struct {
  f32 top;
  f32 left;
  f32 bottom;
  f32 right;
} OsSafeAreaInsets;

OsSafeAreaInsets os_get_safe_area(void);

typedef enum {
  HTTP_METHOD_GET = 0,
  HTTP_METHOD_POST = 1,
  HTTP_METHOD_PUT = 2,
  HTTP_METHOD_DELETE = 3
} HttpMethod;

typedef enum {
  HTTP_OP_NONE = 0,
  HTTP_OP_IN_PROGRESS = 1,
  HTTP_OP_COMPLETED = 2,
  HTTP_OP_ERROR = 3
} HttpOpState;

typedef enum {
  HTTP_STREAM_NOT_STARTED = 0,
  HTTP_STREAM_READY = 1,
  HTTP_STREAM_HAS_CHUNK = 2,
  HTTP_STREAM_COMPLETE = 3,
  HTTP_STREAM_ERROR = 4
} HttpStreamState;

typedef int32 PlatformHttpRequestOp;
typedef int32 PlatformHttpStreamOp;

PlatformHttpRequestOp os_start_http_request(HttpMethod method, const char *url,
                                            int url_len, const char *headers,
                                            int headers_len, const char *body,
                                            int body_len);
HttpOpState os_check_http_request(PlatformHttpRequestOp op_id);
int32 os_get_http_response_info(PlatformHttpRequestOp op_id,
                                _out_ int32 *status_code,
                                _out_ int32 *headers_len,
                                _out_ int32 *body_len);
int32 os_get_http_body(PlatformHttpRequestOp op_id, char *buffer,
                       int32 buffer_len);

PlatformHttpStreamOp os_start_http_stream(HttpMethod method, const char *url,
                                          int url_len, const char *headers,
                                          int headers_len, const char *body,
                                          int body_len);
HttpStreamState os_check_http_stream(PlatformHttpStreamOp op_id);
int32 os_get_http_stream_info(PlatformHttpStreamOp op_id,
                              _out_ int32 *status_code);
int32 os_get_http_stream_chunk_size(PlatformHttpStreamOp op_id);
int32 os_get_http_stream_chunk(PlatformHttpStreamOp op_id, char *buffer,
                               int32 buffer_len, _out_ bool32 *is_final);

void os_time_init(void);
u64 os_time_now(void);
u64 os_time_diff(u64 new_ticks, u64 old_ticks);
f64 os_ticks_to_ms(u64 ticks);
f64 os_ticks_to_us(u64 ticks);
f64 os_ticks_to_ns(u64 ticks);
void os_sleep(u64 microseconds);

void os_init(void);

Thread os_thread_launch(ThreadFunc func, void *arg);
b32 os_thread_join(Thread t, u64 timeout_us);
void os_thread_detach(Thread t);
void os_thread_set_name(Thread t, const char *name);

Mutex os_mutex_alloc(void);
void os_mutex_release(Mutex m);
void os_mutex_take(Mutex m);
void os_mutex_drop(Mutex m);

Semaphore os_semaphore_alloc(i32 initial_count);
void os_semaphore_release(Semaphore s);
void os_semaphore_take(Semaphore s);
void os_semaphore_drop(Semaphore s);

RWMutex os_rw_mutex_alloc(void);
void os_rw_mutex_release(RWMutex m);
void os_rw_mutex_take_r(RWMutex m);
void os_rw_mutex_drop_r(RWMutex m);
void os_rw_mutex_take_w(RWMutex m);
void os_rw_mutex_drop_w(RWMutex m);

CondVar os_cond_var_alloc(void);
void os_cond_var_release(CondVar cv);
b32 os_cond_var_wait(CondVar cv, Mutex m, u64 timeout_us);
void os_cond_var_signal(CondVar cv);
void os_cond_var_broadcast(CondVar cv);

Barrier os_barrier_alloc(u32 count);
void os_barrier_release(Barrier b);
void os_barrier_wait(Barrier b);

typedef struct TaskSystem TaskSystem;

OsFileOp *os_start_read_file(const char *file_path, TaskSystem *task_system);
OsFileReadState os_check_read_file(OsFileOp *op);
i32 os_get_file_size(OsFileOp *op);
b32 os_get_file_data(OsFileOp *op, _out_ PlatformFileData *data,
                     Allocator *allocator);

typedef void *OsDynLib;
typedef void *OsDynSymbol;

OsDynLib os_dynlib_load(const char *path);
void os_dynlib_unload(OsDynLib lib);
OsDynSymbol os_dynlib_get_symbol(OsDynLib lib, const char *symbol_name);

typedef struct {
  i64 modification_time;
  b32 exists;
} OsFileInfo;

OsFileInfo os_file_info(const char *path);
b32 os_file_copy(const char *src_path, const char *dst_path);
b32 os_file_remove(const char *path);
b32 os_file_exists(const char *path);
b32 os_directory_copy(const char *src_path, const char *dst_path);
b32 os_directory_remove(const char *path);
b32 os_symlink(const char *target_path, const char *link_path);
b32 os_symlink_remove(const char *link_path);
b32 os_system(const char *command);

typedef struct {
  char **paths;
  i32 count;
} OsFileList;

OsFileList os_list_files(const char *directory, const char *extension,
                         Allocator *allocator);
OsFileList os_list_dirs(const char *directory, Allocator *allocator);
b32 os_file_set_executable(const char *path);
char *os_cwd(char *buffer, u32 buffer_size);

i32 os_get_processor_count(void);

void os_install_crash_handler(void);

u8 *os_allocate_memory(size_t size);
void os_free_memory(void *ptr, size_t size);
u8 *os_reserve_memory(size_t size);
b32 os_commit_memory(void *ptr, size_t size);
u32 os_get_page_size(void);

const char *os_get_compressed_texture_format_suffix(void);

u32 os_mic_get_available_samples(void);
u32 os_mic_read_samples(i16 *buffer, u32 max_samples);
void os_mic_start_recording(void);
void os_mic_stop_recording(void);
u32 os_mic_get_sample_rate(void);

#endif
