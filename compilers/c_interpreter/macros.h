#ifndef H_MACROS
#define H_MACROS

#include "assert.h"

typedef char bool;

#define internal static

#define GD_JOIN2(a, b) a##b
#define GD_JOIN3(a, b, c) GD_JOIN2(a, GD_JOIN2(b, c))
#define GD_JOIN4(a, b, c, d) GD_JOIN2(a, GD_JOIN2(b, GD_JOIN2(c, d)))

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define ARRAY_LEN(array) (sizeof(array) / sizeof((array)[0]))
#define TRUE 1
#define FALSE 0

#define ASSERT assert

#define ASSERT_WITH_MSG(expr, msg, ...)                                        \
  if (!(expr)) {                                                               \
    fprintf(stderr, (msg), __VA_ARGS__);                                       \
    fprintf(stderr, "\n");                                                     \
    assert(expr);                                                              \
  }

#define ASSERT_EQ_INT(expected, actual)                                        \
  ASSERT_WITH_MSG((int)expected == (int)actual, "Expected %d but got %d",      \
                  (int)expected, (int)actual)

#define ASSERT_NOT_NULL(ptr)                                                   \
  ASSERT_WITH_MSG(ptr != 0, "Expected ptr not to be null%d", 0)

#define ASSERT_NULL(ptr)                                                       \
  ASSERT_WITH_MSG(ptr == 0, "Expected ptr to be null%d", 0)

#ifdef DEBUG
#define DEBUG_ASSERT(expr) assert(expr)
#else
#define DEBUG_ASSERT(expr) ((void)0)
#endif

#endif
