#ifndef H_MEMORY
#define H_MEMORY

#include "assert.h"
#include "common.h"
#include "fmt.h"
#include "typedefs.h"
#include <string.h>

#ifndef DEFAULT_ALIGNMENT
#define DEFAULT_ALIGNMENT (2 * sizeof(void *))
#endif

#ifdef GAME_DEBUG
#define memcpy_safe(to, from, len)                                             \
  do {                                                                         \
    assert_msg((from) != NULL, "from pointer can't be NULL");                  \
    assert_msg((to) != NULL, "to pointer can't be NULL");                      \
    memcpy((to), (from), (len));                                               \
  } while (0)
#else
#define memcpy_safe(to, from, len) memcpy(to, from, len)
#endif

typedef struct {
  uint8 *buffer;
  size_t capacity;
  size_t offset;
} ArenaAllocator;

force_inline size_t arena_free_size(ArenaAllocator *arena) {
  return arena->capacity - arena->offset;
}

ArenaAllocator arena_from_buffer(uint8 *buffer, size_t capacity) {
  return cast(ArenaAllocator){
      .buffer = buffer, .capacity = capacity, .offset = 0};
}

b32 sub_arena_from_arena(ArenaAllocator *a, size_t capacity,
                         ArenaAllocator *sub_arena) {
  assert(a->buffer);
  assert(a->capacity);
  size_t align = DEFAULT_ALIGNMENT;
  uintptr curr_ptr = (uintptr)a->buffer + (uintptr)a->offset;
  uintptr offset = align_forward(curr_ptr, align);
  offset -= (uintptr)a->buffer;

  if (offset + capacity > a->capacity) {
    debug_assert_msg(false,
                     "Failed to allocate memory. Request % kb, Remaining % kb",
                     FMT_UINT(BYTES_TO_KB(capacity)),
                     FMT_UINT(BYTES_TO_KB(arena_free_size(a))));
    return false;
  }

  void *ptr = &a->buffer[offset];
  a->offset = offset + capacity;

  *sub_arena = arena_from_buffer(ptr, capacity);

  return true;
}

force_inline size_t arena_committed_size(ArenaAllocator *arena) {
  return arena->offset;
}

void *arena_alloc_align(ArenaAllocator *a, size_t size, size_t align) {
  assert(a->buffer);
  assert(a->capacity);
  uintptr curr_ptr = (uintptr)a->buffer + (uintptr)a->offset;
  uintptr offset = align_forward(curr_ptr, align);
  offset -= (uintptr)a->buffer;

  if (offset + size <= a->capacity) {
    void *ptr = &a->buffer[offset];
    a->offset = offset + size;

    memset(ptr, 0, size);
    return ptr;
  }

  // couldn't allocate memory
  debug_assert_msg(
      false, "Failed to allocate memory. Request % kb, Remaining % kb",
      FMT_UINT(BYTES_TO_KB(size)), FMT_UINT(BYTES_TO_KB(arena_free_size(a))));
  return NULL;
}

// Because C doesn't have default parameters
void *arena_alloc(ArenaAllocator *a, size_t size) {
  return arena_alloc_align(a, size, DEFAULT_ALIGNMENT);
}

void *arena_realloc(ArenaAllocator *a, void *ptr, size_t size) {
  if (!ptr) {
    return arena_alloc(a, size);
  }
  uintptr ptr_offset = (uintptr)ptr - (uintptr)a->buffer;
  debug_assert_msg(
      ptr_offset < a->offset,
      "invalid pointer (%), outside the bounds of the arena (%, %)",
      FMT_UINT((uintptr)ptr), FMT_UINT((uintptr)a->buffer),
      FMT_INT((int)a->offset));
  if (ptr_offset >= a->offset) {
    return NULL;
  }

  void *new_ptr = arena_alloc(a, size);
  if (!new_ptr) {
    return NULL; // Insufficient space
  }

  size_t copy_size = a->offset - ptr_offset;
  // only copy up to the size that can fit in the new block
  if (copy_size > size) {
    copy_size = size;
  }
  memcpy(new_ptr, ptr, copy_size);
  return new_ptr;
}

void arena_reset(ArenaAllocator *arena) { arena->offset = 0; }

void arena_destroy(ArenaAllocator *arena) {
  arena->buffer = NULL;
  arena->capacity = 0;
  arena->offset = 0;
}

typedef struct {
  void *(*alloc_alloc)(void *ctx, size_t size, size_t align);
  void *(*alloc_realloc)(void *ctx, void *ptr, size_t size);
  void (*alloc_reset)(void *ctx);
  void (*alloc_destroy)(void *ctx);
  size_t (*alloc_capacity)(void *ctx);
  size_t (*alloc_commited_size)(void *ctx);
  size_t (*alloc_free_size)(void *ctx);
  void *ctx;
} Allocator;

void *arena_alloc_impl(void *ctx, size_t size, size_t align) {
  return arena_alloc_align((ArenaAllocator *)ctx, size, align);
}

void *arena_realloc_impl(void *ctx, void *ptr, size_t size) {
  return arena_realloc((ArenaAllocator *)ctx, ptr, size);
}

void arena_reset_impl(void *ctx) { arena_reset((ArenaAllocator *)ctx); }
size_t arena_capacity_impl(void *ctx) {
  return ((ArenaAllocator *)ctx)->capacity;
}
size_t arena_commited_size_impl(void *ctx) {
  return arena_committed_size((ArenaAllocator *)ctx);
}
size_t arena_free_size_impl(void *ctx) {
  return arena_free_size((ArenaAllocator *)ctx);
}

void arena_destroy_impl(void *ctx) { arena_destroy((ArenaAllocator *)ctx); }

Allocator make_arena_allocator(ArenaAllocator *arena) {
  return (Allocator){.alloc_alloc = arena_alloc_impl,
                     .alloc_realloc = arena_realloc_impl,
                     .alloc_reset = arena_reset_impl,
                     .alloc_capacity = arena_capacity_impl,
                     .alloc_commited_size = arena_commited_size_impl,
                     .alloc_free_size = arena_free_size_impl,
                     .alloc_destroy = arena_destroy_impl,
                     .ctx = arena};
}

#define ARENA_ALLOC(arena, type) cast(type *) arena_alloc(arena, sizeof(type))
#define ARENA_ALLOC_ARRAY(arena, type, len)                                    \
  cast(type *) arena_alloc(arena, sizeof(type) * len)

#define ALLOC(allocator, type)                                                 \
  (cast(type *)(allocator)->alloc_alloc((allocator)->ctx, sizeof(type),              \
                                  DEFAULT_ALIGNMENT))
#define ALLOC_ARRAY(allocator, type, len)                                      \
  (cast(type *)(allocator)->alloc_alloc((allocator)->ctx, sizeof(type) * len,        \
                                  DEFAULT_ALIGNMENT))

#define REALLOC(allocator, ptr, type)                                          \
  (cast(type *)(allocator)->alloc_realloc((allocator)->ctx, (ptr), sizeof(type)))

#define REALLOC_ARRAY(allocator, ptr, type, len)                               \
  (cast(type *)(allocator)->alloc_realloc((allocator)->ctx, ptr, sizeof(type) * len))

#define ALLOC_RESET(allocator) (allocator)->alloc_reset((allocator)->ctx)
#define ALLOC_CAPACITY(allocator) ((allocator)->alloc_capacity((allocator)->ctx))
#define ALLOC_COMMITED_SIZE(allocator)                                         \
  ((allocator)->alloc_commited_size((allocator)->ctx))
#define ALLOC_FREE_SIZE(allocator) ((allocator)->alloc_free_size((allocator)->ctx))
#define ALLOC_DESTROY(allocator) (allocator)->alloc_destroy((allocator)->ctx)

#endif
