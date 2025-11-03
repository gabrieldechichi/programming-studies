#ifndef H_TYPEDEFS
#define H_TYPEDEFS

#include <stdint.h>

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

#define WASM_EXPORT(name) __attribute__((export_name(name)))
#define UNUSED(x) ((void)(x))
#define ARRAY_LEN(arr) (sizeof(arr) / sizeof(arr[0]))
#define CSTR_LEN(str) ((sizeof(str) / sizeof(str[0])) - 1)


#define NULL 0
#define true 1
#define false 0

#define force_inline static inline __attribute__((always_inline))

#define thread_static __thread
#endif
