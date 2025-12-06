#ifndef H_OS_VIDEO
#define H_OS_VIDEO

#include "lib/typedefs.h"
#include "lib/memory.h"

typedef struct OsVideoPlayer OsVideoPlayer;

typedef enum {
    OS_VIDEO_STATE_IDLE = 0,
    OS_VIDEO_STATE_PLAYING,
    OS_VIDEO_STATE_PAUSED,
    OS_VIDEO_STATE_ENDED,
    OS_VIDEO_STATE_ERROR
} OsVideoState;

typedef struct {
    const char *file_path;
    void *device;
    void *device_context;
    b32 loop;
    u32 output_width;
    u32 output_height;
} OsVideoPlayerDesc;

HZ_ENGINE_API b32 os_video_init(void);
HZ_ENGINE_API void os_video_shutdown(void);

HZ_ENGINE_API OsVideoPlayer *os_video_player_create(OsVideoPlayerDesc *desc, Allocator *allocator);
HZ_ENGINE_API void os_video_player_destroy(OsVideoPlayer *player);

HZ_ENGINE_API void os_video_player_play(OsVideoPlayer *player);
HZ_ENGINE_API void os_video_player_pause(OsVideoPlayer *player);
HZ_ENGINE_API void os_video_player_stop(OsVideoPlayer *player);
HZ_ENGINE_API void os_video_player_seek(OsVideoPlayer *player, f64 time_seconds);
HZ_ENGINE_API void os_video_player_set_loop(OsVideoPlayer *player, b32 loop);

HZ_ENGINE_API OsVideoState os_video_player_get_state(OsVideoPlayer *player);
HZ_ENGINE_API f64 os_video_player_get_duration(OsVideoPlayer *player);
HZ_ENGINE_API f64 os_video_player_get_current_time(OsVideoPlayer *player);
HZ_ENGINE_API void os_video_player_get_dimensions(OsVideoPlayer *player, u32 *width, u32 *height);
HZ_ENGINE_API void os_video_player_get_output_dimensions(OsVideoPlayer *player, u32 *width, u32 *height);

HZ_ENGINE_API void os_video_player_set_output_textures(OsVideoPlayer *player, void *texture_a, void *texture_b);
HZ_ENGINE_API void *os_video_player_get_display_texture(OsVideoPlayer *player);
HZ_ENGINE_API b32 os_video_player_update(OsVideoPlayer *player, f64 delta_time);

HZ_ENGINE_API b32 os_video_player_has_audio(OsVideoPlayer *player);
HZ_ENGINE_API void os_video_player_get_audio_format(OsVideoPlayer *player, u32 *sample_rate, u32 *channels);
HZ_ENGINE_API u32 os_video_player_read_audio(OsVideoPlayer *player, u8 *buffer, u32 max_bytes);

#endif
