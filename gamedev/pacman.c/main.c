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
#define INSIDE_MAP_BOUNDS(x, y)                                                \
  ((x) >= 0 && (x) < DISPLAY_TILES_X && (y) >= 0 && (y) < DISPLAY_TILES_Y)

typedef struct pacman_tile_t {
  uint8_t tile_code;
  uint8_t color_code;
} pacman_tile_t;

typedef struct game_state_t {
  uint32_t tick;
  pacman_rom_t rom;
  pacman_tile_t tiles[DISPLAY_TILES_Y][DISPLAY_TILES_X];
  uint32_t frame_buffer[DISPLAY_RES_Y][DISPLAY_RES_X];
  tex2d_t render_texture;
} game_state_t;

typedef struct pacman_sprite_t {
  uint32_t tile_code;
  uint32_t color_code;
  bool8_t flip_x;
  bool8_t flip_y;
} pacman_sprite_t;

typedef struct int2_t {
  int16_t x;
  int16_t y;
} int2_t;

// note: low bit = 0 for horizontal, low bit = 1 for vertical
typedef enum { DIR_RIGHT, DIR_DOWN, DIR_LEFT, DIR_UP, NUM_DIRS } dir_t;

typedef struct pacman_t {
  int2_t pos;
  dir_t dir;
} pacman_t;

global game_state_t game_state = {};

color32_t u32_to_color(uint32_t color) {
  return (Color){
      (color >> 0) & 0xFF,
      (color >> 8) & 0xFF,
      (color >> 16) & 0xFF,
      (color >> 24) & 0xFF,
  };
}

int2_t dir_to_vec(dir_t dir) {
  local_persist const int2_t dir_map[NUM_DIRS] = {
      {+1, 0}, {0, +1}, {-1, 0}, {0, -1}};
  return dir_map[dir];
}

bool8_t dir_is_horizontal(dir_t dir) { return !(dir & 1); }

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

bool8_t equal_i2(int2_t v0, int2_t v1) {
  return (v0.x == v1.x) && (v0.y == v1.y);
}

bool8_t nearequal_i2(int2_t v0, int2_t v1, int16_t tolerance) {
  return (abs(v1.x - v0.x) <= tolerance) && (abs(v1.y - v0.y) <= tolerance);
}

// TILE MAP CODE
int2_t pixel_to_tile_coord(int2_t p) {
  return i2(p.x / TILE_SIZE, p.y / TILE_SIZE);
}
int2_t pixel_to_tile_center(int2_t p) {
  int2_t tile = pixel_to_tile_coord(p);
  return add_i2(tile, i2(TILE_SIZE / 2, TILE_SIZE / 2));
}

uint8_t tile_code_at(int2_t tile_coord) {
  return game_state.tiles[tile_coord.y][tile_coord.x].tile_code;
}

bool8_t is_blocking_tile(int2_t tile_pos) {
  if (!INSIDE_MAP_BOUNDS(tile_pos.x, tile_pos.y)) {
    return FALSE;
  }
  return tile_code_at(tile_pos) >= TILE_BLOCKING;
}

int2_t actor_to_sprite_pos(int2_t pos) {
  return i2(pos.x - HALF_SPRITE_SIZE, pos.y - HALF_SPRITE_SIZE);
}

bool8_t can_move(int2_t pos, dir_t wanted_dir) {
  int2_t move_dir = dir_to_vec(wanted_dir);
  int2_t move_amount = mul_i2(move_dir, TILE_SIZE / 2);
  move_amount = add_i2(move_amount, move_dir);
  // move_amount = sub_i2(move_amount, move_dir);
  int2_t next_edge_pos = add_i2(pos, move_amount);

  int2_t next_tile = pixel_to_tile_coord(next_edge_pos);
  if (is_blocking_tile(next_tile)) {
    return FALSE;
  }
  return TRUE;
}

void draw_tile_color(uint8_t tile_x, uint8_t tile_y, uint32_t color,
                     uint8_t tile_size) {
  uint16_t x = tile_x;
  uint16_t y = tile_y;

  for (uint16_t yy = 0; yy < tile_size; yy++) {
    for (uint16_t xx = 0; xx < tile_size; xx++) {
      uint16_t py = y + yy;
      uint16_t px = x + xx;
      game_state.frame_buffer[py][px] = color;
    }
  }
}

void draw_sprite(int16_t sprite_x, int16_t sprite_y,
                 const pacman_sprite_t *sprite) {
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
      uint32_t src_color = game_state.rom.color_palette[color_i];
      uint8_t alpha = (src_color >> 24) & 0xFF;

      if (alpha > 0) {
        game_state.frame_buffer[py][px] = src_color;
      }
    }
  }
}

void draw_tile(uint16_t tile_x, uint16_t tile_y, pacman_tile_t *tile) {
  uint32_t tile_code = tile->tile_code * TILE_SIZE;
  uint32_t color_code = tile->color_code;
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

    draw_tile_color(x, y, c, TILE_SIZE);
    x++;
    if (x >= DISPLAY_TILES_X) {
      x = 0;
      y++;
    }
  }
}

void draw_sprite_atlas() {
  uint16_t x = 0;
  uint16_t y = 0;
  for (size_t i = 0; i < NUM_SPRITES; i++) {
    pacman_sprite_t sprite = {};
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

void draw_tile_atlas() {
  uint16_t x = 0;
  uint16_t y = 0;
  for (size_t i = 0; i < NUM_TILES; i++) {
    pacman_tile_t tile = {};
    tile.tile_code = i;
    tile.color_code = COLOR_DOT;

    draw_tile(x, y, &tile);
    x += TILE_SIZE;
    if (x >= DISPLAY_RES_X) {
      x = 0;
      y += TILE_SIZE;
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

void draw_pacman(pacman_t *pacman) {
  local_persist const uint8_t pacman_anim[2][4] = {
      {44, 46, 48, 46}, // horizontal (needs flipx)
      {45, 47, 48, 47}  // vertical (needs flipy)
  };

  pacman_sprite_t pacman_sprite_t = {};
  uint8_t anim_tick = (game_state.tick / 2) % 4;
  bool8_t is_horizontal = dir_is_horizontal(pacman->dir);
  pacman_sprite_t.tile_code = pacman_anim[is_horizontal ? 0 : 1][anim_tick];
  pacman_sprite_t.color_code = COLOR_PACMAN;
  pacman_sprite_t.flip_x = pacman->dir == DIR_LEFT;
  pacman_sprite_t.flip_y = pacman->dir == DIR_UP;

  int2_t sprite_pos = actor_to_sprite_pos(pacman->pos);

  // draw_tile_color(sprite_pos.x, sprite_pos.y, 0xff0000ff, SPRITE_SIZE);

  draw_sprite(sprite_pos.x, sprite_pos.y, &pacman_sprite_t);
}

void draw_tiles() {
  for (uint8_t y = 0; y < DISPLAY_TILES_Y; ++y) {
    for (uint8_t x = 0; x < DISPLAY_TILES_X; ++x) {
      pacman_tile_t tile = game_state.tiles[y][x];
      uint16_t px = x * TILE_SIZE;
      uint16_t py = y * TILE_SIZE;
      draw_tile(px, py, &tile);
    }
  }
}

void update_pacman(pacman_t *pacman) {
  dir_t wanted_dir = pacman->dir;
  if (IsKeyDown(KEY_A)) {
    wanted_dir = DIR_LEFT;
  } else if (IsKeyDown(KEY_D)) {
    wanted_dir = DIR_RIGHT;
  } else if (IsKeyDown(KEY_W)) {
    wanted_dir = DIR_UP;
  } else if (IsKeyDown(KEY_S)) {
    wanted_dir = DIR_DOWN;
  }

  if (can_move(pacman->pos, wanted_dir)) {
    pacman->dir = wanted_dir;
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
    pacman->pos.y = CLAMP(pacman->pos.y, HALF_SPRITE_SIZE,
                          DISPLAY_RES_Y - HALF_SPRITE_SIZE);
  }
}

void init_level(void) {
  // clang-format off
    // decode the playfield from an ASCII map into tiles codes
    local_persist const char* tiles =
       //0123456789012345678901234567
        "0UUUUUUUUUUUU45UUUUUUUUUUUU1" // 3
        "L............rl............R" // 4
        "L.ebbf.ebbbf.rl.ebbbf.ebbf.R" // 5
        "LPr  l.r   l.rl.r   l.r  lPR" // 6
        "L.guuh.guuuh.gh.guuuh.guuh.R" // 7
        "L..........................R" // 8
        "L.ebbf.ef.ebbbbbbf.ef.ebbf.R" // 9
        "L.guuh.rl.guuyxuuh.rl.guuh.R" // 10
        "L......rl....rl....rl......R" // 11
        "2BBBBf.rzbbf rl ebbwl.eBBBB3" // 12
        "     L.rxuuh gh guuyl.R     " // 13
        "     L.rl          rl.R     " // 14
        "     L.rl mjs--tjn rl.R     " // 15
        "UUUUUh.gh i      q gh.gUUUUU" // 16
        "      .   i      q   .      " // 17
        "BBBBBf.ef i      q ef.eBBBBB" // 18
        "     L.rl okkkkkkp rl.R     " // 19
        "     L.rl          rl.R     " // 20
        "     L.rl ebbbbbbf rl.R     " // 21
        "0UUUUh.gh guuyxuuh gh.gUUUU1" // 22
        "L............rl............R" // 23
        "L.ebbf.ebbbf.rl.ebbbf.ebbf.R" // 24
        "L.guyl.guuuh.gh.guuuh.rxuh.R" // 25
        "LP..rl.......  .......rl..PR" // 26
        "6bf.rl.ef.ebbbbbbf.ef.rl.eb8" // 27
        "7uh.gh.rl.guuyxuuh.rl.gh.gu9" // 28
        "L......rl....rl....rl......R" // 29
        "L.ebbbbwzbbf.rl.ebbwzbbbbf.R" // 30
        "L.guuuuuuuuh.gh.guuuuuuuuh.R" // 31
        "L..........................R" // 32
        "2BBBBBBBBBBBBBBBBBBBBBBBBBB3"; // 33
       //0123456789012345678901234567
    uint8_t t[128];
    for (int i = 0; i < 128; i++) { t[i]=TILE_DOT; }
    t[' ']=0x40; t['0']=0xD1; t['1']=0xD0; t['2']=0xD5; t['3']=0xD4; t['4']=0xFB;
    t['5']=0xFA; t['6']=0xD7; t['7']=0xD9; t['8']=0xD6; t['9']=0xD8; t['U']=0xDB;
    t['L']=0xD3; t['R']=0xD2; t['B']=0xDC; t['b']=0xDF; t['e']=0xE7; t['f']=0xE6;
    t['g']=0xEB; t['h']=0xEA; t['l']=0xE8; t['r']=0xE9; t['u']=0xE5; t['w']=0xF5;
    t['x']=0xF2; t['y']=0xF3; t['z']=0xF4; t['m']=0xED; t['n']=0xEC; t['o']=0xEF;
    t['p']=0xEE; t['j']=0xDD; t['i']=0xD2; t['k']=0xDB; t['q']=0xD3; t['s']=0xF1;
    t['t']=0xF0; t['-']=TILE_DOOR; t['P']=TILE_PILL;

  // clang-format on
  for (int y = 3, i = 0; y <= 33; y++) {
    for (int x = 0; x < 28; x++, i++) {
      game_state.tiles[y][x] = (pacman_tile_t){t[tiles[i] & 127], COLOR_DOT};
    }
  }

  // door colors
  game_state.tiles[15][13].color_code = 0x18;
  game_state.tiles[15][14].color_code = 0x18;
}

int main(void) {
  memset(&game_state, 0, sizeof(game_state));

  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "pacman.c");

  SetTargetFPS(TARGET_FPS);

  game_state.render_texture =
      LoadTextureFromImage(GenImageColor(DISPLAY_RES_X, DISPLAY_RES_Y, BLACK));

  pm_init_rom(&game_state.rom);
  init_level();

  pacman_t pacman = {0};
  pacman.dir = DIR_LEFT;
  pacman.pos = i2(14 * 8, 26 * 8 + 4);

  while (!WindowShouldClose()) {
    update_pacman(&pacman);

    draw_tiles();
    draw_pacman(&pacman);
    // draw_tile_atlas();
    BeginDrawing();
    ClearBackground(BLACK);
    render_frame_buffer();
    EndDrawing();

    game_state.tick++;
  }

  CloseWindow();

  return 0;
}
