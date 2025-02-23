#include "raylib.h"
#include "rom.c"
#include "typedefs.h"
#include <stdbool.h>
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
  uint32_t tick;
  PacmanRom rom;
  uint32_t frame_buffer[DISPLAY_RES_Y][DISPLAY_RES_X];
} GameState;

typedef struct {
  uint32_t tile_code;
  uint32_t color_code;
  bool8 flip_x;
  bool8 flip_y;
} PacmanSprite;

typedef enum { DIR_RIGHT, DIR_LEFT, DIR_UP, DIR_DOWN } Direction;

typedef struct {
  float x;
  float y;
  Direction dir;

  uint8_t move_speed;
} Pacman;

global GameState game_state = {};

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

void draw_sprite(uint16_t sprite_x, uint16_t sprite_y,
                 const PacmanSprite *sprite) {
  uint32_t tile_code = sprite->tile_code * SPRITE_SIZE;
  uint32_t color_code = sprite->color_code;

  uint8_t tile_offset_x = sprite->flip_x ? SPRITE_SIZE : 0;
  int8_t sign_x = sprite->flip_x ? -1 : 1;

  uint8_t tile_offset_y = sprite->flip_y ? SPRITE_SIZE - 1 : 0;
  int8_t sign_y = sprite->flip_y ? -1 : 1;
  for (uint16_t y = 0; y < SPRITE_SIZE; y++) {
    for (uint16_t x = 0; x < SPRITE_SIZE; x++) {
      uint8_t tile_i =
          game_state.rom.sprite_atlas[tile_offset_y + y * sign_y]
                                     [tile_code + tile_offset_x + sign_x * x];
      uint8_t color_i = color_code * 4 + tile_i;
      uint32_t color = game_state.rom.color_palette[color_i];
      game_state.frame_buffer[y + sprite_y][x + sprite_x] = color;
    }
  }
}

void draw_tile(uint16_t tile_x, uint16_t tile_y, uint32_t tile_code,
               uint8_t color_code) {
  for (uint16_t y = 0; y < TILE_SIZE; y++) {
    for (uint16_t x = 0; x < TILE_SIZE; x++) {
      uint8_t tile_i = game_state.rom.tile_atlas[y][tile_code + x];
      uint8_t color_i = color_code * 4 + tile_i;
      uint32_t color = game_state.rom.color_palette[color_i];
      game_state.frame_buffer[y + tile_y][x + tile_x] = color;
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

void draw_all_sprites() {
  uint16_t x = 0;
  uint16_t y = 0;
  for (size_t i = 0; i < NUM_SPRITES; i++) {
    PacmanSprite sprite = {};
    sprite.tile_code = i;
    sprite.color_code = 14;

    draw_sprite(x, y, &sprite);
    x += SPRITE_SIZE;
    if (x >= DISPLAY_RES_X) {
      x = 0;
      y += SPRITE_SIZE;
    }
  }
}

void render_frame_buffer() {
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

  // clear frame buffer
  memset(&game_state.frame_buffer, 0, sizeof(game_state.frame_buffer));
}

void draw_pacman(Pacman *pacman) {
  local_persist const uint8_t pacman_anim[2][4] = {
      {44, 46, 48, 46}, // horizontal (needs flipx)
      {45, 47, 48, 47}  // vertical (needs flipy)
  };

  PacmanSprite pacman_sprite = {};
  uint8_t anim_tick = (game_state.tick / 2) % 4;
  bool8 is_horizontal = pacman->dir == DIR_RIGHT || pacman->dir == DIR_LEFT;
  pacman_sprite.tile_code = pacman_anim[is_horizontal ? 0 : 1][anim_tick];
  pacman_sprite.color_code = COLOR_PACMAN;
  pacman_sprite.flip_x = pacman->dir == DIR_LEFT;
  pacman_sprite.flip_y = pacman->dir == DIR_UP;
  draw_sprite(pacman->x, pacman->y, &pacman_sprite);
}

void update_pacman(Pacman *pacman, float dt) {
  int move_dir[2] = {};
  if (IsKeyDown(KEY_A)) {
    move_dir[0] -= 1;
    pacman->dir = DIR_LEFT;
  }
  if (IsKeyDown(KEY_D)) {
    move_dir[0] += 1;
    pacman->dir = DIR_RIGHT;
  }
  if (IsKeyDown(KEY_W)) {
    move_dir[1] -= 1;
    pacman->dir = DIR_UP;
  }
  if (IsKeyDown(KEY_S)) {
    move_dir[1] += 1;
    pacman->dir = DIR_DOWN;
  }

  int ds[2] = {move_dir[0] * pacman->move_speed,
               move_dir[1] * pacman->move_speed};
  pacman->x += ds[0] * dt;
  pacman->y += ds[1] * dt;
}

int main(void) {
  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "pacman.c");

  SetTargetFPS(TARGET_FPS);

  pm_init_rom(&game_state.rom);

  Pacman pacman = {};
  pacman.move_speed = 20;

  while (!WindowShouldClose()) {
    float dt = GetFrameTime();
    update_pacman(&pacman, dt);
    draw_pacman(&pacman);

    BeginDrawing();
    ClearBackground(BLACK);
    render_frame_buffer();
    EndDrawing();

    game_state.tick++;
  }

  CloseWindow();

  return 0;
}
