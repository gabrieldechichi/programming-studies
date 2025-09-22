#ifndef H_STATS
#define H_STATS

#include "game.h"
#include "lib/typedefs.h"

typedef struct {
  f32 dt_buffer[20];
  u32 dt_idx;
  f32 dt_avg;

  u32 temp_memory_used;
  u32 temp_memory_total;

  u32 memory_used;
  u32 memory_total;
} GameStats;

void game_stats_update(GameContext *ctx, GameStats *stats, f32 dt);

#endif
