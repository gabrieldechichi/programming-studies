#include <stddef.h>
#include <stdint.h>

#define internal static
#define global static

#define ARRAY_SIZE(arr) (size_t)(sizeof(arr) / sizeof(arr[0]))

#define XY_TO_INDEX(x, y, w) (y) * (w) + (x)
