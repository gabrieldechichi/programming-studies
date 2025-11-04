#include "memory.h"

force_inline b32 is_power_of_two(uintptr x) { return (x & (x - 1)) == 0; }

force_inline uintptr align_forward(uintptr ptr, size_t align) {
  uintptr p, a, modulo;


  p = ptr;
  a = (uintptr)align;
  // Same as (p % a) but faster as 'a' is a power of two
  modulo = p & (a - 1);

  if (modulo != 0) {
    // If 'p' address is not aligned, push the address to the
    // next value which is aligned
    p += a - modulo;
  }
  return p;
}

size_t arena_free_size(ArenaAllocator *arena) {
  return arena->capacity - arena->offset;
}

ArenaAllocator arena_from_buffer(uint8 *buffer, size_t capacity) {
  return (ArenaAllocator){.buffer = buffer, .capacity = capacity, .offset = 0};
}

b32 sub_arena_from_arena(ArenaAllocator *a, size_t capacity,
                         ArenaAllocator *sub_arena) {
  size_t align = DEFAULT_ALIGNMENT;
  uintptr curr_ptr = (uintptr)a->buffer + (uintptr)a->offset;
  uintptr offset = align_forward(curr_ptr, align);
  offset -= (uintptr)a->buffer;

  if (offset + capacity > a->capacity) {
    return false;
  }

  void *ptr = &a->buffer[offset];
  a->offset = offset + capacity;

  *sub_arena = arena_from_buffer(ptr, capacity);

  return true;
}

size_t arena_committed_size(ArenaAllocator *arena) { return arena->offset; }

void *arena_alloc_align(ArenaAllocator *a, size_t size, size_t align) {
  uintptr curr_ptr = (uintptr)a->buffer + (uintptr)a->offset;
  uintptr offset = align_forward(curr_ptr, align);
  offset -= (uintptr)a->buffer;

  if (offset + size <= a->capacity) {
    void *ptr = &a->buffer[offset];
    a->offset = offset + size;

#ifdef DEBUG
    // todo: option to write memory as 0xCD instead (also more memory debugging
    // features like overrun/underrun, etc)
    memset(ptr, 0x0, size);
#endif
    return ptr;
  }

  // couldn't allocate memory
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
                     .alloc_free = arena_free_impl,
                     .alloc_capacity = arena_capacity_impl,
                     .alloc_commited_size = arena_commited_size_impl,
                     .alloc_free_size = arena_free_size_impl,
                     .alloc_destroy = arena_destroy_impl,
                     .ctx = arena};
}
