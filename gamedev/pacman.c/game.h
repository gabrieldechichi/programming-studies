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

typedef struct {
  uint64 time_ns;
  uint64 dt_ns;
} Game_Time;

// game
typedef struct {
    Game_Time time;
  PlatformInterface platform;
} Game_Memory;

typedef struct {
  bool is_pressed;
  bool pressed_this_frame;
  bool released_this_frame;
} Game_InputButton;

#define INPUT_BUTTONS                                                          \
  INPUT_BUTTON(KEY_A, "A"), INPUT_BUTTON(KEY_D, "D"),                          \
      INPUT_BUTTON(KEY_W, "W"), INPUT_BUTTON(KEY_S, "S"),                      \
      INPUT_BUTTON(KEY_SPACE, "Space")

typedef enum {
#define INPUT_BUTTON(v, a) v
  INPUT_BUTTONS,
  KEY_MAX
#undef INPUT_BUTTON
} Game_InputButtonType;

global const char *input_button_names[KEY_MAX] = {
#define INPUT_BUTTON(v, a) a
    INPUT_BUTTONS
#undef INPUT_BUTTON
};

#undef INPUT_BUTTONS

typedef enum { EVENT_KEYDOWN, EVENT_KEYUP } Game_InputEventType;

typedef struct {
  Game_InputEventType type;
  union {
    struct {
      Game_InputButtonType type;
    } key;
  };

} Game_InputEvent;

typedef struct {
  Game_InputEvent events[20];
  uint8 len;
} Game_InputEvents;

typedef struct {
  uint16 width;
  uint16 height;
  uint32 *pixels;
} Game_ScreenBuffer;

typedef struct {
  int32 sample_rate;
  int32 sample_count;
  float *samples;
  int32 write_count;
  bool clear_buffer;
} Game_SoundBuffer;

#define GAME_INIT(name) void name(Game_Memory *memory)
typedef GAME_INIT(Game_Init);
GAME_INIT(game_init);

#define GAME_UPDATE_AND_RENDER(name)                                           \
  void name(Game_Memory *memory, Game_InputEvents *input,                      \
            Game_ScreenBuffer *screen_buffer, Game_SoundBuffer *sound_buffer)
typedef GAME_UPDATE_AND_RENDER(Game_UpdateAndRender);
GAME_UPDATE_AND_RENDER(game_update_and_render);

#endif
