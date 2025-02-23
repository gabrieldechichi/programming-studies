#include "raylib.h"
#include "rom.c"
#include "typedefs.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIM_SPEED 1
#define TARGET_FPS 60 * SIM_SPEED

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
  Texture2D render_texture;
} GameState;

typedef struct {
  uint32_t tile_code;
  uint32_t color_code;
  bool8 flip_x;
  bool8 flip_y;
} PacmanSprite;

typedef struct {
  int16_t x;
  int16_t y;
} int2_t;

// note: low bit = 0 for horizontal, low bit = 1 for vertical
typedef enum { DIR_RIGHT, DIR_DOWN, DIR_LEFT, DIR_UP, NUM_DIRS } Direction;

typedef struct {
  int2_t pos;
  Direction dir;
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

int2_t dir_to_vec(Direction dir) {
  local_persist const int2_t dir_map[NUM_DIRS] = {
      {+1, 0}, {0, +1}, {-1, 0}, {0, -1}};
  return dir_map[dir];
}

bool8 dir_is_horizontal(Direction dir) { return !(dir & 1); }

int2_t i2(int16_t x, int16_t y) { return (int2_t){x, y}; }

int2_t add_i2(int2_t v0, int2_t v1) {
  return (int2_t){v0.x + v1.x, v0.y + v1.y};
}

int2_t sub_i2(int2_t v0, int2_t v1) {
  return (int2_t){v0.x - v1.x, v0.y - v1.y};
}

int2_t mul_i2(int2_t v, int16_t s) { return (int2_t){v.x * s, v.y * s}; }

int32_t squared_distance_i2(int2_t v0, int2_t v1) {
  int2_t d = {v1.x - v0.x, v1.y - v0.y};
  return d.x * d.x + d.y * d.y;
}

bool8 equal_i2(int2_t v0, int2_t v1) {
  return (v0.x == v1.x) && (v0.y == v1.y);
}

bool8 nearequal_i2(int2_t v0, int2_t v1, int16_t tolerance) {
  return (abs(v1.x - v0.x) <= tolerance) && (abs(v1.y - v0.y) <= tolerance);
}

int2_t actor_to_sprite_pos(int2_t pos) {
  return i2(pos.x - HALF_SPRITE_SIZE, pos.y - HALF_SPRITE_SIZE);
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

void draw_sprite(int16_t sprite_x, int16_t sprite_y,
                 const PacmanSprite *sprite) {
  uint32_t tile_code = sprite->tile_code * SPRITE_SIZE;
  uint32_t color_code = sprite->color_code;

  uint8_t tile_offset_x = sprite->flip_x ? SPRITE_SIZE : 0;
  int8_t sign_x = sprite->flip_x ? -1 : 1;

  uint8_t tile_offset_y = sprite->flip_y ? SPRITE_SIZE - 1 : 0;
  int8_t sign_y = sprite->flip_y ? -1 : 1;
  for (uint16_t y = 0; y < SPRITE_SIZE; y++) {
    for (uint16_t x = 0; x < SPRITE_SIZE; x++) {
      int16_t py = y + sprite_y;
      int16_t px = x + sprite_x;
      if (px < 0 || py < 0 || px >= DISPLAY_RES_X || py >= DISPLAY_RES_Y) {
        continue;
      }
      uint8_t tile_i =
          game_state.rom.sprite_atlas[tile_offset_y + y * sign_y]
                                     [tile_code + tile_offset_x + sign_x * x];
      uint8_t color_i = color_code * 4 + tile_i;
      uint32_t color = game_state.rom.color_palette[color_i];
      game_state.frame_buffer[py][px] = color;
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
  UpdateTexture(game_state.render_texture, game_state.frame_buffer);
  DrawTextureEx(game_state.render_texture, (Vector2){0, 0}, 0, PIXEL_SCALE,
                WHITE);
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
  bool8 is_horizontal = dir_is_horizontal(pacman->dir);
  pacman_sprite.tile_code = pacman_anim[is_horizontal ? 0 : 1][anim_tick];
  pacman_sprite.color_code = COLOR_PACMAN;
  pacman_sprite.flip_x = pacman->dir == DIR_LEFT;
  pacman_sprite.flip_y = pacman->dir == DIR_UP;

  int2_t sprite_pos = actor_to_sprite_pos(pacman->pos);

  draw_sprite(sprite_pos.x, sprite_pos.y, &pacman_sprite);
}

void update_pacman(Pacman *pacman) {
  if (IsKeyDown(KEY_A)) {
    pacman->dir = DIR_LEFT;
  } else if (IsKeyDown(KEY_D)) {
    pacman->dir = DIR_RIGHT;
  } else if (IsKeyDown(KEY_W)) {
    pacman->dir = DIR_UP;
  } else if (IsKeyDown(KEY_S)) {
    pacman->dir = DIR_DOWN;
  }

  int2_t ds = dir_to_vec(pacman->dir);
  pacman->pos.x += ds.x;
  pacman->pos.y += ds.y;

  int16_t left_bounds_x = -HALF_SPRITE_SIZE;
  int16_t right_bounds_x = DISPLAY_RES_X + HALF_SPRITE_SIZE;
  if (pacman->pos.x > right_bounds_x) {
    pacman->pos.x = left_bounds_x;
  } else if (pacman->pos.x < left_bounds_x) {
    pacman->pos.x = right_bounds_x;
  }
  pacman->pos.y =
      CLAMP(pacman->pos.y, HALF_SPRITE_SIZE, DISPLAY_RES_Y - HALF_SPRITE_SIZE);
}

int main(void) {
  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "pacman.c");

  SetTargetFPS(TARGET_FPS);

  game_state.render_texture =
      LoadTextureFromImage(GenImageColor(DISPLAY_RES_X, DISPLAY_RES_Y, BLACK));

  pm_init_rom(&game_state.rom);

  Pacman pacman = {0};
  pacman.dir = DIR_LEFT;

  while (!WindowShouldClose()) {
    update_pacman(&pacman);
    draw_pacman(&pacman);
    BeginDrawing();
    ClearBackground(BLACK);
    render_frame_buffer();
    EndDrawing();
    DrawFPS(0, 0);

    game_state.tick++;
  }

  CloseWindow();

  return 0;
}
