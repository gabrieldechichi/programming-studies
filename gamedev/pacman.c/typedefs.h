#include "raylib.h"
#include <stddef.h>
#include <stdint.h>

#define internal static
#define global static
#define local_persist static

#define ARRAY_SIZE(arr) (size_t)(sizeof(arr) / sizeof(arr[0]))

#define CLAMP(v, a, b) v < a ? a : (v > b ? b : v)

#define XY_TO_INDEX(x, y, w) (y) * (w) + (x)

typedef unsigned char bool8_t;

typedef Color color32_t;
typedef Texture2D tex2d_t;

#define true 1
#define false 0
