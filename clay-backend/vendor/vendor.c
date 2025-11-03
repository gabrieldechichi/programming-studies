#define CLAY_IMPLEMENTATION
#include "clay/clay.h"

#include "math.h"
#include "memory.h"
#include "assert.h"
#include "string.h"

// stb true type

// #define STBTT_malloc(x, u) malloc(x)
// #define STBTT_free(x, u) free(x)
// #define STBTT_assert(x) assert(x)
// #define STBTT_strlen(x) strlen(x)
// #define STBTT_memcpy memcpy
// #define STBTT_memset memset
//
// #define STBTT_ifloor(x) ((int)floor(x))
// #define STBTT_iceil(x) ((int)ceil(x))
// #define STBTT_sqrt(x) sqrt(x)
// #define STBTT_pow(x, y) pow(x, y)
// #define STBTT_cos(x) cos(x)
// #define STBTT_acos(x) acos(x)
// #define STBTT_fabs(x) fabs(x)
//
// //font stash
// #define FONTSTASH_IMPLEMENTATION	// Expands implementation
// #pragma clang diagnostic push
// #pragma clang diagnostic ignored "-Wunused-parameter"
// #include "fontstash/fontstash.h"
// #undef FONTSTASH_IMPLEMENTATION	// Expands implementation
