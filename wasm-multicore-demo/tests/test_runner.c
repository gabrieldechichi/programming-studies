#include "context.h"
#include "app.h"
#include "lib/test.h"
#include "lib/multicore_runtime.h"
#include "os/os.h"

#include "ecs/ecs_entity.c"
#include "ecs/ecs_table.c"

#include "tests/test_ecs.c"
#include "tests/test_ecs_components.c"
#include "tests/test_ecs_tables.c"
#include "tests/test_ecs_add_remove.c"
#include "tests/test_ecs_query.c"
#include "tests/test_ecs_query_cache.c"
#include "tests/test_ecs_inout.c"
#include "tests/test_ecs_change_detection.c"
#include "tests/test_ecs_systems.c"

global AppContext g_test_app_ctx;

void register_tests(void) {
    REGISTER_TEST(test_ecs);
    REGISTER_TEST(test_ecs_components);
    REGISTER_TEST(test_ecs_tables);
    REGISTER_TEST(test_ecs_add_remove);
    REGISTER_TEST(test_ecs_query);
    REGISTER_TEST(test_ecs_query_cache);
    REGISTER_TEST(test_ecs_inout);
    REGISTER_TEST(test_ecs_change_detection);
    REGISTER_TEST(test_ecs_systems);
}

void test_main(void) {
    if (is_main_thread()) {
        register_tests();
    }
    lane_sync();

    test_runner_run();

    lane_sync();
    test_runner_print_results();
}

WASM_EXPORT(wasm_main)
int wasm_main(AppMemory *memory) {
    LOG_INFO("=== Test Runner Starting ===");

    g_test_app_ctx.arena = arena_from_buffer(memory->heap, memory->heap_size);
    g_test_app_ctx.num_threads = os_get_processor_count();
    app_ctx_set(&g_test_app_ctx);

    test_runner_init(&g_test_app_ctx.arena);

    LOG_INFO("Thread count: %", FMT_UINT(g_test_app_ctx.num_threads));

    mcr_run(g_test_app_ctx.num_threads, MB(4), test_main, &g_test_app_ctx.arena);

    LOG_INFO("=== Test Runner Complete ===");
    return 0;
}

WASM_EXPORT(wasm_frame)
void wasm_frame(AppMemory *memory) {
    UNUSED(memory);
}
