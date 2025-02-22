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
  uint32_t frame_buffer[DISPLAY_RES_X * DISPLAY_RES_Y];
  struct {
    uint8_t video_ram[DISPLAY_TILES_Y][DISPLAY_TILES_X];
    uint8_t color_ram[DISPLAY_TILES_Y][DISPLAY_TILES_X];
  } graphics;
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

void draw_tile(uint16_t x, uint16_t y, uint32_t color, uint8_t tile_size) {
  for (uint16_t yy = 0; yy < tile_size; yy++) {
    for (uint16_t xx = 0; xx < tile_size; xx++) {
      uint16_t py = y + yy;
      uint16_t px = x + xx;
      int16_t pi = py * DISPLAY_RES_Y + px;
      game_state.frame_buffer[pi] = color;
    }
  }
}

void draw_color_palette() {
  uint16_t x = 0;
  uint16_t y = 0;
  uint8_t tile_size = 8;
  for (size_t i = 0; i < ARRAY_SIZE(game_state.rom.color_palette); i++) {
    draw_tile(x, y, game_state.rom.color_palette[i], tile_size);
    x += tile_size;
    if (x >= DISPLAY_RES_X) {
      x = 0;
      y += tile_size;
    }
  }
}

int main(void) {
  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "pacman.c");

  SetTargetFPS(TARGET_FPS);

  pm_init_rom(&game_state.rom);

  while (!WindowShouldClose()) {
    BeginDrawing();

    ClearBackground(GRAY);

    draw_color_palette();

    for (uint16_t y = 0; y < DISPLAY_RES_Y; y++) {
      for (uint16_t x = 0; x < DISPLAY_RES_X; x++) {
        uint16_t i = y * DISPLAY_RES_X + x;
        uint32_t color = game_state.frame_buffer[i];
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
