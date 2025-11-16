#ifndef H_GAME
#define H_GAME

#include "context.h"
#include "input.h"
#include "lib/typedefs.h"

typedef struct {
  f32 now;
  f32 dt;
} GameTime;

typedef struct {
  u32 width;
  u32 height;
} GameCanvas;

typedef struct {
  GameTime time;
  GameCanvas canvas;
  GameInputEvents input_events;

  void *permanent_memory;
  size_t pernament_memory_size;

  void *temporary_memory;
  size_t temporary_memory_size;
} GameMemory;

void game_init(GameMemory *memory);
void game_update_and_render(GameMemory *memory);

extern GameContext *get_global_ctx();
void *global_alloc_temp(size_t size) {
  GameContext *ctx = get_global_ctx();
  debug_assert(ctx);
  if (!ctx) {
    return NULL;
  }
  return ALLOC_ARRAY(&ctx->temp_allocator, u8, size);
}

void *global_realloc_temp(void *ptr, size_t size) {
  GameContext *ctx = get_global_ctx();
  debug_assert(ctx);
  if (!ctx) {
    return NULL;
  }
  return REALLOC_ARRAY(&ctx->temp_allocator, ptr, u8, size);
}

#endif
