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

void test_main(void) {
    RUN_TEST(test_ecs);
    RUN_TEST(test_ecs_components);
    RUN_TEST(test_ecs_tables);
    RUN_TEST(test_ecs_add_remove);
    RUN_TEST(test_ecs_query);
    RUN_TEST(test_ecs_query_cache);
    RUN_TEST(test_ecs_inout);
    RUN_TEST(test_ecs_change_detection);
    RUN_TEST(test_ecs_systems);

    if (is_main_thread()) {
        print_test_results();
    }
}

WASM_EXPORT(wasm_main)
int wasm_main(AppMemory *memory) {
    LOG_INFO("=== Test Runner Starting ===");

    ArenaAllocator arena = arena_from_buffer(memory->heap, memory->heap_size);
    u8 num_threads = os_get_processor_count();

    LOG_INFO("Thread count: %", FMT_UINT(num_threads));

    mcr_run(num_threads, MB(64), test_main, &arena);

    LOG_INFO("=== Test Runner Complete ===");
    return 0;
}

WASM_EXPORT(wasm_frame)
void wasm_frame(AppMemory *memory) {
    UNUSED(memory);
}
