// Include lib .c files directly
#include "lib/string.c"
#include "lib/common.c"
#include "lib/memory.c"
#include "lib/string_builder.c"
#include "os/os_wasm.c"
#include "lib/thread.c"
#include "lib/thread_context.c"
#include "lib/multicore_runtime.c"

#define TEST_ARRAY_SIZE 12000
#define MIN_THREADS 16

arr_define_concurrent(int);

// Shared state across all threads
local_shared ConcurrentArray(int) shared_array = {0};

void app_entrypoint(void) {
  ThreadContext *ctx = tctx_current();
  u8 idx = ctx->thread_idx;

  // Thread 0 allocates the shared array buffer
  if (idx == 0) {
    shared_array.items = arena_alloc(&ctx->temp_arena, TEST_ARRAY_SIZE * sizeof(int));
    shared_array.cap = TEST_ARRAY_SIZE;
    shared_array.len_atomic = 0;
    LOG_INFO("Thread 0 allocated shared array at %", FMT_HEX((u64)shared_array.items));
  }

  // Wait for allocation to complete
  lane_sync();

  // Get this thread's range using lane_range
  Range_u64 range = lane_range(TEST_ARRAY_SIZE);

  // Each thread appends its values using atomic operations
  for (u64 i = range.min; i < range.max; i++) {
    concurrent_arr_append(shared_array, (int)i);
  }

  LOG_INFO("Thread % appended values [%, %)", FMT_UINT(idx), FMT_UINT((u32)range.min), FMT_UINT((u32)range.max));

  // Wait for all threads to finish filling
  lane_sync();
}

WASM_EXPORT(wasm_main)
int wasm_main(void) {
  // Setup arena allocator from heap
  u8 *heap = os_get_heap_base();
  ArenaAllocator arena = arena_from_buffer(heap, MB(16));

  LOG_INFO("Running concurrent array test with % threads", FMT_UINT(MIN_THREADS));

  // Run the multicore runtime
  mcr_run(MIN_THREADS, KB(64), app_entrypoint, &arena);

  // Verify the array was filled correctly
  u32 len = concurrent_arr_len(shared_array);
  LOG_INFO("Verifying array (len=%)...", FMT_UINT(len));

  // With concurrent append, values won't be in order - just verify all values 0..TEST_ARRAY_SIZE-1 exist
  int *seen = arena_alloc(&arena, TEST_ARRAY_SIZE * sizeof(int));
  for (int i = 0; i < TEST_ARRAY_SIZE; i++) {
    seen[i] = 0;
  }

  int errors = 0;
  for (u32 i = 0; i < len; i++) {
    int val = concurrent_arr_get(shared_array, i);
    if (val < 0 || val >= TEST_ARRAY_SIZE) {
      LOG_ERROR("Error: value % out of range at index %", FMT_INT(val), FMT_UINT(i));
      errors++;
    } else {
      seen[val]++;
    }
  }

  // Check all values appeared exactly once
  for (int i = 0; i < TEST_ARRAY_SIZE; i++) {
    if (seen[i] != 1) {
      LOG_ERROR("Error: value % appeared % times (expected 1)", FMT_INT(i), FMT_INT(seen[i]));
      errors++;
      if (errors > 10) {
        LOG_ERROR("Too many errors, stopping verification");
        break;
      }
    }
  }

  if (errors == 0) {
    LOG_INFO("All % values verified correctly!", FMT_INT(TEST_ARRAY_SIZE));
  }

  LOG_INFO("Done!");

  return 0;
}
