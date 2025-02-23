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

typedef struct {
  uint32_t tile_code;
  uint32_t color_code;
} PacmanSprite;

global GameState game_state;

PacmanSprite sprite_pacman = {SPRITETILE_PACMAN_CLOSED_MOUTH, COLOR_PACMAN};

Color u32_to_color(uint32_t color) {
  return (Color){
      (color >> 0) & 0xFF,
      (color >> 8) & 0xFF,
      (color >> 16) & 0xFF,
      (color >> 24) & 0xFF,
  };
}

void draw_tile_color(uint8_t tile_x, uint8_t tile_y, uint32_t color) {
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

    draw_tile_color(x, y, c);
    x++;
    if (x >= DISPLAY_TILES_X) {
      x = 0;
      y++;
    }
  }
}

void draw_from_atlas(uint16_t x, uint16_t y, uint8_t *atlas,
                     uint16_t atlas_width, uint8_t tile_size,
                     uint32_t tile_code, uint8_t color_code) {
  for (uint16_t y = 0; y < tile_size; y++) {
    for (uint16_t x = 0; x < tile_size; x++) {
      uint8_t tile_i = atlas[XY_TO_INDEX(tile_code + x, y, atlas_width)];
      uint8_t color_i = color_code * 4 + tile_i;
      uint32_t color = game_state.rom.color_palette[color_i];
      game_state.frame_buffer[y][x] = color;
    }
  }
}

void draw_sprite(uint16_t sprite_x, uint16_t sprite_y, PacmanSprite sprite) {
  draw_from_atlas(sprite_x, sprite_y, (uint8_t *)game_state.rom.sprite_atlas,
                  TILE_TEXTURE_WIDTH, SPRITE_SIZE,
                  sprite.tile_code * SPRITE_SIZE, sprite.color_code);
}

void draw_tile(uint16_t tile_x, uint16_t tile_y, uint32_t tile_code,
               uint8_t color_code) {
  draw_from_atlas(tile_x, tile_y, (uint8_t *)game_state.rom.tile_atlas,
                  TILE_TEXTURE_WIDTH, TILE_SIZE, tile_code, color_code);
}

int main(void) {
  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "pacman.c");

  SetTargetFPS(TARGET_FPS);

  pm_init_rom(&game_state.rom);

  while (!WindowShouldClose()) {
    BeginDrawing();

    ClearBackground(BLACK);

    // draw_color_palette();
    draw_sprite(0, 0, sprite_pacman);

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
