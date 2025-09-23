/* CGLM */
#include "cglm/cglm.h"

typedef versor quaternion;
/* END CGLM */

/* STB IMAGE */
extern void *global_alloc_temp(size_t size);
extern void *global_realloc_temp(void *ptr, size_t size);

// stb lib include
#define STBI_ASSERT(x) assert(x)
#define STBI_MALLOC(sz) global_alloc_temp(sz)
#define STBI_REALLOC(p, newsz) global_realloc_temp(p, newsz)
// no free of temp arena
#define STBI_FREE(p)

#define STBI_NO_JPEG
// #define STBI_NO_PNG
#define STBI_NO_BMP
#define STBI_NO_PSD
#define STBI_NO_TGA
#define STBI_NO_GIF
#define STBI_NO_HDR
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