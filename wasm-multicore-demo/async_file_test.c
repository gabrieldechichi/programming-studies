#include "lib/typedefs.h"
#include "lib/memory.h"
#include "lib/string.h"
#include "lib/array.h"
#include "lib/assert.h"
#include "lib/thread_context.h"
#include "lib/multicore_runtime.h"
#include "os/os.h"

#include "lib/string.c"
#include "lib/common.c"
#include "lib/memory.c"
#include "lib/allocator_pool.c"
#include "lib/string_builder.c"
#include "lib/thread.c"
#include "lib/thread_context.c"
#include "lib/task.c"
#include "lib/multicore_runtime.c"
#include "os/os_win32.c"

typedef struct {
    OsFileOp *op;
    const char *path;
    u32 size;
    volatile b32 completed;
    volatile b32 error;
} AsyncFileLoad;

local_shared Allocator g_allocator;
local_shared ArenaAllocator g_arena;
local_shared OsFileList g_files;
local_shared AsyncFileLoad *g_loads;
local_shared volatile u64 g_total_bytes;
local_shared volatile i32 g_files_loaded;
local_shared volatile i32 g_errors;

void entrypoint(void) {
    if (is_main_thread()) {
        os_time_init();

        const u64 arena_size = MB(64);
        void *memory = os_allocate_memory(arena_size);
        g_arena = arena_from_buffer(memory, arena_size);
        g_allocator = make_arena_allocator(&g_arena);

        g_files = os_list_files("public", "", &g_allocator);
        g_loads = ALLOC_ARRAY(&g_allocator, AsyncFileLoad, g_files.count);
        memset(g_loads, 0, sizeof(AsyncFileLoad) * g_files.count);

        g_total_bytes = 0;
        g_files_loaded = 0;
        g_errors = 0;

        LOG_INFO("=== Async File Load Test (IOCP + MCR) ===");
        LOG_INFO("Threads: %, Files: %",
                 FMT_UINT(tctx_current()->thread_count),
                 FMT_INT(g_files.count));
    }
    lane_sync();

    if (g_files.count == 0) {
        if (is_main_thread()) {
            LOG_WARN("No files found in public/ directory");
        }
        return;
    }

    u64 start_time = os_time_now();

    Range_u64 range = lane_range((u64)g_files.count);
    for (u64 i = range.min; i < range.max; i++) {
        AsyncFileLoad *load = &g_loads[i];
        load->path = g_files.paths[i];
        load->op = os_start_read_file(g_files.paths[i]);
        load->completed = false;
        load->error = false;

        if (!load->op) {
            load->error = true;
            load->completed = true;
            ins_atomic_u32_add_eval(&g_errors, 1);
        }
    }
    lane_sync();

    i32 my_pending = (i32)(range.max - range.min);
    for (u64 i = range.min; i < range.max; i++) {
        if (g_loads[i].error) my_pending--;
    }

    while (my_pending > 0) {
        for (u64 i = range.min; i < range.max; i++) {
            AsyncFileLoad *load = &g_loads[i];
            if (load->completed)
                continue;

            OsFileReadState state = os_check_read_file(load->op);

            if (state == OS_FILE_READ_STATE_COMPLETED) {
                load->size = (u32)os_get_file_size(load->op);
                load->completed = true;
                my_pending--;

                ins_atomic_u64_add_eval(&g_total_bytes, load->size);
                ins_atomic_u32_add_eval(&g_files_loaded, 1);
            } else if (state == OS_FILE_READ_STATE_ERROR) {
                load->completed = true;
                load->error = true;
                my_pending--;
                ins_atomic_u32_add_eval(&g_errors, 1);
            }
        }

        if (my_pending > 0) {
            os_sleep(10);
        }
    }
    lane_sync();

    u64 end_time = os_time_now();

    if (is_main_thread()) {
        f64 elapsed_ms = os_ticks_to_ms(os_time_diff(end_time, start_time));

        LOG_INFO("=== Results ===");
        LOG_INFO("  Files loaded: %", FMT_INT(g_files_loaded));
        LOG_INFO("  Errors: %", FMT_INT(g_errors));
        LOG_INFO("  Total bytes: %", FMT_UINT(g_total_bytes));
        LOG_INFO("  Time: % ms", FMT_FLOAT(elapsed_ms));

        if (elapsed_ms > 0) {
            f64 throughput = ((f64)g_total_bytes / 1024.0 / 1024.0) / (elapsed_ms / 1000.0);
            LOG_INFO("  Throughput: % MB/s", FMT_FLOAT(throughput));
        }

        for (i32 i = 0; i < g_files.count; i++) {
            AsyncFileLoad *load = &g_loads[i];
            if (load->error) {
                LOG_ERROR("  FAILED: %", FMT_STR(load->path));
            } else {
                LOG_INFO("  OK: % (% bytes)", FMT_STR(load->path), FMT_UINT(load->size));
            }
        }
    }
}

int main(int argc, char *argv[]) {
    UNUSED(argc);
    UNUSED(argv);

    os_init();
    i32 num_cores = os_get_processor_count();
    i32 thread_count = MAX(1, num_cores);

    const u64 runtime_arena_size = GB(1);
    void *runtime_memory = os_allocate_memory(runtime_arena_size);
    ArenaAllocator runtime_arena = arena_from_buffer(runtime_memory, runtime_arena_size);

    mcr_run((u8)thread_count, MB(32), entrypoint, &runtime_arena);

    return 0;
}
