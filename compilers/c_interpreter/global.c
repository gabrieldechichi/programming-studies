#ifndef H_GLOBAL
#define H_GLOBAL
#include "arena_allocator.c"

typedef struct {
  ArenaAllocator arena_alloc;
  ArenaAllocator temp_alloc;
} GlobalContext;

static GlobalContext g_Context;

GlobalContext *global_ctx() { return &g_Context; }

int global_context_init() {
  g_Context = (GlobalContext){0};
  int err = arena_init(&g_Context.arena_alloc, 1024 * 1024);
  if (err) {
    return err;
  }
  err = arena_init(&g_Context.temp_alloc, 1024 * 1024);

  return err;
}

void *gd_alloc(size_t size) {
  return arena_alloc(&global_ctx()->arena_alloc, size);
}
void *gd_realloc(void *ptr, size_t size) {
  return arena_realloc(&global_ctx()->arena_alloc, ptr, size);
}

void *gd_temp_alloc(size_t size) {
  return arena_alloc(&global_ctx()->temp_alloc, size);
}
void *gd_temp_realloc(void *ptr, size_t size) {
  return arena_realloc(&global_ctx()->temp_alloc, ptr, size);
}
void gd_temp_free() { arena_free_all(&global_ctx()->temp_alloc); }

#define GD_ARENA_ALLOC_T(type) (type *)gd_alloc(sizeof(type))
#define GD_ARENA_ALLOC_ARRAY(type,size) (type *)gd_alloc(sizeof(type)*size)
#define GD_TEMP_ALLOC_T(type) (type *)gd_temp_alloc(sizeof(type))
#define GD_TEMP_ALLOC_ARRAY(type, size)                                        \
  (type *)gd_temp_alloc(sizeof(type) * size)
#define GD_TEMP_FREEALL gd_temp_free
#endif
