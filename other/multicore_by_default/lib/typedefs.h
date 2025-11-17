#ifndef H_TYPEDEFS
#define H_TYPEDEFS

#include <stddef.h>
#include <stdint.h>

#ifndef _MSC_VER
#include <stdatomic.h>
#endif

/*
 * Macros
 */
#ifdef _WIN32
#ifdef HOT_RELOAD
#ifdef BUILDING_DLL
#define HZ_ENGINE_API __declspec(dllimport)
#define HZ_APP_API __declspec(dllexport)
#else
#define HZ_ENGINE_API __declspec(dllexport)
#define HZ_APP_API __declspec(dllimport)
#endif
#else
#define HZ_ENGINE_API
#define HZ_APP_API
#endif
#else
#define HZ_ENGINE_API __attribute__((visibility("default")))
#define HZ_APP_API __attribute__((visibility("default")))
#endif

#ifdef WASM
#define WASM_EXPORT(name) __attribute__((export_name(#name)))
#else
#define WASM_EXPORT(name)
#endif

#ifdef _MSC_VER
#define force_inline static inline __forceinline
#ifndef __FILE_NAME__
#define __FILE_NAME__ __FILE__
#endif
#define __builtin_unreachable() __assume(0)
#define thread_local __declspec(thread)
typedef volatile long atomic_int;
extern long _InterlockedExchangeAdd(long volatile *, long);
#define atomic_fetch_add(ptr, val)                                             \
  _InterlockedExchangeAdd((long *)(ptr), (long)(val))
#define atomic_load(ptr) (*(ptr))
#define atomic_store(ptr, val) (*(ptr) = (val))
#else
#define force_inline static inline __attribute__((always_inline))
#define thread_local _Thread_local
#endif

#define UNUSED(x) (void)(x)
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

// MSVC-compatible argument counting (supports 0-32 arguments)
// Extra indirection needed for proper __VA_ARGS__ expansion on MSVC
#define _COUNT_ARGS_IMPL(_0,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,N,...) N
#define _COUNT_ARGS_EXPAND(args) _COUNT_ARGS_IMPL args
#define _COUNT_ARGS(...) _COUNT_ARGS_EXPAND((0,##__VA_ARGS__,32,31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0))

// Legacy macros (kept for non-MSVC compatibility, but _COUNT_ARGS works everywhere)
#ifndef _MSC_VER
#define ARGS_ARRAY(type, ...) ((type[]){##__VA_ARGS__})
#define ARGS_COUNT(type, ...)                                                  \
  (sizeof(ARGS_ARRAY(type, ##__VA_ARGS__)) / sizeof(type))
#else
#define ARGS_COUNT(type, ...) _COUNT_ARGS(__VA_ARGS__)
#endif

#define cast(type) (type)
#define cast_data(type, data) (*((type *)&(data)))

#ifndef _MSC_VER
#define static_assert _Static_assert
#endif

#ifndef _MSC_VER
#define is_same(a, b)                                                          \
  static_assert(__builtin_types_compatible_p(__typeof__(a), __typeof__(b)),    \
                #a " and " #b " must be the same type")
#define is_type(type, a)                                                       \
  static_assert(__builtin_types_compatible_p(type, __typeof__(a)),             \
                #a " should of type " #type " ")
#else
#define is_same(a, b)
#define is_type(type, a)
#endif

#define PI 3.14159265358979323846f
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
#define GB(x) ((size_t)(MB(x)) * (size_t)1024)

#define BYTES_TO_KB(x) ((f32)(x) / (f32)1024)
#define BYTES_TO_MB(x) ((BYTES_TO_KB(x)) / 1024)
#define BYTES_TO_GB(x) ((BYTES_TO_MB(x)) / 1024)

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define ALIGN_POW2(value, alignment)                                           \
  ((value + ((alignment) - 1)) & ~((alignment) - 1))
#define ALIGN_4(value) ((value + 3) & ~3)
#define ALIGN_8(value) ((value + 7) & ~7)
#define ALIGN_16(value) ((value + 15) & ~15)

/*
 * End Macros
 */

/*
 * Typedefs
 */
#if !defined(internal)
#define internal static
#endif
#define global static
#define local_persist static
#define local_shared static
#define true 1
#define false 0

#ifdef _MSC_VER
#define packed_struct
#define PACK_STRUCT_BEGIN __pragma(pack(push, 1))
#define PACK_STRUCT_END __pragma(pack(pop))
#else
#define packed_struct __attribute__((packed))
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END
#endif

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

// Memory barrier for different compilers
#ifdef _MSC_VER
// _WriteBarrier is available from intrin.h which should be included by the user
#elif defined(__clang__) || defined(__GNUC__)
#define _WriteBarrier() __asm__ __volatile__("" ::: "memory")
#else
#warning "Memory barrier not implemented for this compiler"
#define _WriteBarrier()
#endif

// todo: move this somewhere else
#define RANGE_DEFINE(type)                                                     \
  typedef struct Range_##type {                                                \
    type min;                                                                  \
    type max;                                                                  \
  } Range_##type;

RANGE_DEFINE(u64);

#endif
