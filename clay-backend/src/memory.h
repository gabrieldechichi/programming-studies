#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>
#include <stdint.h>

#if defined(__wasm__) && defined(__clang__)
// Use WASM builtins - compile directly to memory.copy and memory.fill
#define memcpy(dest, src, n) __builtin_memcpy(dest, src, n)
#define memset(dest, val, n) __builtin_memset(dest, val, n)
#define memmove(dest, src, n) __builtin_memmove(dest, src, n)

// Simple bump allocator for WASM
// Note: No free() - memory is never reclaimed (use arena/pool allocators instead)
extern unsigned char __heap_base;
static unsigned char *__heap_ptr = &__heap_base;

static inline void *malloc(size_t size) {
    // Align to 8 bytes
    size = (size + 7) & ~7;

    void *ptr = __heap_ptr;
    __heap_ptr += size;

    return ptr;
}

static inline void *calloc(size_t num, size_t size) {
    size_t total = num * size;
    void *ptr = malloc(total);
    memset(ptr, 0, total);
    return ptr;
}

static inline void free(void *ptr) {
    // Bump allocator doesn't free - this is a no-op
    (void)ptr;
}

#else
#define memcpy(dest, src, n) __error("memcpy not implemented for platform")
#define memset(dest, src, n) __error("memset not implemented for platform")
#define memmove(dest, src, n) __error("memmove not implemented for platform")
#define malloc(size) __error("malloc not implemented for platform")
#define calloc(num, size) __error("calloc not implemented for platform")
#define free(ptr) __error("free not implemented for platform")
#endif

#endif // MEMORY_H
