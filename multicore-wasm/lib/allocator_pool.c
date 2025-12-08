#include "lib/memory.h"
#include "lib/common.h"
#include "lib/string.h"

internal size_t pool_align_chunk_size(size_t chunk_size) {
  size_t min_size = sizeof(PoolFreeNode);
  if (chunk_size < min_size) {
    chunk_size = min_size;
  }

  size_t align = DEFAULT_ALIGNMENT;
  size_t aligned = (chunk_size + align - 1) & ~(align - 1);
  return aligned;
}

PoolAllocator pool_from_buffer(u8 *buffer, size_t capacity,
                                               size_t chunk_size) {
  assert(buffer);
  assert(capacity > 0);
  assert(chunk_size > 0);

  chunk_size = pool_align_chunk_size(chunk_size);

  size_t chunk_count = capacity / chunk_size;
  assert_msg(chunk_count > 0, "Buffer too small for even one chunk");

  PoolAllocator pool = {0};
  pool.buffer = buffer;
  pool.capacity = chunk_count * chunk_size;
  pool.chunk_size = chunk_size;
  pool.chunk_count = chunk_count;
  pool.allocated_count = 0;
  pool.head = NULL;

  for (size_t i = 0; i < chunk_count; i++) {
    void *chunk_ptr = buffer + (i * chunk_size);
    PoolFreeNode *node = (PoolFreeNode *)chunk_ptr;
    node->next = pool.head;
    pool.head = node;
  }

  return pool;
}

void *pool_alloc(PoolAllocator *pool) {
  assert(pool);

  if (!pool->head) {
    debug_assert_msg(false, "Pool allocator out of memory. All % chunks allocated",
                     FMT_UINT(pool->chunk_count));
    return NULL;
  }

  PoolFreeNode *node = pool->head;
  pool->head = pool->head->next;
  pool->allocated_count++;

#ifdef DEBUG
  memset(node, 0x0, pool->chunk_size);
#endif

  return node;
}

void pool_free(PoolAllocator *pool, void *ptr) {
  assert(pool);

  if (!ptr) {
    return;
  }

  uintptr ptr_addr = (uintptr)ptr;
  uintptr buffer_addr = (uintptr)pool->buffer;
  uintptr buffer_end = buffer_addr + pool->capacity;

  debug_assert_msg(ptr_addr >= buffer_addr && ptr_addr < buffer_end,
                   "Pointer outside pool bounds");
  debug_assert_msg((ptr_addr - buffer_addr) % pool->chunk_size == 0,
                   "Pointer not aligned to chunk boundary");

  PoolFreeNode *node = (PoolFreeNode *)ptr;
  node->next = pool->head;
  pool->head = node;
  pool->allocated_count--;
}

void pool_free_all(PoolAllocator *pool) {
  assert(pool);

  pool->head = NULL;
  pool->allocated_count = 0;

  for (size_t i = 0; i < pool->chunk_count; i++) {
    void *chunk_ptr = pool->buffer + (i * pool->chunk_size);
    PoolFreeNode *node = (PoolFreeNode *)chunk_ptr;
    node->next = pool->head;
    pool->head = node;
  }
}

size_t pool_free_size(PoolAllocator *pool) {
  assert(pool);
  return (pool->chunk_count - pool->allocated_count) * pool->chunk_size;
}

size_t pool_allocated_size(PoolAllocator *pool) {
  assert(pool);
  return pool->allocated_count * pool->chunk_size;
}

internal void *pool_alloc_impl(void *ctx, size_t size, size_t align) {
  UNUSED(align);
  PoolAllocator *pool = (PoolAllocator *)ctx;

  debug_assert_msg(size <= pool->chunk_size,
                   "Requested size % exceeds pool chunk size %",
                   FMT_UINT(size), FMT_UINT(pool->chunk_size));

  return pool_alloc(pool);
}

internal void *pool_realloc_impl(void *ctx, void *ptr, size_t size) {
  UNUSED(ptr);
  UNUSED(size);
  PoolAllocator *pool = (PoolAllocator *)ctx;

  debug_assert_msg(false, "Pool allocator does not support realloc");
  UNUSED(pool);
  return NULL;
}

internal void pool_reset_impl(void *ctx) {
  pool_free_all((PoolAllocator *)ctx);
}

internal void pool_free_impl(void *ctx, void *ptr) {
  pool_free((PoolAllocator *)ctx, ptr);
}

internal size_t pool_capacity_impl(void *ctx) {
  return ((PoolAllocator *)ctx)->capacity;
}

internal size_t pool_commited_size_impl(void *ctx) {
  return pool_allocated_size((PoolAllocator *)ctx);
}

internal size_t pool_free_size_impl(void *ctx) {
  return pool_free_size((PoolAllocator *)ctx);
}

internal void pool_destroy_impl(void *ctx) {
  PoolAllocator *pool = (PoolAllocator *)ctx;
  pool->buffer = NULL;
  pool->capacity = 0;
  pool->chunk_size = 0;
  pool->chunk_count = 0;
  pool->allocated_count = 0;
  pool->head = NULL;
}

Allocator make_pool_allocator(PoolAllocator *pool) {
  return (Allocator){.alloc_alloc = pool_alloc_impl,
                     .alloc_realloc = pool_realloc_impl,
                     .alloc_reset = pool_reset_impl,
                     .alloc_free = pool_free_impl,
                     .alloc_capacity = pool_capacity_impl,
                     .alloc_commited_size = pool_commited_size_impl,
                     .alloc_free_size = pool_free_size_impl,
                     .alloc_destroy = pool_destroy_impl,
                     .ctx = pool};
}
