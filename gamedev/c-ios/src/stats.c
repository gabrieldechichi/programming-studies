#include "stats.h"
#include "lib/array.h"
#include "lib/memory.h"

void game_stats_update(GameContext *ctx, GameStats *stats, f32 dt) {
  stats->dt_buffer[stats->dt_idx] = dt;
  stats->dt_idx = (stats->dt_idx + 1) % ARRAY_SIZE(stats->dt_buffer);

  stats->dt_avg =
      arr_sum_raw(stats->dt_buffer, ARRAY_SIZE(stats->dt_buffer), f32);
  u32 array_size = ARRAY_SIZE(stats->dt_buffer);
  stats->dt_avg /= (f32)array_size;

  stats->temp_memory_used = ALLOC_COMMITED_SIZE(&ctx->temp_allocator);
  stats->temp_memory_total = ALLOC_CAPACITY(&ctx->temp_allocator);

  stats->memory_used = ALLOC_COMMITED_SIZE(&ctx->allocator);
  stats->memory_total = ALLOC_CAPACITY(&ctx->allocator);
}
