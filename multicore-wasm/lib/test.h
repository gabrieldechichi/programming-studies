#ifndef H_TEST
#define H_TEST

#include "os/os.h"
#include "assert.h"
#include "memory.h"

typedef struct TestContext {
  Allocator allocator;
} TestContext;

static u32 test_count = 0;
static u32 test_passed = 0;

#define RUN_TEST(test_func, ctx)                                              \
  do {                                                                         \
    LOG_INFO("Running test: %", FMT_STR(#test_func));                          \
    test_count++;                                                              \
    test_func(ctx);                                                            \
    ALLOC_RESET(&(ctx)->allocator);                                            \
    test_passed++;                                                             \
    LOG_INFO("PASSED: %", FMT_STR(#test_func));                                \
  } while(0)

#define assert_eq(actual, expected)                                            \
  assert_msg((actual) == (expected),                                           \
             "ASSERT_EQ failed at %:%: expected %, got %",                     \
             FMT_STR(__FILE_NAME__), FMT_UINT(__LINE__),                       \
             FMT_UINT((u32)(expected)), FMT_UINT((u32)(actual)))

#define assert_str_eq(actual, expected)                                        \
  assert_msg(str_equal(actual, expected),                                      \
             "ASSERT_STR_EQ failed at %:%: expected '%', got '%'",             \
             FMT_STR(__FILE_NAME__), FMT_UINT(__LINE__), FMT_STR(expected),    \
             FMT_STR(actual))

#define assert_true(condition)                                                 \
  assert_msg(condition, "ASSERT_TRUE failed at %:%: condition was false",      \
             FMT_STR(__FILE_NAME__), FMT_UINT(__LINE__))

#define assert_false(condition)                                                \
  assert_msg(!(condition), "ASSERT_FALSE failed at %:%: condition was true",   \
             FMT_STR(__FILE_NAME__), FMT_UINT(__LINE__))

#define assert_mem_eq(type, actual, expected)                                  \
  assert_msg((actual == NULL && expected == NULL) ||                           \
                 (actual != NULL && expected != NULL &&                        \
                  memcmp(actual, expected, sizeof(type)) == 0),                \
             "ASSERT_MEM_EQ failed at %:%", FMT_STR(__FILE_NAME__),            \
             FMT_UINT(__LINE__))

#define print_test_results()                                                   \
  do {                                                                         \
    if (test_passed == test_count) {                                           \
      LOG_INFO("All % tests passed!", FMT_UINT(test_count));                   \
    } else {                                                                   \
      LOG_ERROR("% out of % tests failed", FMT_UINT(test_count - test_passed), \
                FMT_UINT(test_count));                                         \
    }                                                                          \
  } while (0)

#endif // H_TEST