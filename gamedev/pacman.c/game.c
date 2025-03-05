#include "game.h"
#include "./common.h"
#include "rom.c"
#include "typedefs.h"
#include <_string.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// clang-format off
#define INSIDE_MAP_BOUNDS(x, y)                                                \
  ((x) >= 0 && (x) < DISPLAY_TILES_X && (y) >= 0 && (y) < DISPLAY_TILES_Y)

#define NUM_PILLS (4)              // number of energizer pills on playfield
#define NUM_DOTS (240) + NUM_PILLS // 240 small dots + 4 pills
#define NUM_VOICES 3            // number of sound voices
#define NUM_SOUNDS 3            // max number of sounds effects that can be active at a time
#define NUM_SAMPLES 128          // max number of audio samples in local sample buffer
// clang-format on

typedef struct int2_t {
  int16_t x;
  int16_t y;
} int2_t;

typedef struct pacman_tile_t {
  uint8_t tile_code;
  uint8_t color_code;
} pacman_tile_t;

typedef struct pacman_sprite_t {
  uint32 tile_code;
  uint32 color_code;
  bool flip_x;
  bool flip_y;
} pacman_sprite_t;

typedef enum {
  FRUIT_NONE,
  FRUIT_CHERRIES,
  FRUIT_STRAWBERRY,
  FRUIT_PEACH,
  FRUIT_APPLE,
  FRUIT_GRAPES,
  FRUIT_GALAXIAN,
  FRUIT_BELL,
  FRUIT_KEY,
  NUM_FRUITS
} fruit_opt_t;

typedef struct fruit_t {
  pacman_sprite_t sprite;
  uint16_t bonus_score;
  uint16_t despawn_ticks;
} fruit_t;

// clang-format off
const fruit_t fruits[NUM_FRUITS] = {
    {{0, 0, false, false}, 0, 0}, // FRUIT_NONE
    {{SPRITETILE_CHERRIES, COLOR_CHERRIES,false, false},10,6*60, },
    // { FRUIT_APPLE,      70,  2*60, },
    // { FRUIT_APPLE,      70,  5*60, },
    // { FRUIT_GRAPES,     100, 2*60, },
    // { FRUIT_GRAPES,     100, 2*60, },
    // { FRUIT_GALAXIAN,   200, 1*60, },
    // { FRUIT_GALAXIAN,   200, 5*60, },
    // { FRUIT_BELL,       300, 2*60, },
    // { FRUIT_BELL,       300, 1*60, },
    // { FRUIT_KEY,        500, 1*60, },
    // { FRUIT_KEY,        500, 3*60, },
    // { FRUIT_KEY,        500, 1*60, },
    // { FRUIT_KEY,        500, 1*60, },
    // { FRUIT_KEY,        500, 1,    },
    // { FRUIT_KEY,        500, 1*60, },
    // { FRUIT_KEY,        500, 1,    },
    // { FRUIT_KEY,        500, 1,    },
    // { FRUIT_KEY,        500, 1,    },
    {{SPRITETILE_STRAWBERRY, COLOR_STRAWBERRY,false, false}, 30,  5*60, },
    {{SPRITETILE_PEACH, COLOR_PEACH,false, false}, 50,  4*60, },
    {{SPRITETILE_APPLE, COLOR_APPLE,false, false}, 70,  2*60, },
    // {{SPRITETILE_GRAPES, COLOR_GRAPES,false, false}},
    // {{SPRITETILE_GALAXIAN, COLOR_GALAXIAN,false, false}},
    // {{SPRITETILE_BELL, COLOR_BELL,false, false}},
    // {{SPRITETILE_KEY, COLOR_KEY,false, false}}
};
// clang-format on

// note: low bit = 0 for horizontal, low bit = 1 for vertical
typedef enum { DIR_RIGHT, DIR_DOWN, DIR_LEFT, DIR_UP, NUM_DIRS } dir_t;

typedef enum { SOUND_DUMP, SOUND_PROCEDURAL } sound_opt_t;
typedef enum { SOUND_DEAD, SOUND_EATDOT_1, NUM_GAME_SOUNDS } sounds_t;

typedef void (*sound_func_t)(int sound_slot);

typedef struct sound_desc_t {
  sound_opt_t type;
  bool voice[3];
  union {
    struct {
      const uint32 *ptr;
      uint32 size;
    };
    sound_func_t sound_fn;
  };
} sound_desc_t;

const sound_desc_t sounds[NUM_GAME_SOUNDS];

typedef enum {
  SOUNDFLAG_VOICE0 = (1 << 0),
  SOUNDFLAG_VOICE1 = (1 << 1),
  SOUNDFLAG_VOICE2 = (1 << 2),
  NUM_SOUNDFLAGS = 3,
  SOUNDFLAG_ALL_VOICES = (1 << 0) | (1 << 1) | (1 << 2),
} soundflag_t;

typedef struct {
  uint32 cur_tick;
  sound_func_t func;
  uint32 num_ticks;
  uint32 stride; // number of uint32 values per tick (only for register dump
                 // effects)
  const uint32 *data; // 3 * num_ticks register dump values
  uint8_t flags;      // combination of soundflag_t (active voices)
} sound_t;

typedef struct {
  uint32 counter;   // 20-bit counter, top 5 bits are index into wavetable ROM
  uint32 frequency; // 20-bit frequency (added to counter at 96kHz)
  uint8_t waveform; // 3-bit waveform index
  uint8_t volume;   // 4-bit volume
  float sample_acc; // current float sample accumulator
  float sample_div; // current float sample divisor
} voice_t;

typedef struct pacman_t {
  int2_t pos;
  dir_t dir;
} pacman_t;

typedef struct game_state_t {
  // clock
  bool is_running;
  uint32 tick;

  // input
  union {
    struct {
      Game_InputButton a;
      Game_InputButton d;
      Game_InputButton w;
      Game_InputButton s;
      Game_InputButton space_bar;
    };
    Game_InputButton buttons[KEY_MAX];
  } input;

  pacman_t pacman;

  // score
  uint32 score;
  uint8_t num_dots_eaten;
  fruit_opt_t active_fruit;
  int2_t fruit_pos;
  uint32 fruit_despawn_tick;

  struct {
    voice_t voice[NUM_VOICES];
    sound_t sound[NUM_SOUNDS];
    int32_t voice_tick_accum;
    int32_t voice_tick_period_ns;
    int32_t sample_duration_ns;
    int32_t sample_accum;
    uint32 num_samples;
    float sample_buffer[NUM_SAMPLES];
  } audio;

  // rom
  pacman_rom_t rom;

  // tilemap
  pacman_tile_t tiles[DISPLAY_TILES_Y][DISPLAY_TILES_X];

  // rendering
  uint32 frame_buffer[DISPLAY_RES_Y][DISPLAY_RES_X];
} game_state_t;

global game_state_t game_state = {};

int2_t dir_to_vec(dir_t dir) {
  local_persist const int2_t dir_map[NUM_DIRS] = {
      {+1, 0}, {0, +1}, {-1, 0}, {0, -1}};
  return dir_map[dir];
}

bool dir_is_horizontal(dir_t dir) { return !(dir & 1); }

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

bool equal_i2(int2_t v0, int2_t v1) { return (v0.x == v1.x) && (v0.y == v1.y); }

bool nearequal_i2(int2_t v0, int2_t v1, int16_t tolerance) {
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

int2_t dist_to_tile_center(int2_t pos) {
  return i2((TILE_SIZE / 2) - pos.x % TILE_SIZE,
            (TILE_SIZE / 2) - pos.y % TILE_SIZE);
}

uint8_t tile_code_at(int2_t tile_coord) {
  return game_state.tiles[tile_coord.y][tile_coord.x].tile_code;
}

bool is_blocking_tile(int2_t tile_pos) {
  if (!INSIDE_MAP_BOUNDS(tile_pos.x, tile_pos.y)) {
    return false;
  }
  return tile_code_at(tile_pos) >= TILE_BLOCKING;
}

bool is_dot(int2_t tile_pos) { return tile_code_at(tile_pos) == TILE_DOT; }

bool is_pill(int2_t tile_pos) { return tile_code_at(tile_pos) == TILE_PILL; }

int2_t actor_to_sprite_pos(int2_t pos) {
  return i2(pos.x - HALF_SPRITE_SIZE, pos.y - HALF_SPRITE_SIZE);
}

bool can_move(int2_t pos, dir_t wanted_dir) {
  int2_t move_dir = dir_to_vec(wanted_dir);
  int2_t move_amount = mul_i2(move_dir, TILE_SIZE / 2);
  move_amount = add_i2(move_amount, move_dir);
  // move_amount = sub_i2(move_amount, move_dir);
  int2_t next_edge_pos = add_i2(pos, move_amount);

  int2_t next_tile = pixel_to_tile_coord(next_edge_pos);
  if (is_blocking_tile(next_tile)) {
    return false;
  }
  return true;
}

int2_t move(int2_t pos, dir_t dir) {
  int2_t ds = dir_to_vec(dir);
  pos = add_i2(pos, ds);
  int2_t dist_to_center = dist_to_tile_center(pos);
  if (ds.x != 0) {
    if (dist_to_center.y < 0) {
      pos.y--;
    } else if (dist_to_center.y > 0) {
      pos.y++;
    }
  } else if (ds.y != 0) {
    if (dist_to_center.x < 0) {
      pos.x--;
    } else if (dist_to_center.x > 0) {
      pos.x++;
    }
  }
  return pos;
}

// SOUND
void sound_start(int slot, const sound_desc_t *desc) {
  assert((slot >= 0) && (slot < NUM_SOUNDS));
  assert(desc);
  assert((desc->ptr && desc->size) || desc->sound_fn);

  sound_t *snd = &game_state.audio.sound[slot];
  *snd = (sound_t){0};
  int num_voices = 0;
  for (int i = 0; i < NUM_VOICES; i++) {
    if (desc->voice[i]) {
      snd->flags |= (1 << i);
      num_voices++;
    }
  }
  if (desc->sound_fn) {
    // procedural sounds only need a callback function
    snd->func = desc->sound_fn;
  } else {
    // assert(num_voices > 0);
    // assert((desc->size % (num_voices * sizeof(uint32))) == 0);
    snd->stride = num_voices;
    snd->num_ticks = desc->size / (snd->stride * sizeof(uint32));
    snd->data = desc->ptr;
  }
}

void sound_stop(int slot) {
  // silence the sound's output voices
  for (int i = 0; i < NUM_VOICES; i++) {
    if (game_state.audio.sound[slot].flags & (1 << i)) {
      game_state.audio.voice[i] = (voice_t){0};
    }
  }

  // clear the sound slot
  game_state.audio.sound[slot] = (sound_t){0};
}

void snd_voice_tick(void) {
  for (int i = 0; i < NUM_VOICES; i++) {
    voice_t *voice = &game_state.audio.voice[i];
    voice->counter += voice->frequency;
    /* lookup current 4-bit sample from the waveform number and the
        topmost 5 bits of the 20-bit sample counter
    */
    uint32 wave_index =
        ((voice->waveform << 5) | ((voice->counter >> 15) & 0x1F)) & 0xFF;
    int sample = (((int)(rom_wavetable[wave_index] & 0xF)) - 8) * voice->volume;
    voice->sample_acc +=
        (float)sample; // sample is (-8..+7 wavetable value) * 16 (volume)
    voice->sample_div += 128.0f;
  }
}

void snd_func_eatdot1(int slot) {
  const sound_t *snd = &game_state.audio.sound[slot];
  voice_t *voice = &game_state.audio.voice[2];
  if (snd->cur_tick == 0) {
    voice->volume = 12;
    voice->waveform = 2;
    voice->frequency = 0x1500;
  } else if (snd->cur_tick == 5) {
    sound_stop(slot);
  } else {
    voice->frequency -= 0x0300;
  }
}

void draw_tile_color(uint8_t tile_x, uint8_t tile_y, uint32 color,
                     uint8_t tile_width, uint8_t tile_height) {
  uint16_t x = tile_x;
  uint16_t y = tile_y;

  for (uint16_t yy = 0; yy < tile_height; yy++) {
    for (uint16_t xx = 0; xx < tile_width; xx++) {
      uint16_t py = y + yy;
      uint16_t px = x + xx;
      game_state.frame_buffer[py][px] = color;
    }
  }
}

void draw_sprite(int16_t sprite_x, int16_t sprite_y,
                 const pacman_sprite_t *sprite) {
  uint32 tile_code = sprite->tile_code * SPRITE_SIZE;
  uint32 color_code = sprite->color_code;

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
      uint32 src_color = game_state.rom.color_palette[color_i];
      uint8_t alpha = (src_color >> 24) & 0xFF;

      if (alpha > 0) {
        game_state.frame_buffer[py][px] = src_color;
      }
    }
  }
}

void draw_tile(uint16_t tile_x, uint16_t tile_y, pacman_tile_t *tile) {
  uint32 tile_code = tile->tile_code * TILE_SIZE;
  uint32 color_code = tile->color_code;
  for (uint16_t y = 0; y < TILE_SIZE; y++) {
    for (uint16_t x = 0; x < TILE_SIZE; x++) {
      uint8_t tile_i = game_state.rom.tile_atlas[y][tile_code + x];
      uint8_t color_i = color_code * 4 + tile_i;
      uint32 color = game_state.rom.color_palette[color_i];
      game_state.frame_buffer[y + tile_y][x + tile_x] = color;
    }
  }
}

void draw_color_palette() {
  uint16_t x = 0;
  uint16_t y = 0;
  for (size_t i = 0; i < ARRAY_SIZE(game_state.rom.color_palette); i++) {
    uint32 c = game_state.rom.color_palette[i];

    draw_tile_color(x, y, c, TILE_SIZE, TILE_SIZE);
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

char conv_char(char c) {
  // clang-format off
    switch (c) {
        case ' ':   c = 0x40; break;
        case '/':   c = 58; break;
        case '-':   c = 59; break;
        case '\"':  c = 38; break;
        case '!':   c = 'Z'+1; break;
        default: break;
    }
  // clang-format on
  return c;
}

void set_tile_char(int2_t tile_pos, uint8_t color_code, char chr) {
  if (INSIDE_MAP_BOUNDS(tile_pos.x, tile_pos.y)) {
    game_state.tiles[tile_pos.y][tile_pos.x] =
        (pacman_tile_t){conv_char(chr), color_code};
  }
}

void set_tile_text(int2_t tile_pos, uint8_t color_code, const char *text) {
  // assert(valid_tile_pos(tile_pos));
  uint8_t chr;
  while ((chr = (uint8_t)*text++)) {
    if (tile_pos.x < DISPLAY_TILES_X) {
      set_tile_char(tile_pos, color_code, chr);
      tile_pos.x++;
    } else {
      break;
    }
  }
}

void set_tile_score(int2_t tile_pos, uint8_t color_code, uint32 score) {
  set_tile_char(tile_pos, color_code, '0');
  tile_pos.x--;
  for (uint8_t digit = 0; digit < 8; digit++) {
    char ch = (score % 10) + '0';
    set_tile_char(tile_pos, color_code, ch);
    tile_pos.x--;
    score /= 10;
    if (score == 0) {
      break;
    }
  }
}

void draw_pacman() {
  pacman_t *pacman = &game_state.pacman;
  local_persist const uint8_t pacman_anim[2][4] = {
      {44, 46, 48, 46}, // horizontal (needs flipx)
      {45, 47, 48, 47}  // vertical (needs flipy)
  };

  pacman_sprite_t pacman_sprite_t = {};
  uint8_t anim_tick = (game_state.tick / 2) % 4;
  bool is_horizontal = dir_is_horizontal(pacman->dir);
  pacman_sprite_t.tile_code = pacman_anim[is_horizontal ? 0 : 1][anim_tick];
  pacman_sprite_t.color_code = COLOR_PACMAN;
  pacman_sprite_t.flip_x = pacman->dir == DIR_LEFT;
  pacman_sprite_t.flip_y = pacman->dir == DIR_UP;

  int2_t sprite_pos = actor_to_sprite_pos(pacman->pos);

  // int8_t extra_x = 0;
  // draw_tile_color(sprite_pos.x - extra_x / 2, sprite_pos.y, 0x550000ff,
  //                 SPRITE_SIZE + extra_x, SPRITE_SIZE);

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

void pacman_eat_dot_or_pill(int2_t tile_coords, bool is_pill) {
  game_state.tiles[tile_coords.y][tile_coords.x].tile_code = TILE_SPACE;
  game_state.score += (is_pill ? 5 : 1);

  game_state.num_dots_eaten++;
  if (game_state.num_dots_eaten >= NUM_DOTS) {
    game_state.is_running = false;
  } else if (game_state.num_dots_eaten == 10 ||
             game_state.num_dots_eaten == 170) {
    game_state.active_fruit = FRUIT_STRAWBERRY;
    fruit_t fruit = fruits[game_state.active_fruit];
    game_state.fruit_despawn_tick = game_state.tick + fruit.despawn_ticks;
  }

  sound_start(2, &sounds[SOUND_EATDOT_1]);
}

void update_pacman() {
  pacman_t *pacman = &game_state.pacman;
  dir_t wanted_dir = pacman->dir;

  if (game_state.input.a.is_pressed) {
    wanted_dir = DIR_LEFT;
  } else if (game_state.input.d.is_pressed) {
    wanted_dir = DIR_RIGHT;
  } else if (game_state.input.w.is_pressed) {
    wanted_dir = DIR_UP;
  } else if (game_state.input.s.is_pressed) {
    wanted_dir = DIR_DOWN;
  }

  if (can_move(pacman->pos, wanted_dir)) {
    pacman->dir = wanted_dir;
  }

  if (can_move(pacman->pos, pacman->dir)) {
    pacman->pos = move(pacman->pos, pacman->dir);

    // check bounds
    int16_t left_bounds_x = -HALF_SPRITE_SIZE;
    int16_t right_bounds_x = DISPLAY_RES_X + HALF_SPRITE_SIZE;
    if (pacman->pos.x > right_bounds_x) {
      pacman->pos.x = left_bounds_x;
    } else if (pacman->pos.x < left_bounds_x) {
      pacman->pos.x = right_bounds_x;
    }
    pacman->pos.y = CLAMP(pacman->pos.y, HALF_SPRITE_SIZE,
                          DISPLAY_RES_Y - HALF_SPRITE_SIZE);

    int2_t tile_coords = pixel_to_tile_coord(pacman->pos);
    if (is_dot(tile_coords)) {
      pacman_eat_dot_or_pill(tile_coords, false);
    } else if (is_pill(tile_coords)) {
      pacman_eat_dot_or_pill(tile_coords, true);
    }

    if (game_state.active_fruit) {
      int2_t fruit_coords = pixel_to_tile_coord(game_state.fruit_pos);
      // hack(Gabriel): we offset the fruit graphically which moves it to the
      // wrong tile pos add support for offseting sprites
      fruit_coords.y++;
      int2_t pacman_coords = pixel_to_tile_coord(pacman->pos);
      if (equal_i2(pacman_coords, fruit_coords)) {
        fruit_t fruit = fruits[game_state.active_fruit];
        game_state.score += fruit.bonus_score;
        game_state.active_fruit = FRUIT_NONE;
      }
    }
  }
}

void update_fruits() {
  if (game_state.active_fruit) {
    if (game_state.tick >= game_state.fruit_despawn_tick) {
      game_state.active_fruit = FRUIT_NONE;
    }
  }
}

void draw_fruits() {
  if (game_state.active_fruit) {
    fruit_t fruit = fruits[game_state.active_fruit];
    draw_sprite(game_state.fruit_pos.x, game_state.fruit_pos.y, &fruit.sprite);
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

// clang-format off
const sound_desc_t sounds[NUM_GAME_SOUNDS] = {
    { .type = SOUND_DUMP, .ptr = snd_dump_dead, .size = sizeof(snd_dump_dead) },
    { .type = SOUND_PROCEDURAL, .sound_fn = snd_func_eatdot1, .voice = { false, false, true } },
};
// clang-format on

#define VOLUME 0.5
#define SINE_FREQUENCY 256
#define SINE_TIME_STEP (((2.0 * PI) * SINE_FREQUENCY) / AUDIO_SAMPLE_RATE)

void debug_audio_sine_wave(Game_SoundBuffer *sound_buffer, bool flag) {
  local_persist float time = PI / 2;
  float time_step = (((2.0 * PI) * SINE_FREQUENCY) / sound_buffer->sample_rate);

  for (int i = 0; i < sound_buffer->sample_count; i++) {
    float sine = sinf(time);
    if (flag) {
      if (sine < 0) {
        sine = 0;
      }
    }
    sound_buffer->samples[i] = sine * VOLUME;
    time += time_step;
  }
  sound_buffer->write_count = sound_buffer->sample_count;
}

void handle_keydown(Game_InputButton *button) {
  bool was_pressed = button->is_pressed;
  button->is_pressed = true;
  button->pressed_this_frame = !was_pressed;
  button->released_this_frame = false;
}

void handle_keyup(Game_InputButton *button) {
  bool was_pressed = button->is_pressed;
  button->is_pressed = false;
  button->pressed_this_frame = false;
  button->released_this_frame = was_pressed;
}

void process_platform_input_events(const Game_InputEvents *input) {
  for (uint8 i = 0; i < input->len; i++) {
    Game_InputEvent event = input->events[i];
    switch (event.type) {
    case EVENT_KEYDOWN: {
      for (uint8 key_index = 0; key_index < KEY_MAX; key_index++) {
        if (event.key.type == key_index) {
          handle_keydown(&game_state.input.buttons[key_index]);
        }
      }
      break;
    }
    case EVENT_KEYUP: {
      for (uint8 key_index = 0; key_index < KEY_MAX; key_index++) {
        if (event.key.type == key_index) {
          handle_keyup(&game_state.input.buttons[key_index]);
        }
      }
      break;
    }
    }
  }
}

export GAME_INIT(game_init) {
  UNUSED(memory);
  memset(&game_state, 0, sizeof(game_state));
  game_state.is_running = true;
  game_state.fruit_pos = i2(13 * TILE_SIZE, 20 * TILE_SIZE - TILE_SIZE / 2);

  pm_init_rom(&game_state.rom);
  init_level();

  game_state.pacman.dir = DIR_LEFT;
  game_state.pacman.pos = i2(14 * 8, 26 * 8 + 4);
}

export GAME_UPDATE_AND_RENDER(game_update_and_render) {
  UNUSED(memory);
  local_persist bool flag = false;

  process_platform_input_events(input);

  // update sound
  for (int i = 0; i < NUM_SOUNDS; i++) {
    sound_t sound = game_state.audio.sound[i];
    if (sound.func) {
      sound.func(i);
    }
  }

  // audio
  {
    if (game_state.input.space_bar.pressed_this_frame) {
      flag = !flag;
      sound_buffer->clear_buffer = true;
    }
    if (flag) {
      debug_audio_sine_wave(sound_buffer, flag);
    }

  }

  update_fruits();
  update_pacman();

  set_tile_score(i2(6, 1), COLOR_DEFAULT, game_state.score);

  draw_tiles();
  draw_pacman();
  draw_fruits();

  // clear frame buffer
  memset(screen_buffer->pixels, 0,
         sizeof(uint32) * screen_buffer->width * screen_buffer->height);
  for (uint16 y = 0; y < screen_buffer->height; y++) {
    for (uint16 x = 0; x < screen_buffer->width; x++) {
      uint32 i = y * screen_buffer->width + x;
      screen_buffer->pixels[i] = game_state.frame_buffer[y][x];
    }
  }

  // clear input
  for (uint8 key_index = 0; key_index < KEY_MAX; key_index++) {
    game_state.input.buttons[key_index].pressed_this_frame = false;
    game_state.input.buttons[key_index].released_this_frame = false;
  }

  game_state.tick++;
}
