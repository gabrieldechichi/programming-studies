#ifndef H_GAME
#define H_GAME

#include "./typedefs.h"
#include <stdint.h>

typedef struct {
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

#define GAME_INIT(name) void name()

#define GAME_UPDATE_AND_RENDER(name)                                           \
  void name(Game_Input *input, Game_ScreenBuffer *screen_buffer,               \
            Game_SoundBuffer *sound_buffer)

typedef GAME_INIT(Game_Init);
typedef GAME_UPDATE_AND_RENDER(Game_UpdateAndRender);

GAME_INIT(game_init);
GAME_UPDATE_AND_RENDER(game_update_and_render);
#endif
