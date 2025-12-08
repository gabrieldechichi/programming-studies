#include "lib/memory.h"
#include "lib/string.c"
#include "lib/string_builder.c"
#include "lib/common.c"
#include "os/os_wasm.c"
#include "lib/memory.c"

#define SHARED_ARRAY_SIZE 12000
#define NUM_THREADS 8

// Thread arguments - stored in shared memory so workers can read
typedef struct {
  i32 idx;
  i32 num_threads;
  Barrier barrier;
  i32 *shared_array;
} ThreadArgs;

// Global pointer to args array (in shared memory)
local_shared ThreadArgs *g_args = 0;

WASM_EXPORT(thread_func)
void thread_func(void *arg) {
  i32 idx = (i32)(uintptr)arg;
  ThreadArgs *args = &g_args[idx];

  // Thread 0 announces array is ready
  if (idx == 0) {
    print("Thread 0: shared array ready");
  }

  // Wait for all threads to be ready
  os_barrier_wait(args->barrier);

  // Calculate this thread's chunk
  i32 chunk = SHARED_ARRAY_SIZE / args->num_threads;
  i32 start = idx * chunk;
  i32 end = (idx == args->num_threads - 1) ? SHARED_ARRAY_SIZE : start + chunk;

  // Fill the chunk with sequential values
  for (i32 i = start; i < end; i++) {
    args->shared_array[i] = i;
  }

  print_int("Thread filled chunk, idx=", idx);

  // Wait for all threads to finish filling
  os_barrier_wait(args->barrier);
}

WASM_EXPORT(wasm_main)
int wasm_main(void) {
  print("Main: starting parallel array fill test");

  // Setup arena allocator from heap
  u8 *heap = os_get_heap_base();
  ArenaAllocator arena = arena_from_buffer(heap, MB(16));

  // Allocate shared array
  i32 *shared_array = ARENA_ALLOC_ARRAY(&arena, i32, SHARED_ARRAY_SIZE);
  if (!shared_array) {
    print("ERROR: Failed to allocate shared array");
    return 1;
  }

  // Allocate thread args
  g_args = ARENA_ALLOC_ARRAY(&arena, ThreadArgs, NUM_THREADS);
  if (!g_args) {
    print("ERROR: Failed to allocate thread args");
    return 1;
  }

  // Allocate thread handles
  Thread *threads = ARENA_ALLOC_ARRAY(&arena, Thread, NUM_THREADS);
  if (!threads) {
    print("ERROR: Failed to allocate thread handles");
    return 1;
  }

  print_int("Main: num threads = ", NUM_THREADS);
  print_int("Main: array size = ", SHARED_ARRAY_SIZE);

  // Create barrier
  Barrier barrier = os_barrier_alloc(NUM_THREADS);

  // Setup args and spawn threads
  for (i32 i = 0; i < NUM_THREADS; i++) {
    g_args[i] = (ThreadArgs){
        .idx = i,
        .num_threads = NUM_THREADS,
        .barrier = barrier,
        .shared_array = shared_array,
    };
    threads[i] = os_thread_launch(thread_func, (void *)(uintptr)i);
  }

  print("Main: all threads launched, waiting for joins");

  // Join all threads
  for (i32 i = 0; i < NUM_THREADS; i++) {
    os_thread_join(threads[i], 0);
  }

  print("Main: all threads joined, verifying array...");

  // Verify the array was filled correctly
  i32 errors = 0;
  for (i32 i = 0; i < SHARED_ARRAY_SIZE; i++) {
    if (shared_array[i] != i) {
      if (errors < 10) {
        print_int("ERROR at index ", i);
      }
      errors++;
      if (errors >= 10) {
        print("Too many errors, stopping verification");
        break;
      }
    }
  }

  if (errors == 0) {
    print_int("SUCCESS: All values verified correctly! Count = ", SHARED_ARRAY_SIZE);
  } else {
    print_int("FAILED: Error count = ", errors);
  }

  // Cleanup
  os_barrier_release(barrier);

  print("Done!");
  return 0;
}
