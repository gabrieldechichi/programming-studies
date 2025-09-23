#ifndef H_PLATFORM
#define H_PLATFORM
#include "lib/array.h"
#include "lib/fmt.h"
#include "lib/handle.h"
#include "lib/memory.h"

b32 platform_is_mobile();

typedef enum { LOGLEVEL_INFO, LOGLEVEL_WARN, LOGLEVEL_ERROR } LogLevel;
void platform_log(LogLevel log_level, const char *fmt, const FmtArgs *args,
                  const char *file_name, uint32 line_number);

#define PLATFORM_LOG(level, fmt, ...)                                          \
  do {                                                                         \
    FmtArg args[] = {__VA_ARGS__};                                             \
    FmtArgs fmtArgs = {args, COUNT_VARGS(FmtArg, __VA_ARGS__)};                \
    platform_log(level, fmt, &fmtArgs, __FILE_NAME__, __LINE__);               \
  } while (0)

#define LOG_INFO(fmt, ...) PLATFORM_LOG(LOGLEVEL_INFO, fmt, __VA_ARGS__)
#define LOG_WARN(fmt, ...) PLATFORM_LOG(LOGLEVEL_WARN, fmt, __VA_ARGS__)
#define LOG_ERROR(fmt, ...) PLATFORM_LOG(LOGLEVEL_ERROR, fmt, __VA_ARGS__)

void assert_log(u8 log_level, const char *fmt, const FmtArgs *args,
                const char *file_name, uint32 line_number) {
  platform_log(log_level, fmt, args, file_name, line_number);
}

/*!
 * File IO
 * */

bool32 platform_write_file(const char *file_path, u8 *buffer, u32 buffer_len);
bool32 platform_create_dir(const char *dir_path);

/*!
 * File IO async
 * */
typedef int32 PlatformReadFileOp;
slice_define(PlatformReadFileOp);

typedef enum {
  FREADSTATE_NONE = 0,
  FREADSTATE_IN_PROGRESS,
  FREADSTATE_COMPLETED,
  FREADSTATE_ERROR
} PlatformReadFileState;

typedef struct {
  uint32 buffer_len;
  uint8 *buffer;
} PlatformFileData;

PlatformReadFileOp platform_start_read_file(char *file_name);
PlatformReadFileState platform_check_read_file(PlatformReadFileOp op_id);
bool32 platform_get_file_data(PlatformReadFileOp op_id,
                              _out_ PlatformFileData *data,
                              Allocator *allocator);

typedef int32 PlatformWebPLoadOp;

PlatformWebPLoadOp platform_start_webp_texture_load(const char *file_path,
                                                    u32 file_path_len,
                                                    Handle texture_handle);
PlatformReadFileState
platform_check_webp_texture_load(PlatformWebPLoadOp op_id);

void platform_lock_mouse(b32 lock);

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

PlatformHttpRequestOp
platform_start_http_request(HttpMethod method, const char *url, int url_len,
                            const char *headers, int headers_len,
                            const char *body, int body_len);

HttpOpState platform_check_http_request(PlatformHttpRequestOp op_id);

int32 platform_get_http_response_info(PlatformHttpRequestOp op_id,
                                      _out_ int32 *status_code,
                                      _out_ int32 *headers_len,
                                      _out_ int32 *body_len);

int32 platform_get_http_headers(PlatformHttpRequestOp op_id, char *buffer,
                                int32 buffer_len);

int32 platform_get_http_body(PlatformHttpRequestOp op_id, char *buffer,
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

PlatformHttpStreamOp platform_start_http_stream(HttpMethod method,
                                                const char *url, int url_len,
                                                const char *headers,
                                                int headers_len,
                                                const char *body, int body_len);

HttpStreamState platform_check_http_stream(PlatformHttpStreamOp op_id);

int32 platform_get_http_stream_info(PlatformHttpStreamOp op_id,
                                    _out_ int32 *status_code);

int32 platform_get_http_stream_chunk_size(PlatformHttpStreamOp op_id);

int32 platform_get_http_stream_chunk(PlatformHttpStreamOp op_id, char *buffer,
                                     int32 buffer_len, _out_ bool32 *is_final);

/*!
 * Timing
 * */
u64 platform_time_now(void);
u64 platform_time_diff(u64 new_ticks, u64 old_ticks);
f64 platform_ticks_to_ms(u64 ticks);
f64 platform_ticks_to_us(u64 ticks);
f64 platform_ticks_to_ns(u64 ticks);

/*!
 * Audio
 * */
void platform_audio_init(void);
void platform_audio_shutdown(void);
void platform_audio_update(void);
void platform_audio_write_samples(f32 *samples, i32 sample_count);
i32 platform_audio_get_sample_rate();

#if defined(__wasm__)
extern unsigned char __heap_base;

export void *platform_get_heap_base(void) { return &__heap_base; }
#endif

#endif
