#ifndef H_TYPEDEFS
#define H_TYPEDEFS

#include <stddef.h>
#include <stdint.h>

/*
 * Macros
 */
#define export __attribute__((visibility("default")))
#define force_inline static inline __attribute((always_inline))
#define UNUSED(x) (void)(x)
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))
#define COUNT_VARGS(type, ...) (sizeof((type[]){__VA_ARGS__}) / sizeof(type))
#define cast(type) (type)
#define cast_data(type, data) (*((type *)&(data)))
#define static_assert _Static_assert
#define is_same(a, b)                                                          \
  _Static_assert(__builtin_types_compatible_p(__typeof__(a), __typeof__(b)),   \
                 #a " and " #b " must be the same type")
#define is_type(type, a)                                                       \
  _Static_assert(__builtin_types_compatible_p(type, __typeof__(a)),            \
                 #a " should of type " #type " ")

#define PI 3.14159265358979323846
#define EPSILON 0.000001

#define MS_TO_SECS(ms) ((float)(ms) / 1000.0f)
#define MS_TO_MCS(ms) ((uint64)((ms) * 1000.0f))
#define MS_TO_NS(ms) ((uint64)((ms) * 1000000.0f))

#define MCS_TO_SECS(mcs) ((float)(mcs) / 1000000.0f)

#define NS_TO_SECS(ns) ((float)(ns) / 1000000000.0f)
#define NS_TO_MS(ns) ((uint64)((ns) / 1000000))
#define NS_TO_MCS(ns) ((uint64)((ns) / 1000))

#define SECS_TO_MS(secs) ((uint64)((secs) * 1000.0f))
#define SECS_TO_MCS(secs) ((uint64)((secs) * 1000000.0f))
#define SECS_TO_NS(secs) ((uint64)((secs) * 1000000000.0f))

#define KB(x) (((u32)(x)) * (u32)1024)
#define MB(x) (KB(x) * (u32)1024)
#define GB(x) ((u64)(MB(x)) * (u64)1024)

#define BYTES_TO_KB(x) ((f32)(x) / (f32)1024)
#define BYTES_TO_MB(x) ((BYTES_TO_KB(x)) / 1024)
#define BYTES_TO_GB(x) ((BYTES_TO_MB(x)) / 1024)

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
/*
 * End Macros
 */

/*
 * Typedefs
 */
#define true 1
#define false 0
#define packed_struct __attribute__((packed))

#define _out_

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float float32;
typedef float f32;
typedef double float64;
typedef double f64;

typedef uint32_t bool32;
typedef uint32_t b32;
typedef uintptr_t uintptr;
/*
 * End Typedefs
 */
#endif
