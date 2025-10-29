#ifndef H_TYPEDEFS
#define H_TYPEDEFS

#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef u32 b32;
typedef uintptr_t uintptr;

#define UNUSED(x) (void)(x)

#define true 1
#define false 0

#define force_inline static inline __attribute__((always_inline))
#define local_shared static

#define ARRAY_LEN(arr) (sizeof((arr)) / sizeof((arr)[0]))
#define ARGS_ARRAY(type, ...) ((type[]){__VA_ARGS__})
#define ARGS_COUNT(type, ...)                                                  \
  (sizeof(ARGS_ARRAY(type, __VA_ARGS__)) / sizeof(type))

#define KB(x) (((u32)(x)) * (u32)1024)
#define MB(x) (KB(x) * (u32)1024)
#define GB(x) ((size_t)(MB(x)) * (size_t)1024)

#define RANGE_DEFINE(type)                                                     \
  typedef struct Range_##type {                                                \
    type min;                                                                  \
    type max;                                                                  \
  } Range_##type;

RANGE_DEFINE(u64);

#endif
