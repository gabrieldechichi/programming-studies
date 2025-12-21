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
#include "lib/typedefs.h"

#ifndef ASSERT_FAILED
#define ASSERT_FAILED() __builtin_unreachable()
#endif

HZ_ENGINE_API void assert_log(u8 log_level, const char *fmt,
                              const FmtArgs *args, const char *file_name,
                              uint32 line_number);

#define ASSERT_LOG(level, fmt, ...)                                            \
  do {                                                                         \
    FmtArg _assert_args[] = {(FmtArg){.type = 0}, ##__VA_ARGS__};              \
    size_t _count = (sizeof(_assert_args) / sizeof(FmtArg)) - 1;               \
    FmtArgs _assert_fmt = {_assert_args + 1, (uint8)_count};                   \
    assert_log(level, fmt, &_assert_fmt, __FILE_NAME__, __LINE__);             \
  } while (0)

#undef assert
#define assert(expr)                                                           \
  do {                                                                         \
    if (!(expr)) {                                                             \
      ASSERT_LOG(2, "assert triggered %", FMT_STR(#expr));                     \
      ASSERT_FAILED();                                                         \
    }                                                                          \
  } while (0)

#define assert_msg(expr, fmt, ...)                                             \
  do {                                                                         \
    if (!(expr)) {                                                             \
      ASSERT_LOG(2, "assert triggered: " fmt, __VA_ARGS__);                    \
      ASSERT_FAILED();                                                         \
    }                                                                          \
  } while (0)

force_inline void assert_expr_helper(b32 condition, const char *expr_str,
                                     const char *file, uint32 line) {
  if (!condition) {
    FmtArg args[] = {(FmtArg){.type = 0}, FMT_STR(expr_str)};
    FmtArgs fmt = {args + 1, 1};
    assert_log(2, "assert triggered %", &fmt, file, line);
    ASSERT_FAILED();
  }
}

#define assert_expr(expr)                                                      \
  (assert_expr_helper((expr), #expr, __FILE_NAME__, __LINE__))

//todo: fix this
#define _ARGS_ARRAY(...) \
  (FmtArg[]){(FmtArg){.type = 0}, ##__VA_ARGS__}

#define _ARGS_COUNT(...) \
  ((uint8)((sizeof(_ARGS_ARRAY(__VA_ARGS__)) / sizeof(FmtArg)) - 1))

force_inline void assert_expr_msg_helper(b32 condition, const char *fmt,
                                         const FmtArgs *args, const char *file,
                                         uint32 line) {
  if (!condition) {
    assert_log(2, fmt, args, file, line);
    ASSERT_FAILED();
  }
}

#define assert_expr_msg(expr, fmt, ...)                                        \
  (assert_expr_msg_helper((expr), fmt,                                         \
                          &(FmtArgs){_ARGS_ARRAY(__VA_ARGS__) + 1,              \
                                     _ARGS_COUNT(__VA_ARGS__)},                 \
                          __FILE_NAME__, __LINE__))

#ifdef DEBUG
#define debug_assert(expr) assert(expr)
#define debug_assert_msg(expr, fmt, ...) assert_msg(expr, fmt, __VA_ARGS__)
#define debug_assert_expr(expr) assert_expr(expr)
#define debug_assert_expr_msg(expr, fmt, ...) assert_expr_msg(expr, fmt, __VA_ARGS__)
#else
#define debug_assert(expr) UNUSED(expr)
#define debug_assert_msg(expr, fmt, ...)
#define debug_assert_expr(expr) ((void)0)
#define debug_assert_expr_msg(expr, fmt, ...) ((void)0)
#endif

#endif
