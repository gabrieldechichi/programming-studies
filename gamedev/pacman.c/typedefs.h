#include <stddef.h>
#include <stdint.h>

#define internal static
#define global static
#define local_persist static

#define ARRAY_SIZE(arr) (size_t)(sizeof(arr) / sizeof(arr[0]))

#define CLAMP(v, a, b) v < a ? a : (v > b ? b : v)

#define XY_TO_INDEX(x, y, w) (y) * (w) + (x)

#define assert(expr)                                                           \
  if (!(expr)) {                                                               \
    printf("Assert failed: %s, in %s:%d\n", #expr, __FILE__, __LINE__);        \
    abort();                                                                   \
  }

typedef unsigned char bool8_t;

#define true 1
#define false 0
