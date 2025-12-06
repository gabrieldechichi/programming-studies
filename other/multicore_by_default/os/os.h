#ifndef H_PLATFORM
#define H_PLATFORM
#include "lib/fmt.h"
#include "lib/memory.h"

b32 os_is_mobile();

typedef enum
{
  OS_THERMAL_STATE_UNKNOWN = 0,
  OS_THERMAL_STATE_NOMINAL,
  OS_THERMAL_STATE_FAIR,
  OS_THERMAL_STATE_SERIOUS,
  OS_THERMAL_STATE_CRITICAL
} OSThermalState;

HZ_ENGINE_API OSThermalState os_get_thermal_state(void);

typedef enum
{
  LOGLEVEL_INFO,
  LOGLEVEL_WARN,
  LOGLEVEL_ERROR
} LogLevel;
HZ_ENGINE_API void os_log(LogLevel log_level, const char *fmt,
                          const FmtArgs *args, const char *file_name,
                          uint32 line_number);

#define PLATFORM_LOG(level, fmt, ...)                      \
  do                                                       \
  {                                                        \
    FmtArg args[] = {(FmtArg){.type = 0}, ##__VA_ARGS__};  \
    size_t _count = (sizeof(args) / sizeof(FmtArg)) - 1;   \
    FmtArgs fmtArgs = {args + 1, (u8)_count};              \
    os_log(level, fmt, &fmtArgs, __FILE_NAME__, __LINE__); \
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

typedef struct
{
  uint32 buffer_len;
  uint8 *buffer;
  bool32 success;
} PlatformFileData;

HZ_ENGINE_API PlatformFileData os_read_file(const char *file_path,
                                            Allocator *allocator);

typedef enum
{
  OS_FILE_READ_STATE_NONE = 0,
  OS_FILE_READ_STATE_IN_PROGRESS = 1,
  OS_FILE_READ_STATE_COMPLETED = 2,
  OS_FILE_READ_STATE_ERROR = 3
} OsFileReadState;

typedef struct OsFileOp OsFileOp;

HZ_ENGINE_API void os_lock_mouse(b32 lock);
HZ_ENGINE_API b32 os_is_mouse_locked(void);
HZ_ENGINE_API void os_show_keyboard(b32 show, f32 time);
HZ_ENGINE_API b32 os_is_keyboard_shown();
HZ_ENGINE_API void os_set_text_input(const char *text, uint32_t cursor_pos, uint32_t selection_length);

typedef struct
{
  f32 x;
  f32 y;
  f32 width;
  f32 height;
} OsKeyboardRect;

HZ_ENGINE_API OsKeyboardRect os_get_keyboard_rect(f32 time);

typedef struct
{
  f32 top;
  f32 left;
  f32 bottom;
  f32 right;
} OsSafeAreaInsets;

HZ_ENGINE_API OsSafeAreaInsets os_get_safe_area(void);

/*!
 * HTTP Requests
 * */
typedef enum
{
  HTTP_METHOD_GET = 0,
  HTTP_METHOD_POST = 1,
  HTTP_METHOD_PUT = 2,
  HTTP_METHOD_DELETE = 3
} HttpMethod;

typedef enum
{
  HTTP_OP_NONE = 0,
  HTTP_OP_IN_PROGRESS = 1,
  HTTP_OP_COMPLETED = 2,
  HTTP_OP_ERROR = 3
} HttpOpState;

typedef enum
{
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
typedef enum
{
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
HZ_ENGINE_API void os_sleep(u64 microseconds);

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
 * OS Initialization
 * */
HZ_ENGINE_API void os_init(void);

/*!
 * Threading - see lib/thread.h for opaque types and thin wrapper API
 * */
#include "lib/thread.h"

HZ_ENGINE_API Thread os_thread_launch(ThreadFunc func, void *arg);
HZ_ENGINE_API b32 os_thread_join(Thread t, u64 timeout_us);
HZ_ENGINE_API void os_thread_detach(Thread t);

HZ_ENGINE_API Mutex os_mutex_alloc(void);
HZ_ENGINE_API void os_mutex_release(Mutex m);
HZ_ENGINE_API void os_mutex_take(Mutex m);
HZ_ENGINE_API void os_mutex_drop(Mutex m);

HZ_ENGINE_API Semaphore os_semaphore_alloc(i32 initial_count);
HZ_ENGINE_API void os_semaphore_release(Semaphore s);
HZ_ENGINE_API void os_semaphore_take(Semaphore s);
HZ_ENGINE_API void os_semaphore_drop(Semaphore s);

HZ_ENGINE_API RWMutex os_rw_mutex_alloc(void);
HZ_ENGINE_API void os_rw_mutex_release(RWMutex m);
HZ_ENGINE_API void os_rw_mutex_take_r(RWMutex m);
HZ_ENGINE_API void os_rw_mutex_drop_r(RWMutex m);
HZ_ENGINE_API void os_rw_mutex_take_w(RWMutex m);
HZ_ENGINE_API void os_rw_mutex_drop_w(RWMutex m);

HZ_ENGINE_API CondVar os_cond_var_alloc(void);
HZ_ENGINE_API void os_cond_var_release(CondVar cv);
HZ_ENGINE_API b32 os_cond_var_wait(CondVar cv, Mutex m, u64 timeout_us);
HZ_ENGINE_API void os_cond_var_signal(CondVar cv);
HZ_ENGINE_API void os_cond_var_broadcast(CondVar cv);

HZ_ENGINE_API Barrier os_barrier_alloc(u32 count);
HZ_ENGINE_API void os_barrier_release(Barrier b);
HZ_ENGINE_API void os_barrier_wait(Barrier b);

/*!
 * Async File Operations
 * */
typedef struct TaskSystem TaskSystem;

HZ_ENGINE_API OsFileOp *os_start_read_file(const char *file_path,
                                           TaskSystem *mcr_system);
HZ_ENGINE_API OsFileReadState os_check_read_file(OsFileOp *op);
HZ_ENGINE_API i32 os_get_file_size(OsFileOp *op);
HZ_ENGINE_API b32 os_get_file_data(OsFileOp *op,
                                   _out_ PlatformFileData *data,
                                   Allocator *allocator);

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
typedef struct
{
  i64 modification_time;
  b32 exists;
} OsFileInfo;

HZ_ENGINE_API OsFileInfo os_file_info(const char *path);
HZ_ENGINE_API b32 os_file_copy(const char *src_path, const char *dst_path);
HZ_ENGINE_API b32 os_file_remove(const char *path);
HZ_ENGINE_API b32 os_file_exists(const char *path);
HZ_ENGINE_API b32 os_directory_copy(const char *src_path, const char *dst_path);
HZ_ENGINE_API b32 os_directory_remove(const char *path);
HZ_ENGINE_API b32 os_symlink(const char *target_path, const char *link_path);
HZ_ENGINE_API b32 os_system(const char *command);

typedef struct
{
  char **paths;
  i32 count;
} OsFileList;

HZ_ENGINE_API OsFileList os_list_files(const char *directory,
                                       const char *extension,
                                       Allocator *allocator);
HZ_ENGINE_API OsFileList os_list_dirs(const char *directory,
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
HZ_ENGINE_API u8 *os_reserve_memory(size_t size);
HZ_ENGINE_API b32 os_commit_memory(void *ptr, size_t size);
HZ_ENGINE_API u32 os_get_page_size(void);

/*!
 * Compressed Texture Format Detection
 * */
typedef enum
{
  COMPRESSED_TEXTURE_FORMAT_NONE = 0,
  COMPRESSED_TEXTURE_FORMAT_DXT5,
  COMPRESSED_TEXTURE_FORMAT_ETC2,
  COMPRESSED_TEXTURE_FORMAT_ASTC,
} CompressedTextureFormat;

HZ_ENGINE_API const char *os_get_compressed_texture_format_suffix(void);

/*!
 * Video Decoder
 * */
typedef i32 OsVideoDecoder;

typedef struct
{
  u32 width;
  u32 height;
  f32 duration_seconds;
  f32 framerate;
  b32 has_audio;
  i32 audio_sample_rate;
  i32 audio_channels;
} OsVideoInfo;

typedef enum
{
  OS_VIDEO_DECODE_OK = 0,
  OS_VIDEO_DECODE_EOF = 1,
  OS_VIDEO_DECODE_ERROR = 2,
  OS_VIDEO_DECODE_NO_FRAME = 3,
} OsVideoDecodeResult;

HZ_ENGINE_API OsVideoDecoder os_video_decoder_create(u8 *data, u32 data_len);
HZ_ENGINE_API OsVideoInfo os_video_decoder_get_info(OsVideoDecoder handle);
HZ_ENGINE_API OsVideoDecodeResult os_video_decoder_get_frame(
    OsVideoDecoder handle, f32 time_seconds, u8 *rgba_buffer, u32 buffer_size);
HZ_ENGINE_API void os_video_decoder_destroy(OsVideoDecoder handle);

HZ_ENGINE_API i32 os_video_decoder_get_audio(OsVideoDecoder handle,
                                             i16 *sample_buffer,
                                             i32 max_samples,
                                             i32 *samples_decoded);

HZ_ENGINE_API OsVideoDecodeResult os_video_decoder_seek(OsVideoDecoder handle,
                                                        f32 time_seconds);

HZ_ENGINE_API u32 os_mic_get_available_samples(void);
HZ_ENGINE_API u32 os_mic_read_samples(i16 *buffer, u32 max_samples);
HZ_ENGINE_API void os_mic_start_recording(void);
HZ_ENGINE_API void os_mic_stop_recording(void);
HZ_ENGINE_API u32 os_mic_get_sample_rate(void);

#endif
