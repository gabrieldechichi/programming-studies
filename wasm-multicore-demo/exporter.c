#include "lib/typedefs.h"
#include "lib/memory.h"
#include "lib/string.h"
#include "lib/array.h"
#include "lib/thread_context.h"
#include "lib/multicore_runtime.h"
#include "os/os.h"

#include "lib/string.c"
#include "lib/common.c"
#include "lib/memory.c"
#include "lib/allocator_pool.c"
#include "lib/string_builder.c"
#include "lib/cmd_line.c"
#include "lib/thread.c"
#include "lib/thread_context.c"
#include "lib/task.c"
#include "lib/multicore_runtime.c"
#include "os/os_win32.c"

local_shared i32 g_argc;
local_shared char **g_argv;

void print_usage(void) {
    LOG_INFO("Usage: exporter --input <path.glb> --output <path.hasset>");
    LOG_INFO("Options:");
    LOG_INFO("  --input   Path to input .glb file");
    LOG_INFO("  --output  Path to output .hasset file");
}

void entrypoint(void) {
    local_shared Allocator allocator;
    local_shared ArenaAllocator arena;
    local_shared b32 parse_success;
    local_shared String input_path;
    local_shared String output_path;

    if (is_main_thread()) {
        os_time_init();

        const u64 arena_size = MB(256);
        void *memory = os_allocate_memory(arena_size);
        arena = arena_from_buffer(memory, arena_size);
        allocator = make_arena_allocator(&arena);

        CmdLineParser parser = cmdline_create(&allocator);

        cmdline_add_option(&parser, "input");
        cmdline_add_option(&parser, "output");

        parse_success = cmdline_parse(&parser, g_argc, g_argv);

        if (parse_success) {
            input_path = cmdline_get_option(&parser, "input");
            output_path = cmdline_get_option(&parser, "output");

            if (input_path.len == 0 || output_path.len == 0) {
                LOG_ERROR("Missing required options --input and --output");
                print_usage();
                parse_success = false;
            }
        }

        if (!parse_success) {
            print_usage();
        }
    }
    lane_sync();

    if (!parse_success) {
        return;
    }

    if (is_main_thread()) {
        LOG_INFO("Exporter started");
        LOG_INFO("  Input:  %", FMT_STR_VIEW(input_path));
        LOG_INFO("  Output: %", FMT_STR_VIEW(output_path));

        ThreadContext *tctx = tctx_current();
        LOG_INFO("  Thread count: %", FMT_UINT(tctx->thread_count));
    }
    lane_sync();

    ThreadContext *tctx = tctx_current();
    LOG_INFO("Thread % ready", FMT_UINT(tctx->thread_idx));
    lane_sync();

    if (is_main_thread()) {
        LOG_INFO("Export complete (placeholder)");
    }
}

int main(int argc, char *argv[]) {
    g_argc = argc;
    g_argv = argv;

    os_init();
    i32 num_cores = os_get_processor_count();
    i32 thread_count = MAX(1, num_cores);

    const u64 runtime_arena_size = MB(64);
    void *runtime_memory = os_allocate_memory(runtime_arena_size);
    ArenaAllocator runtime_arena = arena_from_buffer(runtime_memory, runtime_arena_size);

    mcr_run((u8)thread_count, MB(4), entrypoint, &runtime_arena);

    return 0;
}
