#include "raylib.h"
#include "rom.c"
#include "typedefs.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TARGET_FPS 60

#define DISPLAY_TILES_X (28)
#define DISPLAY_TILES_Y (36)

#define DISPLAY_TILES_X (28)
#define DISPLAY_TILES_Y (36)
#define DISPLAY_RES_X (224)
#define DISPLAY_RES_Y (288)
#define PIXEL_SCALE 2
#define SCREEN_WIDTH DISPLAY_RES_X *PIXEL_SCALE
#define SCREEN_HEIGHT DISPLAY_RES_Y *PIXEL_SCALE

typedef struct {
  PacmanRom rom;
  uint32_t frame_buffer[DISPLAY_RES_Y][DISPLAY_RES_X];
} GameState;

global GameState game_state;

Color u32_to_color(uint32_t color) {
  return (Color){
      (color >> 0) & 0xFF,
      (color >> 8) & 0xFF,
      (color >> 16) & 0xFF,
      (color >> 24) & 0xFF,
  };
}

#define XY_TO_SCREEN_BUFFER_INDEX(x, y) XY_TO_INDEX(x, y, DISPLAY_RES_X)

void draw_tile(uint8_t tile_x, uint8_t tile_y, uint32_t color) {
  uint16_t x = tile_x * TILE_SIZE;
  uint16_t y = tile_y * TILE_SIZE;

  for (uint16_t yy = 0; yy < TILE_SIZE; yy++) {
    for (uint16_t xx = 0; xx < TILE_SIZE; xx++) {
      uint16_t py = y + yy;
      uint16_t px = x + xx;
      game_state.frame_buffer[py][px] = color;
    }
  }
}

void draw_color_palette() {
  uint16_t x = 0;
  uint16_t y = 0;
  for (size_t i = 0; i < ARRAY_SIZE(game_state.rom.color_palette); i++) {
    uint32_t c = game_state.rom.color_palette[i];

    draw_tile(x, y, c);
    x++;
    if (x >= DISPLAY_TILES_X) {
      x = 0;
      y++;
    }
  }
}

void draw_sprite(uint16_t sprite_x, uint16_t sprite_y, uint32_t tile_code,
                 uint8_t color_code) {
  for (uint16_t y = 0; y < SPRITE_SIZE; y++) {
    for (uint16_t x = 0; x < SPRITE_SIZE; x++) {
      uint8_t tile_i = game_state.rom.sprite_atlas[y][tile_code + x];
      uint8_t color_i = color_code * 4 + tile_i;
      uint32_t color = game_state.rom.color_palette[color_i];
      game_state.frame_buffer[sprite_y + y][sprite_x + x] = color;
    }
  }
}

int main(void) {
  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "pacman.c");

  SetTargetFPS(TARGET_FPS);

  pm_init_rom(&game_state.rom);

  while (!WindowShouldClose()) {
    BeginDrawing();

    ClearBackground(BLACK);

    // draw_color_palette();
    uint32_t tile_code = 16 * 46;
    uint8_t color_code = 0x09;
    draw_sprite(0, 0, tile_code, color_code);

    for (uint16_t y = 0; y < DISPLAY_RES_Y; y++) {
      for (uint16_t x = 0; x < DISPLAY_RES_X; x++) {
        uint32_t color = game_state.frame_buffer[y][x];
        Color rl_color = u32_to_color(color);
        for (uint16_t yy = 0; yy < PIXEL_SCALE; yy++) {
          for (uint16_t xx = 0; xx < PIXEL_SCALE; xx++) {
            uint16_t py = y * PIXEL_SCALE + yy;
            uint16_t px = x * PIXEL_SCALE + xx;
            DrawPixel(px, py, rl_color);
          }
        }
      }
    }

    EndDrawing();
  }

  CloseWindow();

  return 0;
}
