#include "raylib.h"
#include "rom.c"
#include "typedefs.h"
#include <stdint.h>
#include <string.h>

#define TARGET_FPS 60

#define DISPLAY_TILES_X (28)
#define DISPLAY_TILES_Y (36)
#define TILE_WIDTH (8)
#define TILE_HEIGHT (8)

#define SCREEN_WIDTH DISPLAY_TILES_X *TILE_WIDTH * 2
#define SCREEN_HEIGHT DISPLAY_TILES_Y *TILE_HEIGHT * 2

/* common tile, sprite and color codes, these are the same as on the Pacman
   arcade machine and extracted by looking at memory locations of a Pacman
   emulator
*/
enum {
  TILE_SPACE = 0x40,
  TILE_DOT = 0x10,
  TILE_PILL = 0x14,
  TILE_GHOST = 0xB0,
  TILE_LIFE = 0x20,       // 0x20..0x23
  TILE_CHERRIES = 0x90,   // 0x90..0x93
  TILE_STRAWBERRY = 0x94, // 0x94..0x97
  TILE_PEACH = 0x98,      // 0x98..0x9B
  TILE_BELL = 0x9C,       // 0x9C..0x9F
  TILE_APPLE = 0xA0,      // 0xA0..0xA3
  TILE_GRAPES = 0xA4,     // 0xA4..0xA7
  TILE_GALAXIAN = 0xA8,   // 0xA8..0xAB
  TILE_KEY = 0xAC,        // 0xAC..0xAF
  TILE_DOOR = 0xCF,       // the ghost-house door

  SPRITETILE_INVISIBLE = 30,
  SPRITETILE_SCORE_200 = 40,
  SPRITETILE_SCORE_400 = 41,
  SPRITETILE_SCORE_800 = 42,
  SPRITETILE_SCORE_1600 = 43,
  SPRITETILE_CHERRIES = 0,
  SPRITETILE_STRAWBERRY = 1,
  SPRITETILE_PEACH = 2,
  SPRITETILE_BELL = 3,
  SPRITETILE_APPLE = 4,
  SPRITETILE_GRAPES = 5,
  SPRITETILE_GALAXIAN = 6,
  SPRITETILE_KEY = 7,
  SPRITETILE_PACMAN_CLOSED_MOUTH = 48,

  COLOR_BLANK = 0x00,
  COLOR_DEFAULT = 0x0F,
  COLOR_DOT = 0x10,
  COLOR_PACMAN = 0x09,
  COLOR_BLINKY = 0x01,
  COLOR_PINKY = 0x03,
  COLOR_INKY = 0x05,
  COLOR_CLYDE = 0x07,
  COLOR_FRIGHTENED = 0x11,
  COLOR_FRIGHTENED_BLINKING = 0x12,
  COLOR_GHOST_SCORE = 0x18,
  COLOR_EYES = 0x19,
  COLOR_CHERRIES = 0x14,
  COLOR_STRAWBERRY = 0x0F,
  COLOR_PEACH = 0x15,
  COLOR_BELL = 0x16,
  COLOR_APPLE = 0x14,
  COLOR_GRAPES = 0x17,
  COLOR_GALAXIAN = 0x09,
  COLOR_KEY = 0x16,
  COLOR_WHITE_BORDER = 0x1F,
  COLOR_FRUIT_SCORE = 0x03,
};

typedef struct {
  PacmanRom rom;
  struct {
    uint8_t video_ram[DISPLAY_TILES_Y][DISPLAY_TILES_X];
    uint8_t color_ram[DISPLAY_TILES_Y][DISPLAY_TILES_X];
  } graphics;
} GameState;

global GameState game_state;

Color rl_to_color(uint32_t color) {
  return (Color){
      (color >> 0) & 0xFF,
      (color >> 8) & 0xFF,
      (color >> 16) & 0xFF,
      (color >> 24) & 0xFF,
  };
}

int main(void) {
  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "pacman.c");

  SetTargetFPS(TARGET_FPS);

  pm_init_rom(&game_state.rom);

  while (!WindowShouldClose()) {
    BeginDrawing();

    ClearBackground(BLACK);

    const size_t tile_size = 16;
    const size_t padding = 2;
    size_t offset_x = padding;
    size_t offset_y = padding;
    for (size_t i = 0; i < ARRAY_SIZE(game_state.rom.color_palette); i++) {
      Color color = rl_to_color(game_state.rom.color_palette[i]);
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

    EndDrawing();
  }

  CloseWindow();

  return 0;
}
