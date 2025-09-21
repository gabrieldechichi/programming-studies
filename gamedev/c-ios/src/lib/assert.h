#ifndef H_ASSERT
#define H_ASSERT
#include "fmt.h"
#include <assert.h>

#define assert_msg(expr, ...) assert(expr)

#define debug_assert_or_return_void(expr) debug_assert_or_return(expr, (void)0)
#define debug_assert_or_return_void_msg(expr, fmt, ...)                        \
  debug_assert_or_return_msg(expr, (void)0, fmt, __VA_ARGS__)

#ifdef GAME_DEBUG
#define debug_assert(expr) assert(expr)
#define debug_assert_msg(expr, fmt, ...) assert_msg(expr, fmt, __VA_ARGS__)
#else
#define debug_assert(expr) UNUSED(expr)
#define debug_assert_msg(expr, fmt, ...)
#endif

#endif
