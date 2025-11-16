#ifndef H_OS
#define H_OS

#include "lib/handle.h"
#include "lib/memory.h"
#include "lib/typedefs.h"

/*OS Log*/
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
/*End - OS Log*/

/*OS File IO*/
bool32 platform_write_file(const char *file_path, u8 *buffer, u32 buffer_len);
bool32 platform_create_dir(const char *dir_path);

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
/*End - OS File IO*/

/*OS Time*/
void platform_init(void);

u64 platform_time_now(void);
u64 platform_time_diff(u64 new_ticks, u64 old_ticks);
f64 platform_ticks_to_ms(u64 ticks);
f64 platform_ticks_to_us(u64 ticks);
f64 platform_ticks_to_ns(u64 ticks);

void platform_sleep_us(u32 microseconds);
/*END - OS Time*/

/*OS Audio*/
void platform_audio_init(void);
void platform_audio_shutdown(void);
void platform_audio_update(void);
void platform_audio_write_samples(f32 *samples, i32 sample_count);
i32 platform_audio_get_sample_rate();
/*END - OS Audio*/
#endif
