#ifndef H_ASSERT
#define H_ASSERT
#include "fmt.h"

extern void assert_log(u8 log_level, const char *fmt, const FmtArgs *args,
                       const char *file_name, uint32 line_number);

#define ASSERT_LOG(level, fmt, ...)                                            \
  ({                                                                           \
    FmtArg args[] = {__VA_ARGS__};                                             \
    FmtArgs fmtArgs = {args, COUNT_VARGS(FmtArg, __VA_ARGS__)};                \
    assert_log(level, fmt, &fmtArgs, __FILE_NAME__, __LINE__);                 \
  })

#undef assert
#define assert(expr)                                                           \
  (!(expr) ? (ASSERT_LOG(2, "assert triggered %", FMT_STR(#expr)),             \
              __builtin_unreachable(), (void)0)                                \
           : (void)0)

#define assert_msg(expr, fmt, ...)                                             \
  (!(expr) ? (ASSERT_LOG(2, "assert triggered: " fmt, __VA_ARGS__),            \
              __builtin_unreachable(), (void)0)                                \
           : (void)0)

#ifdef GAME_DEBUG
#define debug_assert(expr) assert(expr)
#define debug_assert_msg(expr, fmt, ...) assert_msg(expr, fmt, __VA_ARGS__)
#else
#define debug_assert(expr) UNUSED(expr)
#define debug_assert_msg(expr, fmt, ...)
#endif

#endif
