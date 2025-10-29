#ifndef H_PLATFORM
#define H_PLATFORM
#include "lib/fmt.h"
#include "lib/memory.h"

b32 os_is_mobile();

typedef enum {
  OS_THERMAL_STATE_UNKNOWN = 0,
  OS_THERMAL_STATE_NOMINAL,
  OS_THERMAL_STATE_FAIR,
  OS_THERMAL_STATE_SERIOUS,
  OS_THERMAL_STATE_CRITICAL
} OSThermalState;

HZ_ENGINE_API OSThermalState os_get_thermal_state(void);

typedef enum { LOGLEVEL_INFO, LOGLEVEL_WARN, LOGLEVEL_ERROR } LogLevel;
HZ_ENGINE_API void os_log(LogLevel log_level, const char *fmt,
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

// Definition moved to os_desktop.c to avoid duplicate symbols

/*!
 * File IO
 * */

HZ_ENGINE_API bool32 os_write_file(const char *file_path, u8 *buffer,
                                   size_t buffer_len);
HZ_ENGINE_API bool32 os_create_dir(const char *dir_path);

typedef struct {
  uint32 buffer_len;
  uint8 *buffer;
  b32 success;
} PlatformFileData;

HZ_ENGINE_API PlatformFileData os_read_file(const char *file_path,
                                            Allocator *allocator);

typedef enum {
  OS_FILE_READ_STATE_NONE = 0,
  OS_FILE_READ_STATE_IN_PROGRESS = 1,
  OS_FILE_READ_STATE_COMPLETED = 2,
  OS_FILE_READ_STATE_ERROR = 3
} OsFileReadState;

typedef int32 OsFileReadOp;

HZ_ENGINE_API OsFileReadOp os_start_read_file(const char *file_path);
HZ_ENGINE_API OsFileReadState os_check_read_file(OsFileReadOp op_id);
HZ_ENGINE_API int32 os_get_file_size(OsFileReadOp op_id);
HZ_ENGINE_API bool32 os_get_file_data(OsFileReadOp op_id,
                                       _out_ PlatformFileData *data,
                                       Allocator *allocator);

typedef int32 OsWebPLoadOp;

HZ_ENGINE_API OsWebPLoadOp os_start_webp_texture_load(const char *file_path,
                                                       u32 file_path_len,
                                                       u32 texture_handle_idx,
                                                       u32 texture_handle_gen);
HZ_ENGINE_API OsFileReadState os_check_webp_texture_load(OsWebPLoadOp op_id);

HZ_ENGINE_API void os_lock_mouse(b32 lock);

/*!
 * HTTP Requests
 * */
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
  HTTP_SUCCESS = 0,
  HTTP_NETWORK_ERROR = 1,
  HTTP_TIMEOUT = 2,
  HTTP_PARSE_ERROR = 3
} HttpResultCode;

typedef int32 PlatformHttpRequestOp;

HZ_ENGINE_API PlatformHttpRequestOp os_start_http_request(
    HttpMethod method, const char *url, int url_len, const char *headers,
    int headers_len, const char *body, int body_len);

HZ_ENGINE_API HttpOpState os_check_http_request(PlatformHttpRequestOp op_id);

HZ_ENGINE_API int32 os_get_http_response_info(PlatformHttpRequestOp op_id,
                                              _out_ int32 *status_code,
                                              _out_ int32 *headers_len,
                                              _out_ int32 *body_len);

HZ_ENGINE_API int32 os_get_http_headers(PlatformHttpRequestOp op_id,
                                        char *buffer, int32 buffer_len);

HZ_ENGINE_API int32 os_get_http_body(PlatformHttpRequestOp op_id, char *buffer,
                                     int32 buffer_len);

/*!
 * HTTP Streaming
 * */
typedef enum {
  HTTP_STREAM_NOT_STARTED = 0,
  HTTP_STREAM_READY = 1,
  HTTP_STREAM_HAS_CHUNK = 2,
  HTTP_STREAM_COMPLETE = 3,
  HTTP_STREAM_ERROR = 4
} HttpStreamState;

typedef int32 PlatformHttpStreamOp;

HZ_ENGINE_API PlatformHttpStreamOp os_start_http_stream(
    HttpMethod method, const char *url, int url_len, const char *headers,
    int headers_len, const char *body, int body_len);

HZ_ENGINE_API HttpStreamState os_check_http_stream(PlatformHttpStreamOp op_id);

HZ_ENGINE_API int32 os_get_http_stream_info(PlatformHttpStreamOp op_id,
                                            _out_ int32 *status_code);

HZ_ENGINE_API int32 os_get_http_stream_chunk_size(PlatformHttpStreamOp op_id);

HZ_ENGINE_API int32 os_get_http_stream_chunk(PlatformHttpStreamOp op_id,
                                             char *buffer, int32 buffer_len,
                                             _out_ bool32 *is_final);

HZ_ENGINE_API void os_quit(void);

/*!
 * Timing
 * */
HZ_ENGINE_API void os_time_init(void);
HZ_ENGINE_API u64 os_time_now(void);
HZ_ENGINE_API u64 os_time_diff(u64 new_ticks, u64 old_ticks);
HZ_ENGINE_API f64 os_ticks_to_ms(u64 ticks);
HZ_ENGINE_API f64 os_ticks_to_us(u64 ticks);
HZ_ENGINE_API f64 os_ticks_to_ns(u64 ticks);

/*!
 * Audio
 * */
HZ_ENGINE_API void os_audio_init();
HZ_ENGINE_API void os_audio_shutdown(void);
HZ_ENGINE_API void os_audio_update(void);
HZ_ENGINE_API void os_audio_write_samples(f32 *samples, i32 sample_count);
HZ_ENGINE_API i32 os_audio_get_sample_rate();
HZ_ENGINE_API u32 os_audio_get_samples_needed();

/*!
 * Threading
 * */
typedef struct OsThread OsThread;
typedef struct OsMutex OsMutex;
typedef void *(*OsThreadFunc)(void *arg);

HZ_ENGINE_API OsThread *os_thread_create(OsThreadFunc func, void *arg);
HZ_ENGINE_API void os_thread_join(OsThread *thread);
HZ_ENGINE_API void os_thread_destroy(OsThread *thread);

HZ_ENGINE_API OsMutex *os_mutex_create(void);
HZ_ENGINE_API void os_mutex_lock(OsMutex *mutex);
HZ_ENGINE_API void os_mutex_unlock(OsMutex *mutex);
HZ_ENGINE_API void os_mutex_destroy(OsMutex *mutex);

/*!
 * Worker Queue System
 * */
typedef struct OsWorkQueue OsWorkQueue;
typedef void (*OsWorkQueueCallback)(void *data);

HZ_ENGINE_API OsWorkQueue *os_work_queue_create(i32 thread_count);
HZ_ENGINE_API void os_work_queue_destroy(OsWorkQueue *queue);

HZ_ENGINE_API void os_add_work_entry(OsWorkQueue *queue,
                                     OsWorkQueueCallback callback, void *data);
HZ_ENGINE_API void os_complete_all_work(OsWorkQueue *queue);

/*!
 * Dynamic Library Loading
 * */
typedef void *OsDynLib;
typedef void *OsDynSymbol;

HZ_ENGINE_API OsDynLib os_dynlib_load(const char *path);
HZ_ENGINE_API void os_dynlib_unload(OsDynLib lib);
HZ_ENGINE_API OsDynSymbol os_dynlib_get_symbol(OsDynLib lib,
                                               const char *symbol_name);

/*!
 * File System
 * */
typedef struct {
  i64 modification_time;
  b32 exists;
} OsFileInfo;

HZ_ENGINE_API OsFileInfo os_file_info(const char *path);
HZ_ENGINE_API b32 os_file_copy(const char *src_path, const char *dst_path);
HZ_ENGINE_API b32 os_file_remove(const char *path);
HZ_ENGINE_API b32 os_file_exists(const char *path);
HZ_ENGINE_API b32 os_directory_copy(const char *src_path, const char *dst_path);
HZ_ENGINE_API b32 os_directory_remove(const char *path);
HZ_ENGINE_API b32 os_system(const char *command);

typedef struct {
  char **paths;
  i32 count;
} OsFileList;

HZ_ENGINE_API OsFileList os_list_files(const char *directory,
                                       const char *extension,
                                       Allocator *allocator);
HZ_ENGINE_API b32 os_file_set_executable(const char *path);
HZ_ENGINE_API char *os_cwd(char *buffer, u32 buffer_size);

/*!
 * System Information
 * */
HZ_ENGINE_API i32 os_get_processor_count(void);

/*!
 * Crash Handling
 * */
HZ_ENGINE_API void os_install_crash_handler(void);

/*!
 * Memory Allocation
 * */
HZ_ENGINE_API u8 *os_allocate_memory(size_t size);
HZ_ENGINE_API void os_free_memory(void *ptr, size_t size);

#endif
