#include "memory.h"
#include "common.h"
#include "fmt.h"
#include "lib/string.h"

#ifndef WASM
#include "os/os.h"
#endif

size_t arena_free_size(ArenaAllocator *arena) {
  return arena->reserved - arena->offset;
}

ArenaAllocator arena_from_buffer(uint8 *buffer, size_t capacity) {
  return cast(ArenaAllocator){
      .buffer = buffer,
      .reserved = capacity,
      .committed = capacity,
      .offset = 0,
      .commit_size = 0,
      .owns_memory = false};
}

ArenaAllocator arena_from_reserved_buffer(uint8 *buffer,
                                          size_t reserved_size,
                                          size_t committed_size,
                                          size_t commit_size) {
  return cast(ArenaAllocator){
      .buffer = buffer,
      .reserved = reserved_size,
      .committed = committed_size,
      .offset = 0,
      .commit_size = commit_size,
      .owns_memory = false};
}

#ifndef WASM
ArenaAllocator arena_create(size_t reserve_size, size_t commit_size) {
  ArenaAllocator arena = {0};

  u8 *buffer = os_reserve_memory(reserve_size);
  if (!buffer) {
    return arena;
  }

  size_t initial_commit = commit_size > 0 ? commit_size : ARENA_DEFAULT_COMMIT_SIZE;
  if (initial_commit > reserve_size) {
    initial_commit = reserve_size;
  }

  if (!os_commit_memory(buffer, initial_commit)) {
    os_free_memory(buffer, reserve_size);
    return arena;
  }

  arena.buffer = buffer;
  arena.reserved = reserve_size;
  arena.committed = initial_commit;
  arena.offset = 0;
  arena.commit_size = commit_size > 0 ? commit_size : ARENA_DEFAULT_COMMIT_SIZE;
  arena.owns_memory = true;

  return arena;
}

void arena_release(ArenaAllocator *arena) {
  if (arena->owns_memory && arena->buffer) {
    os_free_memory(arena->buffer, arena->reserved);
  }
  arena->buffer = NULL;
  arena->reserved = 0;
  arena->committed = 0;
  arena->offset = 0;
  arena->commit_size = 0;
  arena->owns_memory = false;
}
#endif

b32 sub_arena_from_arena(ArenaAllocator *a, size_t capacity,
                         ArenaAllocator *sub_arena) {
  assert(a->buffer);
  assert(a->reserved);
  size_t align = DEFAULT_ALIGNMENT;
  uintptr curr_ptr = (uintptr)a->buffer + (uintptr)a->offset;
  uintptr offset = align_forward(curr_ptr, align);
  offset -= (uintptr)a->buffer;

  if (offset + capacity > a->reserved) {
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

size_t arena_committed_size(ArenaAllocator *arena) {
  return arena->offset;
}

void *arena_alloc_align(ArenaAllocator *a, size_t size, size_t align) {
  assert(a->buffer);
  assert(a->reserved);
  uintptr curr_ptr = (uintptr)a->buffer + (uintptr)a->offset;
  uintptr offset = align_forward(curr_ptr, align);
  offset -= (uintptr)a->buffer;

  size_t needed = offset + size;

  if (needed <= a->committed) {
    void *ptr = &a->buffer[offset];
    a->offset = offset + size;
    memset(ptr, 0, size);
    return ptr;
  }

#ifndef WASM
  if (needed <= a->reserved && a->commit_size > 0) {
    size_t commit_to = align_forward(needed, a->commit_size);
    if (commit_to > a->reserved) {
      commit_to = a->reserved;
    }
    size_t commit_amount = commit_to - a->committed;
    if (os_commit_memory(a->buffer + a->committed, commit_amount)) {
      a->committed = commit_to;
      void *ptr = &a->buffer[offset];
      a->offset = offset + size;
      memset(ptr, 0, size);
      return ptr;
    }
  }
#endif

  debug_assert_msg(
      false, "Failed to allocate memory. Request % kb, Remaining % kb. Total %",
      FMT_UINT(BYTES_TO_KB(size)), FMT_UINT(BYTES_TO_KB(arena_free_size(a))), FMT_UINT(BYTES_TO_KB(a->reserved)));
  return NULL;
}

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
  if (arena->owns_memory && arena->buffer) {
#ifndef WASM
    os_free_memory(arena->buffer, arena->reserved);
#endif
  }
  arena->buffer = NULL;
  arena->reserved = 0;
  arena->committed = 0;
  arena->offset = 0;
  arena->commit_size = 0;
  arena->owns_memory = false;
}

void *arena_alloc_impl(void *ctx, size_t size, size_t align) {
  return arena_alloc_align((ArenaAllocator *)ctx, size, align);
}

void *arena_realloc_impl(void *ctx, void *ptr, size_t size) {
  return arena_realloc((ArenaAllocator *)ctx, ptr, size);
}

void arena_reset_impl(void *ctx) { arena_reset((ArenaAllocator *)ctx); }

void arena_free_impl(void *ctx, void *ptr) {
    UNUSED(ctx);
    UNUSED(ptr);
  // no free impl for arena
}

size_t arena_capacity_impl(void *ctx) {
  return ((ArenaAllocator *)ctx)->reserved;
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
                     .alloc_free = arena_free_impl,
                     .alloc_capacity = arena_capacity_impl,
                     .alloc_commited_size = arena_commited_size_impl,
                     .alloc_free_size = arena_free_size_impl,
                     .alloc_destroy = arena_destroy_impl,
                     .ctx = arena};
}
