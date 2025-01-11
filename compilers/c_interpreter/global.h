#ifndef H_GLOBAL
#define H_GLOBAL

typedef struct ArenaAllocator ArenaAllocator;

typedef struct {
  ArenaAllocator *arena_alloc;
  ArenaAllocator *temp_alloc;
} GlobalContext;

GlobalContext *global_ctx();
#endif
