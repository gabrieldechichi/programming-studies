#include "lib/memory.h"
#include "lib/string.c"
#include "lib/string_builder.c"
#include "lib/common.c"
#include "os/os_wasm.c"
#include "lib/memory.c"

// Simple global in shared memory
local_shared i32 g_value = 0;

WASM_EXPORT(thread_func)
void thread_func(void *arg) {
  i32 my_arg = (i32)(uintptr)arg;
  print_int("Thread received arg: ", my_arg);
  print_int("Thread sees g_value: ", g_value);
}

WASM_EXPORT(wasm_main)
int wasm_main(void) {
  print("Main: setting g_value to 42");
  g_value = 42;

  print("Main: spawning 4 threads");
  Thread t0 = os_thread_launch(thread_func, (void *)(uintptr)0);
  Thread t1 = os_thread_launch(thread_func, (void *)(uintptr)1);
  Thread t2 = os_thread_launch(thread_func, (void *)(uintptr)2);
  Thread t3 = os_thread_launch(thread_func, (void *)(uintptr)3);

  print("Main: joining threads");
  os_thread_join(t0, 0);
  os_thread_join(t1, 0);
  os_thread_join(t2, 0);
  os_thread_join(t3, 0);

  print("Main: done");
  return 0;
}
