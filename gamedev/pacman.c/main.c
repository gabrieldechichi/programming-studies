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

void draw_color_palette() {
  const size_t tile_size = 1;
  const size_t padding = 0;
  size_t offset_x = padding;
  size_t offset_y = 100;
  for (size_t i = 0; i < ARRAY_SIZE(game_state.rom.color_palette); i++) {
    Color color = u32_to_color(game_state.rom.color_palette[i]);
    for (size_t y = 0; y < tile_size; y++) {
      for (size_t x = 0; x < tile_size; x++) {
        size_t px = x + offset_x;
        size_t py = y + offset_y;
        DrawPixel(px, py, color);
      }
    }

    offset_x += tile_size + padding;

    if (offset_x + tile_size > SCREEN_WIDTH) {
      offset_x = padding;
      offset_y += tile_size + padding;
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

    for (size_t y = 0; y < DISPLAY_RES_Y; y++) {
      for (size_t x = 0; x < DISPLAY_RES_X; x++) {
        size_t i = y * DISPLAY_RES_X + x;
        game_state.frame_buffer[i] = game_state.rom.color_palette[COLOR_PACMAN];
      }
    }

    for (uint16_t y = 0; y < DISPLAY_RES_Y; y++) {
      for (uint16_t x = 0; x < DISPLAY_RES_X; x++) {
        uint16_t i = y * DISPLAY_RES_X + x;
        uint32_t color = game_state.frame_buffer[i];
        Color rl_color = u32_to_color(color);
        rl_color = RED;

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
