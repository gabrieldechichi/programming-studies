#include "game.h"
#include "typedefs.h"

export void game_update_and_render(game_memory_t *memory, game_input_t *input,
                                   game_offscreen_buffer_t *screen_buffer) {

  // pixel stuff
  {
    static uint8_t r_shift = 0;
    static uint8_t g_shift = 0xFF / 2;
    r_shift += 1;
    g_shift -= 1;
    for (int y = 0; y < screen_buffer->height; y++) {
      for (int x = 0; x < screen_buffer->width; x++) {
        int i = y * screen_buffer->width + x;
        uint32_t color = r_shift << 24 | g_shift << 16 | 0xFF;
        screen_buffer->pixels[i] = color;
      }
    }
  }
}
