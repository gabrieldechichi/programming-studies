#ifndef H_GLOBAL
#define H_GLOBAL
#include "arena_allocator.c"

typedef struct {
  ArenaAllocator arena_alloc;
} GlobalContext;

static GlobalContext g_Context;

GlobalContext *global_ctx() { return &g_Context; }

int global_context_init() {
  g_Context = (GlobalContext){0};
  return arena_init(&g_Context.arena_alloc, 1024 * 1024);
}
#endif
