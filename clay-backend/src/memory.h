#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>
#include <stdint.h>

#if defined(__wasm__) && defined(__clang__)
// Use WASM builtins - compile directly to memory.copy and memory.fill
#define memcpy(dest, src, n) __builtin_memcpy(dest, src, n)
#define memset(dest, val, n) __builtin_memset(dest, val, n)
#define memmove(dest, src, n) __builtin_memmove(dest, src, n)
#else
#endif

#ifndef H_MEMORY
#define H_MEMORY

#include "typedefs.h"

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

/*
    ArenaAllocator - bump allocator that allocates from a fixed buffer

    .buffer: pointer to memory buffer
    .capacity: total size of buffer in bytes
    .offset: current allocation offset (grows with each allocation)
*/
typedef struct {
  uint8 *buffer;
  size_t capacity;
  size_t offset;
} ArenaAllocator;

/* returns remaining free space in arena */
size_t arena_free_size(ArenaAllocator *arena);

/* create arena allocator from existing buffer */
ArenaAllocator arena_from_buffer(uint8 *buffer, size_t capacity);

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
    PoolAllocator - fixed-size chunk allocator with free-list

    Allocates fixed-size chunks from a buffer. Supports individual free
   operations. All chunks are same size, making allocation/deallocation O(1).
    Use when you need many same-sized allocations that can be freed
   individually.
*/
typedef struct PoolFreeNode {
  struct PoolFreeNode *next;
} PoolFreeNode;

typedef struct PoolAllocator {
  uint8 *buffer;
  size_t capacity;
  size_t chunk_size;
  size_t chunk_count;
  size_t allocated_count;
  PoolFreeNode *head;
} PoolAllocator;

/* create pool allocator from buffer with fixed chunk_size */
PoolAllocator pool_from_buffer(uint8 *buffer, size_t capacity,
                               size_t chunk_size);

/* allocate one chunk from pool */
void *pool_alloc(PoolAllocator *pool);

/* free one chunk back to pool */
void pool_free(PoolAllocator *pool, void *ptr);

/* free all chunks at once */
void pool_free_all(PoolAllocator *pool);

/* returns total free space in pool */
size_t pool_free_size(PoolAllocator *pool);

/* returns total allocated space in pool */
size_t pool_allocated_size(PoolAllocator *pool);

/*
    Allocator - generic allocator interface with function pointers

    Wraps any allocator type (arena, pool, etc) with uniform interface.
    Use ALLOC/ALLOC_ARRAY macros instead of calling function pointers directly.
*/
typedef struct {
  void *(*alloc_alloc)(void *ctx, size_t size, size_t align);
  void *(*alloc_realloc)(void *ctx, void *ptr, size_t size);
  void (*alloc_reset)(void *ctx);
  void (*alloc_destroy)(void *ctx);
  void (*alloc_free)(void *ctx, void *ptr);
  size_t (*alloc_capacity)(void *ctx);
  size_t (*alloc_commited_size)(void *ctx);
  size_t (*alloc_free_size)(void *ctx);
  void *ctx;
} Allocator;

/* wrap ArenaAllocator in generic Allocator interface */
Allocator make_arena_allocator(ArenaAllocator *arena);

/* wrap PoolAllocator in generic Allocator interface */
Allocator make_pool_allocator(PoolAllocator *pool);

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

#endif // MEMORY_H
