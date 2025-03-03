#ifndef H_GAME
#define H_GAME

#include <stdint.h>
typedef struct {
} game_memory_t;

typedef struct {
} game_input_t;

typedef struct {
  uint16_t width;
  uint16_t height;
  uint32_t *pixels;
} game_offscreen_buffer_t;

typedef struct {

} game_sound_buffer_t;

#define GAME_UPDATE_AND_RENDER(name)                                           \
  void name(game_memory_t * memory, game_input_t * input,                           \
       game_offscreen_buffer_t * screen_buffer)

typedef GAME_UPDATE_AND_RENDER(game_update_and_render_t);

void game_update_and_render(game_memory_t *memory, game_input_t *input,
                            game_offscreen_buffer_t *screen_buffer);
#endif
