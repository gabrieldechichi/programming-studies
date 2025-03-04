#ifndef H_GAME
#define H_GAME

#include <stdint.h>
#include "./typedefs.h"

typedef struct {
} game_memory_t;

typedef struct {
    bool8_t is_pressed;
    bool8_t pressed_this_frame;
    bool8_t released_this_frame;
} input_button_t;

typedef struct {
    input_button_t space_bar;
} game_input_t;

typedef struct {
  uint16_t width;
  uint16_t height;
  uint32_t *pixels;
} game_offscreen_buffer_t;

typedef struct {
  int32_t sample_rate;
  int32_t sample_count;
  float *samples;
  bool8_t clear_buffer;
} game_sound_buffer_t;

#define GAME_UPDATE_AND_RENDER(name)                                           \
  void name(game_memory_t *memory, game_input_t *input,                        \
            game_offscreen_buffer_t *screen_buffer,                            \
            game_sound_buffer_t *sound_buffer)

typedef GAME_UPDATE_AND_RENDER(game_update_and_render_t);

GAME_UPDATE_AND_RENDER(game_update_and_render);
#endif
