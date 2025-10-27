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

#define local_shared static

#define RANGE_DEFINE(type)                                                     \
  typedef struct Range_##type {                                                \
    type min;                                                                  \
    type max;                                                                  \
  } Range_##type;

RANGE_DEFINE(u64);

#endif
