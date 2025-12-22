#ifndef H_TEST
#define H_TEST

#include "os/os.h"
#include "assert.h"
#include "memory.h"
#include "thread_context.h"
#include "multicore_runtime.h"

#define TEST_MAX_TESTS 64

typedef void (*TestFunc)(void);

typedef struct {
    TestFunc func;
    const char *name;
    b32 multicore;
} TestEntry;

typedef struct {
    TestEntry tests[TEST_MAX_TESTS];
    u32 test_count;
    u32 single_threaded_count;
    u32 multicore_count;

    u32 tests_passed;
    u32 tests_failed;

    MCRTaskQueue queue;
    ArenaAllocator *arena;
} TestRunner;

global TestRunner g_test_runner;
global _Thread_local b32 g_test_failed = false;
global _Thread_local const char *g_current_test_name = NULL;

#define REGISTER_TEST(test_func)                                               \
  do {                                                                         \
    TestEntry *entry = &g_test_runner.tests[g_test_runner.test_count++];       \
    entry->func = test_func;                                                   \
    entry->name = #test_func;                                                  \
    entry->multicore = false;                                                  \
    g_test_runner.single_threaded_count++;                                     \
  } while(0)

#define REGISTER_TEST_MULTICORE(test_func)                                     \
  do {                                                                         \
    TestEntry *entry = &g_test_runner.tests[g_test_runner.test_count++];       \
    entry->func = test_func;                                                   \
    entry->name = #test_func;                                                  \
    entry->multicore = true;                                                   \
    g_test_runner.multicore_count++;                                           \
  } while(0)

internal void _test_task_wrapper(void *data) {
    TestEntry *entry = (TestEntry *)data;
    g_test_failed = false;
    g_current_test_name = entry->name;

    LOG_INFO("Running test: %", FMT_STR(entry->name));

    entry->func();

    arena_reset(&tctx_current()->temp_arena);

    if (g_test_failed) {
        ins_atomic_u32_inc_eval(&g_test_runner.tests_failed);
        LOG_ERROR("FAILED: %", FMT_STR(entry->name));
    } else {
        ins_atomic_u32_inc_eval(&g_test_runner.tests_passed);
        LOG_INFO("PASSED: %", FMT_STR(entry->name));
    }
}

internal void test_runner_init(ArenaAllocator *arena) {
    g_test_runner.arena = arena;
    g_test_runner.test_count = 0;
    g_test_runner.single_threaded_count = 0;
    g_test_runner.multicore_count = 0;
    g_test_runner.tests_passed = 0;
    g_test_runner.tests_failed = 0;
    memset(&g_test_runner.queue, 0, sizeof(MCRTaskQueue));
}

internal void test_runner_run(void) {
    if (is_main_thread()) {
        for (u32 i = 0; i < g_test_runner.test_count; i++) {
            TestEntry *entry = &g_test_runner.tests[i];
            if (!entry->multicore) {
                mcr_queue_append(&g_test_runner.queue, _test_task_wrapper, entry,
                                NULL, 0, NULL, 0);
            }
        }
    }
    lane_sync();

    if (g_test_runner.single_threaded_count > 0) {
        mcr_queue_process(&g_test_runner.queue);
    }

    for (u32 i = 0; i < g_test_runner.test_count; i++) {
        TestEntry *entry = &g_test_runner.tests[i];
        if (entry->multicore) {
            if (is_main_thread()) {
                g_test_failed = false;
                g_current_test_name = entry->name;
                LOG_INFO("Running multicore test: %", FMT_STR(entry->name));
            }
            lane_sync();

            entry->func();

            lane_sync();
            arena_reset(&tctx_current()->temp_arena);

            if (is_main_thread()) {
                if (g_test_failed) {
                    g_test_runner.tests_failed++;
                    LOG_ERROR("FAILED: %", FMT_STR(entry->name));
                } else {
                    g_test_runner.tests_passed++;
                    LOG_INFO("PASSED: %", FMT_STR(entry->name));
                }
            }
            lane_sync();
        }
    }
}

internal void test_runner_print_results(void) {
    if (is_main_thread()) {
        u32 total = g_test_runner.tests_passed + g_test_runner.tests_failed;
        if (g_test_runner.tests_failed == 0) {
            LOG_INFO("[PASS] All % tests passed!", FMT_UINT(total));
        } else {
            LOG_ERROR("[FAIL] % out of % tests failed",
                      FMT_UINT(g_test_runner.tests_failed),
                      FMT_UINT(total));
        }
    }
}

#undef ASSERT_FAILED
#define ASSERT_FAILED() (g_test_failed = true)

#define assert_eq(actual, expected)                                            \
  do {                                                                         \
    if ((actual) != (expected)) {                                              \
      LOG_ERROR("ASSERT_EQ failed at %:%: expected %, got %",                  \
               FMT_STR(__FILE_NAME__), FMT_UINT(__LINE__),                     \
               FMT_UINT((u32)(expected)), FMT_UINT((u32)(actual)));            \
      g_test_failed = true;                                                    \
      return;                                                                  \
    }                                                                          \
  } while(0)

#define assert_str_eq(actual, expected)                                        \
  do {                                                                         \
    if (!str_equal(actual, expected)) {                                        \
      LOG_ERROR("ASSERT_STR_EQ failed at %:%: expected '%', got '%'",          \
               FMT_STR(__FILE_NAME__), FMT_UINT(__LINE__), FMT_STR(expected),  \
               FMT_STR(actual));                                               \
      g_test_failed = true;                                                    \
      return;                                                                  \
    }                                                                          \
  } while(0)

#define assert_true(condition)                                                 \
  do {                                                                         \
    if (!(condition)) {                                                        \
      LOG_ERROR("ASSERT_TRUE failed at %:%: condition was false",              \
               FMT_STR(__FILE_NAME__), FMT_UINT(__LINE__));                    \
      g_test_failed = true;                                                    \
      return;                                                                  \
    }                                                                          \
  } while(0)

#define assert_false(condition)                                                \
  do {                                                                         \
    if (condition) {                                                           \
      LOG_ERROR("ASSERT_FALSE failed at %:%: condition was true",              \
               FMT_STR(__FILE_NAME__), FMT_UINT(__LINE__));                    \
      g_test_failed = true;                                                    \
      return;                                                                  \
    }                                                                          \
  } while(0)

#define assert_mem_eq(type, actual, expected)                                  \
  do {                                                                         \
    if (!((actual == NULL && expected == NULL) ||                              \
          (actual != NULL && expected != NULL &&                               \
           memcmp(actual, expected, sizeof(type)) == 0))) {                    \
      LOG_ERROR("ASSERT_MEM_EQ failed at %:%", FMT_STR(__FILE_NAME__),         \
               FMT_UINT(__LINE__));                                            \
      g_test_failed = true;                                                    \
      return;                                                                  \
    }                                                                          \
  } while(0)

#endif // H_TEST
