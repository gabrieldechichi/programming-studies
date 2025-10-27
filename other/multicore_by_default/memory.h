#ifndef H_MEMORY
#define H_MEMORY

#include "assert.h"
#include "typedefs.h"
#include <string.h>

#ifndef DEFAULT_ALIGNMENT
#define DEFAULT_ALIGNMENT (2 * sizeof(void *))
#endif

#ifdef DEBUG
#define memcpy_safe(to, from, len)                                             \
  do {                                                                         \
    assert_msg((from) != NULL, "from pointer can't be NULL");                  \
    assert_msg((to) != NULL, "to pointer can't be NULL");                      \
    memcpy((to), (from), (len));                                               \
  } while (0)
#else
#define memcpy_safe(to, from, len) memcpy(to, from, len)
#endif

force_inline b32 is_power_of_two(uintptr x) { return (x & (x - 1)) == 0; }

force_inline uintptr align_forward(uintptr ptr, size_t align) {
  uintptr p, a, modulo;

  assert(is_power_of_two(align));

  p = ptr;
  a = (uintptr) align;
  // Same as (p % a) but faster as 'a' is a power of two
  modulo = p & (a - 1);

  if (modulo != 0) {
    // If 'p' address is not aligned, push the address to the
    // next value which is aligned
    p += a - modulo;
  }
  return p;
}

/*
    ArenaAllocator - bump allocator that allocates from a fixed buffer

    .buffer: pointer to memory buffer
    .capacity: total size of buffer in bytes
    .offset: current allocation offset (grows with each allocation)
*/
typedef struct {
  u8 *buffer;
  size_t capacity;
  size_t offset;
} ArenaAllocator;

/* returns remaining free space in arena */
size_t arena_free_size(ArenaAllocator *arena);

/* create arena allocator from existing buffer */
ArenaAllocator arena_from_buffer(u8 *buffer, size_t capacity);

/* allocate sub-arena from parent arena, returns false if insufficient space */
b32 sub_arena_from_arena(ArenaAllocator *a, size_t capacity,
                         ArenaAllocator *sub_arena);

/* returns total bytes allocated so far (arena offset) */
size_t arena_committed_size(ArenaAllocator *arena);

/* allocate size bytes with custom alignment */
void *arena_alloc_align(ArenaAllocator *a, size_t size, size_t align);

/* allocate size bytes with default alignment */
void *arena_alloc(ArenaAllocator *a, size_t size);

/* grow previous allocation, returns new pointer (may move memory) */
void *arena_realloc(ArenaAllocator *a, void *ptr, size_t size);

/* reset arena offset to 0, invalidates all previous allocations */
void arena_reset(ArenaAllocator *arena);

/* destroy arena (currently no-op, arena doesn't own buffer) */
void arena_destroy(ArenaAllocator *arena);

/*
    Direct arena allocation macros (use when you have ArenaAllocator*)

    ARENA_ALLOC(arena, type) - allocate single instance of type
    ARENA_ALLOC_ARRAY(arena, type, len) - allocate array of type
*/
#define ARENA_ALLOC(arena, type) cast(type *) arena_alloc(arena, sizeof(type))
#define ARENA_ALLOC_ARRAY(arena, type, len)                                    \
  cast(type *) arena_alloc(arena, sizeof(type) * len)

/*
    Generic allocator macros (use when you have Allocator*)

    ALLOC(allocator, type) - allocate single instance
    ALLOC_ARRAY(allocator, type, len) - allocate array
    REALLOC(allocator, ptr, type) - grow existing allocation
    ALLOC_RESET(allocator) - reset allocator (clear all allocations)
    ALLOC_CAPACITY(allocator) - get total capacity
    ALLOC_COMMITED_SIZE(allocator) - get bytes allocated
    ALLOC_FREE_SIZE(allocator) - get bytes remaining
*/
#define ALLOC(allocator, type)                                                 \
  (cast(type *)(allocator)->alloc_alloc((allocator)->ctx, sizeof(type),        \
                                        DEFAULT_ALIGNMENT))
#define ALLOC_ARRAY(allocator, type, len)                                      \
  (cast(type *)(allocator)->alloc_alloc((allocator)->ctx, sizeof(type) * len,  \
                                        DEFAULT_ALIGNMENT))

#define REALLOC(allocator, ptr, type)                                          \
  (cast(type *)(allocator)->alloc_realloc((allocator)->ctx, (ptr),             \
                                          sizeof(type)))

#define REALLOC_ARRAY(allocator, ptr, type, len)                               \
  (cast(type *)(allocator)->alloc_realloc((allocator)->ctx, ptr,               \
                                          sizeof(type) * len))

#define ALLOC_RESET(allocator) (allocator)->alloc_reset((allocator)->ctx)
#define ALLOC_CAPACITY(allocator)                                              \
  ((allocator)->alloc_capacity((allocator)->ctx))
#define ALLOC_COMMITED_SIZE(allocator)                                         \
  ((allocator)->alloc_commited_size((allocator)->ctx))
#define ALLOC_FREE_SIZE(allocator)                                             \
  ((allocator)->alloc_free_size((allocator)->ctx))
#define ALLOC_DESTROY(allocator) (allocator)->alloc_destroy((allocator)->ctx)

#define ALLOC_ALIGNED(allocator, size, align)                                  \
  ((allocator)->alloc_alloc((allocator)->ctx, (size), (align)))

#endif
