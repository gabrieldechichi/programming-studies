#ifndef H_TYPEDEFS
#define H_TYPEDEFS

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define internal static
#define global static
#define local_persist static

#ifdef _WIN32
#define export __declspec(dllexport)
#else
#define export
#endif

#define ARRAY_SIZE(arr) (size_t)(sizeof(arr) / sizeof(arr[0]))

#define CLAMP(v, a, b) v < a ? a : (v > b ? b : v)

#define XY_TO_INDEX(x, y, w) (y) * (w) + (x)

#define assert(expr)                                                           \
  if (!(expr)) {                                                               \
    printf("Assert failed: %s, in %s:%d\n", #expr, __FILE__, __LINE__);        \
    abort();                                                                   \
  }

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

#define true 1
#define false 0

#define UNUSED(x) (void)(x)

#define PI 3.14159265358979323846

#define MS_TO_SECS(ms) ((float)(ms) / 1000.0f)
#define MS_TO_MCS(ms) ((uint64_t)((ms) * 1000.0f))
#define MS_TO_NS(ms) ((uint64_t)((ms) * 1000000.0f))

#define MCS_TO_SECS(mcs) ((float)(mcs) / 1000000.0f)

#define NS_TO_SECS(ns) ((float)(ns) / 1000000000.0f)
#define NS_TO_MS(ns) ((uint64_t)((ns) / 1000000))
#define NS_TO_MCS(ns) ((uint64_t)((ns) / 1000))

#define SECS_TO_MS(secs) ((uint64_t)((secs) * 1000.0f))
#define SECS_TO_MCS(secs) ((uint64_t)((secs) * 1000000.0f))
#define SECS_TO_NS(secs) ((uint64_t)((secs) * 1000000000.0f))

#define MIN(a,b) (a) < (b) ? (a) : (b)
#define MAX(a,b) (a) > (b) ? (a) : (b)

#endif
