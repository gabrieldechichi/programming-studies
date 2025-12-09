// Include lib .c files directly
#include "lib/string.c"
#include "lib/common.c"
#include "lib/memory.c"
#include "lib/string_builder.c"
#include "os/os_wasm.c"
#include "lib/thread.c"
#include "lib/thread_context.c"
#include "lib/multicore_runtime.c"

// Shared state for verification
local_shared i32 *g_seen = 0;
local_shared i32 g_num_threads = 0;

void app_entrypoint(void) {
  ThreadContext *ctx = tctx_current();
  u8 idx = ctx->thread_idx;

  LOG_INFO("Hello from thread %", FMT_UINT(idx));

  // Mark this thread's index as seen (atomic increment to detect duplicates)
  ins_atomic_u32_inc_eval(&g_seen[idx]);

  lane_sync();

  // Thread 0 verifies all indices are unique
  if (idx == 0) {
    i32 errors = 0;
    for (i32 i = 0; i < g_num_threads; i++) {
      if (g_seen[i] != 1) {
        LOG_ERROR("Thread idx % appeared % times (expected 1)", FMT_UINT(i),
                  FMT_UINT(g_seen[i]));
        errors++;
      }
    }
    if (errors == 0) {
      LOG_INFO("SUCCESS: All % thread indices unique and in range [0, %)",
               FMT_UINT(g_num_threads), FMT_UINT(g_num_threads));
    } else {
      LOG_ERROR("FAILED: % errors in thread index verification",
                FMT_UINT(errors));
    }
  }
}

WASM_EXPORT(wasm_main)
int wasm_main(void) {
  LOG_INFO("Main: starting MCR test");

  // Setup arena allocator from heap
  u8 *heap = os_get_heap_base();
  ArenaAllocator arena = arena_from_buffer(heap, MB(16));

  g_num_threads = os_get_processor_count();

  // Allocate seen array for verification
  g_seen = ARENA_ALLOC_ARRAY(&arena, i32, g_num_threads);
  memset(g_seen, 0, g_num_threads * sizeof(i32));

  LOG_INFO("Main: launching MCR with % threads", FMT_UINT(g_num_threads));

  // Run the multicore runtime
  mcr_run((u8)g_num_threads, KB(64), app_entrypoint, &arena);

  LOG_INFO("Main: MCR completed, all threads joined");
  LOG_INFO("Done!");

  return 0;
}
