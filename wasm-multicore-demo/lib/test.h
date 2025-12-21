#ifndef H_TEST
#define H_TEST

static b32 g_test_failed = false;
#define ASSERT_FAILED() (g_test_failed = true)

#include "os/os.h"
#include "assert.h"
#include "memory.h"
#include "thread_context.h"

static u32 g_test_count = 0;
static u32 g_test_passed = 0;

#define RUN_TEST(test_func)                                                    \
  do {                                                                         \
    if (is_main_thread()) {                                                    \
      g_test_failed = false;                                                   \
      LOG_INFO("Running test: %", FMT_STR(#test_func));                        \
      g_test_count++;                                                          \
    }                                                                          \
    test_func();                                                               \
    lane_sync();                                                               \
    arena_reset(&tctx_current()->temp_arena);                                  \
    if (is_main_thread()) {                                                    \
      if (g_test_failed) {                                                     \
        LOG_ERROR("FAILED: %", FMT_STR(#test_func));                           \
      } else {                                                                 \
        g_test_passed++;                                                       \
        LOG_INFO("PASSED: %", FMT_STR(#test_func));                            \
      }                                                                        \
    }                                                                          \
    lane_sync();                                                               \
  } while(0)

#define assert_eq(actual, expected)                                            \
  do {                                                                         \
    assert_msg((actual) == (expected),                                         \
               "ASSERT_EQ failed at %:%: expected %, got %",                   \
               FMT_STR(__FILE_NAME__), FMT_UINT(__LINE__),                     \
               FMT_UINT((u32)(expected)), FMT_UINT((u32)(actual)));            \
    if (g_test_failed) return;                                                 \
  } while(0)

#define assert_str_eq(actual, expected)                                        \
  do {                                                                         \
    assert_msg(str_equal(actual, expected),                                    \
               "ASSERT_STR_EQ failed at %:%: expected '%', got '%'",           \
               FMT_STR(__FILE_NAME__), FMT_UINT(__LINE__), FMT_STR(expected),  \
               FMT_STR(actual));                                               \
    if (g_test_failed) return;                                                 \
  } while(0)

#define assert_true(condition)                                                 \
  do {                                                                         \
    assert_msg(condition, "ASSERT_TRUE failed at %:%: condition was false",    \
               FMT_STR(__FILE_NAME__), FMT_UINT(__LINE__));                    \
    if (g_test_failed) return;                                                 \
  } while(0)

#define assert_false(condition)                                                \
  do {                                                                         \
    assert_msg(!(condition), "ASSERT_FALSE failed at %:%: condition was true", \
               FMT_STR(__FILE_NAME__), FMT_UINT(__LINE__));                    \
    if (g_test_failed) return;                                                 \
  } while(0)

#define assert_mem_eq(type, actual, expected)                                  \
  do {                                                                         \
    assert_msg((actual == NULL && expected == NULL) ||                         \
                   (actual != NULL && expected != NULL &&                      \
                    memcmp(actual, expected, sizeof(type)) == 0),              \
               "ASSERT_MEM_EQ failed at %:%", FMT_STR(__FILE_NAME__),          \
               FMT_UINT(__LINE__));                                            \
    if (g_test_failed) return;                                                 \
  } while(0)

#define print_test_results()                                                   \
  do {                                                                         \
    if (g_test_passed == g_test_count) {                                       \
      LOG_INFO("[PASS] All % tests passed!", FMT_UINT(g_test_count));          \
    } else {                                                                   \
      LOG_ERROR("[FAIL] % out of % tests failed",                              \
                FMT_UINT(g_test_count - g_test_passed),                        \
                FMT_UINT(g_test_count));                                       \
    }                                                                          \
  } while (0)

#endif // H_TEST