/*
    assert.h - Assertions with formatting

    OVERVIEW

    --- Custom assert macros with formatted messages

    --- assert(expr): always enabled, crashes on false

    --- debug_assert(expr): only in DEBUG builds

    --- both support _msg variants with formatted messages

    USAGE
        assert(ptr != NULL);
        assert_msg(x > 0, "x must be positive, got %", FMT_INT(x));

        debug_assert(index < array_len);
        debug_assert_msg(health > 0, "health is %", FMT_INT(health));
*/

#ifndef H_ASSERT
#define H_ASSERT
#include "fmt.h"

HZ_ENGINE_API void assert_log(u8 log_level, const char *fmt, const FmtArgs *args,
                       const char *file_name, uint32 line_number);

#define ASSERT_LOG(level, fmt, ...)                                            \
  do {                                                                         \
    FmtArg _assert_args[] = {(FmtArg){.type=0}, ##__VA_ARGS__};               \
    size_t _count = (sizeof(_assert_args)/sizeof(FmtArg)) - 1;                \
    FmtArgs _assert_fmt = {_assert_args + 1, (uint8)_count};                  \
    assert_log(level, fmt, &_assert_fmt, __FILE_NAME__, __LINE__);            \
  } while(0)

#undef assert
#define assert(expr)                                                           \
  do {                                                                         \
    if (!(expr)) {                                                             \
      ASSERT_LOG(2, "assert triggered %", FMT_STR(#expr));                    \
      __builtin_unreachable();                                                 \
    }                                                                          \
  } while(0)

#define assert_msg(expr, fmt, ...)                                             \
  do {                                                                         \
    if (!(expr)) {                                                             \
      ASSERT_LOG(2, "assert triggered: " fmt, __VA_ARGS__);                   \
      __builtin_unreachable();                                                 \
    }                                                                          \
  } while(0)

#ifdef DEBUG
#define debug_assert(expr) assert(expr)
#define debug_assert_msg(expr, fmt, ...) assert_msg(expr, fmt, __VA_ARGS__)
#else
#define debug_assert(expr) UNUSED(expr)
#define debug_assert_msg(expr, fmt, ...)
#endif

#endif
