#ifndef H_ARENA_ALLOC
#define H_ARENA_ALLOC

#include "utils.c"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  char *buffer;
  size_t capacity;
  size_t offset;
} ArenaAllocator;

int arena_init(ArenaAllocator *arena, size_t size) {
  arena->buffer = (char *)malloc(size);
  if (!arena->buffer) {
    return -1;
  }
  arena->capacity = size;
  arena->offset = 0;
  return 0;
}

void *arena_alloc_align(ArenaAllocator *a, size_t size, size_t align) {
  ASSERT(a->buffer);
  ASSERT(a->capacity);
  uintptr_t curr_ptr = (uintptr_t)a->buffer + (uintptr_t)a->offset;
  uintptr_t offset = align_forward(curr_ptr, align);
  offset -= (uintptr_t)a->buffer;

  if (offset + size <= a->capacity) {
    void *ptr = &a->buffer[offset];
    a->offset = offset + size;

    memset(ptr, 0, size);
    return ptr;
  }
  return NULL;
}

#ifndef DEFAULT_ALIGNMENT
#define DEFAULT_ALIGNMENT (2 * sizeof(void *))
#endif

// Because C doesn't have default parameters
void *arena_alloc(ArenaAllocator *a, size_t size) {
  return arena_alloc_align(a, size, DEFAULT_ALIGNMENT);
}

void arena_free_all(ArenaAllocator *arena) { arena->offset = 0; }

void arena_destroy(ArenaAllocator *arena) {
  free(arena->buffer);
  arena->buffer = NULL;
  arena->capacity = 0;
  arena->offset = 0;
}
#endif
