#include <stdio.h>
#include <stdlib.h>
#include <emscripten/threading.h>

// Include lib .c files directly
#include "lib/string.c"
#include "lib/common.c"
#include "lib/memory.c"
#include "lib/string_builder.c"
#include "lib/thread.c"
#include "lib/thread_context.c"
#include "lib/multicore_runtime.c"
#include "os/os_wasm.c"

#define TEST_ARRAY_SIZE 12000
#define MIN_THREADS 16

arr_define_concurrent(int);

// Shared state across all threads (file-scope statics)
local_shared ConcurrentArray(int) shared_array = { 0 };

void app_entrypoint(void) {
  ThreadContext *ctx = tctx_current();
  u8 idx = ctx->thread_idx;

  // Thread 0 allocates the shared array buffer
  if (idx == 0) {
    shared_array.items = malloc(TEST_ARRAY_SIZE * sizeof(int));
    shared_array.cap = TEST_ARRAY_SIZE;
    shared_array.len_atomic = 0;
    printf("Thread 0 allocated shared array at %p\n",
           (void *)shared_array.items);
  }

  // Wait for allocation to complete
  lane_sync();

  // Get this thread's range using lane_range
  Range_u64 range = lane_range(TEST_ARRAY_SIZE);

  // Each thread appends its values using atomic operations
  for (u64 i = range.min; i < range.max; i++) {
    concurrent_arr_append(shared_array, (int)i);
  }

  printf("Thread %d appended values [%llu, %llu)\n", idx, range.min, range.max);

  // Wait for all threads to finish filling
  lane_sync();
}

int main() {
  int num_cores = emscripten_num_logical_cores();
  if (num_cores < MIN_THREADS) {
    num_cores = MIN_THREADS;
  }
  printf("Detected %d cores, using %d threads\n",
         emscripten_num_logical_cores(), num_cores);

  // Run the multicore runtime
  mcr_run((u8)num_cores, KB(64), app_entrypoint);

  // Verify the array was filled correctly
  u32 len = concurrent_arr_len(shared_array);
  printf("Verifying array (len=%u)...\n", len);

  // With concurrent append, values won't be in order - just verify all values
  // 0..TEST_ARRAY_SIZE-1 exist
  int *seen = calloc(TEST_ARRAY_SIZE, sizeof(int));
  int errors = 0;
  for (u32 i = 0; i < len; i++) {
    int val = concurrent_arr_get(shared_array, i);
    if (val < 0 || val >= TEST_ARRAY_SIZE) {
      printf("Error: value %d out of range at index %u\n", val, i);
      errors++;
    } else {
      seen[val]++;
    }
  }

  // Check all values appeared exactly once
  for (int i = 0; i < TEST_ARRAY_SIZE; i++) {
    if (seen[i] != 1) {
      printf("Error: value %d appeared %d times (expected 1)\n", i, seen[i]);
      errors++;
      if (errors > 10) {
        printf("Too many errors, stopping verification\n");
        break;
      }
    }
  }

  if (errors == 0) {
    printf("All %d values verified correctly!\n", TEST_ARRAY_SIZE);
  }

  // Cleanup
  free(seen);
  free(shared_array.items);

  printf("Done!\n");
  return 0;
}
