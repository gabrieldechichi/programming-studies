#ifndef H_GAME
#define H_GAME

#include "./typedefs.h"
#include <stdint.h>

// platform
typedef enum {
    LOG_INFO,
    LOG_ERROR,
} LogType;

#define PLATFORM_LOG(name) void name(const char *fmt, LogType log_type, ...)
typedef PLATFORM_LOG(Platform_Log);

typedef struct {
  Platform_Log *platform_log;
} PlatformInterface;

// game
typedef struct {
  PlatformInterface platform;
} Game_Memory;

typedef struct {
  bool is_pressed;
  bool pressed_this_frame;
  bool released_this_frame;
} Game_InputButton;

typedef struct {
  Game_InputButton space_bar;
} Game_Input;

typedef struct {
  uint16 width;
  uint16 height;
  uint32 *pixels;
} Game_ScreenBuffer;

typedef struct {
  int32 sample_rate;
  int32 sample_count;
  float *samples;
  bool clear_buffer;
} Game_SoundBuffer;

#define GAME_INIT(name) void name(Game_Memory* memory)
typedef GAME_INIT(Game_Init);
GAME_INIT(game_init);

#define GAME_UPDATE_AND_RENDER(name)                                           \
  void name(Game_Memory* memory, Game_Input *input, Game_ScreenBuffer *screen_buffer,               \
            Game_SoundBuffer *sound_buffer)
typedef GAME_UPDATE_AND_RENDER(Game_UpdateAndRender);
GAME_UPDATE_AND_RENDER(game_update_and_render);

#endif
