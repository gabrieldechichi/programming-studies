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

void test_main(void) {
    RUN_TEST(test_ecs, &g_test_app_ctx.arena);
    RUN_TEST(test_ecs_components, &g_test_app_ctx.arena);
    RUN_TEST(test_ecs_tables, &g_test_app_ctx.arena);
    RUN_TEST(test_ecs_add_remove, &g_test_app_ctx.arena);
    RUN_TEST(test_ecs_query, &g_test_app_ctx.arena);
    RUN_TEST(test_ecs_query_cache, &g_test_app_ctx.arena);
    RUN_TEST(test_ecs_inout, &g_test_app_ctx.arena);
    RUN_TEST(test_ecs_change_detection, &g_test_app_ctx.arena);
    RUN_TEST(test_ecs_systems, &g_test_app_ctx.arena);

    if (is_main_thread()) {
        print_test_results();
    }
}

WASM_EXPORT(wasm_main)
int wasm_main(AppMemory *memory) {
    LOG_INFO("=== Test Runner Starting ===");

    g_test_app_ctx.arena = arena_from_buffer(memory->heap, memory->heap_size);
    g_test_app_ctx.num_threads = os_get_processor_count();
    app_ctx_set(&g_test_app_ctx);

    LOG_INFO("Thread count: %", FMT_UINT(g_test_app_ctx.num_threads));

    mcr_run(g_test_app_ctx.num_threads, KB(64), test_main, &g_test_app_ctx.arena);

    LOG_INFO("=== Test Runner Complete ===");
    return 0;
}

WASM_EXPORT(wasm_frame)
void wasm_frame(AppMemory *memory) {
    UNUSED(memory);
}
