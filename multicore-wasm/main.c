// Include lib .c files directly
#include "lib/string.c"
#include "lib/common.c"
#include "lib/memory.c"
#include "lib/string_builder.c"
#include "os/os_wasm.c"
#include "lib/thread.c"
#include "lib/thread_context.c"
#include "lib/multicore_runtime.c"

#define NUM_THREADS 8

void app_entrypoint(void) {
  ThreadContext *ctx = tctx_current();
  u8 idx = ctx->thread_idx;

  print_int("Hello from thread ", idx);

  // Sync all threads
  lane_sync();

  if (idx == 0) {
    print("Thread 0: all threads synchronized!");
  }
}

WASM_EXPORT(wasm_main)
int wasm_main(void) {
  print("Main: starting MCR test");

  // Setup arena allocator from heap
  u8 *heap = os_get_heap_base();
  ArenaAllocator arena = arena_from_buffer(heap, MB(16));

  print_int("Main: launching MCR with threads = ", NUM_THREADS);

  // Run the multicore runtime
  mcr_run((u8)NUM_THREADS, KB(64), app_entrypoint, &arena);

  print("Main: MCR completed, all threads joined");
  print("Done!");

  return 0;
}
