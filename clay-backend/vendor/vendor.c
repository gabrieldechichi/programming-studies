#define CLAY_IMPLEMENTATION
#include "clay/clay.h"

#include "math.h"
#include "memory.h"
#include "thread.h"
#include "assert.h"
#include "str.h"

// stb true type

#define STBTT_malloc(x, u) arena_alloc(&tctx_current()->temp_allocator, x)
#define STBTT_free(x, u)                                                       \
  do {                                                                         \
    UNUSED(x);                                                                 \
    UNUSED(u);                                                                 \
  } while (0)

#define STBTT_assert(x) assert(x)
#define STBTT_strlen(x) strlen(x)
#define STBTT_memcpy memcpy
#define STBTT_memset memset

#define STBTT_ifloor(x) ((int)floor(x))
#define STBTT_iceil(x) ((int)ceil(x))
#define STBTT_sqrt(x) sqrt(x)
#define STBTT_pow(x, y) pow(x, y)
#define STBTT_cos(x) cos(x)
#define STBTT_acos(x) acos(x)
#define STBTT_fabs(x) fabs(x)

#define STB_TRUETYPE_IMPLEMENTATION
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#include "stb/stb_truetype.h"
#pragma clang diagnostic pop
#undef STB_TRUETYPE_IMPLEMENTATION

#define STB_IMAGE_IMPLEMENTATION

#define STBI_MALLOC(x) arena_alloc(&tctx_current()->temp_allocator, x)
#define STBI_FREE(x)                                                       \
  do {                                                                         \
    UNUSED(x);                                                                 \
  } while (0)
#define STBI_REALLOC(ptr, newsz) arena_realloc(&tctx_current()->temp_allocator, ptr, newsz)

#define STBI_NO_STDIO
#define STBI_ONLY_PNG
// #define STBI_NO_ZLIB
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wunused-function"
#include "stb/stb_image.h"
#pragma clang diagnostic pop
#undef STB_IMAGE_IMPLEMENTATION

