/* STB IMAGE */
// extern void *global_alloc_temp(size_t size);
// extern void *global_realloc_temp(void *ptr, size_t size);

// stb lib include
// todo: investigate why this doesn't work on native
#ifdef WASM
#define STBI_ASSERT(x) assert(x)
#define STBI_MALLOC(x) ARENA_ALLOC_ARRAY(&tctx_current()->temp_arena, u8, x)
#define STBI_FREE(x)                                                           \
  do {                                                                         \
    UNUSED(x);                                                                 \
  } while (0)
#define STBI_REALLOC(ptr, newsz)                                               \
  ARENA_ALLOC_ARRAY(&tctx_current()->temp_arena, u8, newsz)
#endif

#define STBI_NO_STDIO
// #define STBI_ONLY_PNG

#define STBIDEF HZ_ENGINE_API
// #define STBI_NO_JPEG
// #define STBI_NO_PNG
#define STBI_NO_BMP
#define STBI_NO_PSD
#define STBI_NO_TGA
#define STBI_NO_GIF
// #define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_PNM

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wunused-function"
#endif
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION
#ifdef __clang__
#pragma clang diagnostic pop
#endif
/* END STB IMAGE */
