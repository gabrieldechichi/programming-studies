#ifndef MATH_H
#define MATH_H

#include "typedefs.h"

#if defined(__wasm__) && defined(__clang__)
// Use WASM builtins - these compile directly to f32.floor/ceil instructions
#define floorf(x) __builtin_floorf(x)
#define floor(x) __builtin_floor(x)
#define ceilf(x) __builtin_ceilf(x)
#define ceil(x) __builtin_ceil(x)
#define fabs(x) __builtin_fabs(x)
#define sqrt(x) __builtin_sqrt(x)
#define fmod(x, y) __builtin_fmod(x, y)

// todo: check if rolling our own would be faster
extern f64 _os_cos(f64 x);
extern f64 _os_acos(f64 x);
extern f64 _os_pow(f64 x, f64 y);
extern f32 _os_roundf(f32 x);

#define cos(x) _os_cos(x)
#define acos(x) _os_acos(x)
#define pow(x, y) _os_pow(x, y)
#define roundf(x) _os_roundf(x)
#else

#include <math.h>

#endif // __wasm__ && __clang__
#endif // MATH_H
