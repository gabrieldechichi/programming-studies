#include "os/os_wasm.c"

#define NUM_THREADS 8

// Global barrier - threads need access to it
local_shared Barrier g_barrier;

WASM_EXPORT(thread_func)
void thread_func(void *arg) {
  (void)arg;
  print("Thread: before barrier");
  os_barrier_wait(g_barrier);
  print("Thread: after barrier");
}

WASM_EXPORT(wasm_main)
int wasm_main() {
  print("Main: starting barrier test with 8 threads");

  // Create barrier for 8 threads
  g_barrier = os_barrier_alloc(NUM_THREADS);

  // Spawn 8 threads
  Thread threads[NUM_THREADS];
  for (int i = 0; i < NUM_THREADS; i++) {
    threads[i] = os_thread_launch(thread_func, (void *)(uintptr)i);
  }

  print("Main: all threads launched, waiting for joins");

  // Join all threads
  for (int i = 0; i < NUM_THREADS; i++) {
    os_thread_join(threads[i], 0);
  }

  print("Main: all threads joined, done!");

  return 0;
}
