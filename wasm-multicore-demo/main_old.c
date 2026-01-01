// Include lib .c files directly
#include "lib/string.c"
#include "lib/common.c"
#include "lib/memory.c"
#include "lib/string_builder.c"
#include "os/os_wasm.c"
#include "lib/thread.c"
#include "lib/thread_context.c"
#include "lib/multicore_runtime.c"

#define NUM_THREADS 16

local_shared int seen[NUM_THREADS];

void app_entrypoint(void)
{
  u8 idx = tctx_current()->thread_idx;
  seen[idx]++;
  LOG_INFO("Thread % running", FMT_UINT(idx));
}

WASM_EXPORT(wasm_init)
int wasm_init(void)
{
  LOG_INFO("Testing thread indices with % threads", FMT_UINT(NUM_THREADS));

  // Setup arena allocator from heap
  u8 *heap = os_get_heap_base();
  ArenaAllocator arena = arena_from_buffer(heap, MB(16));

  mcr_run(NUM_THREADS, KB(64), app_entrypoint, &arena);

  int errors = 0;
  for (int i = 0; i < NUM_THREADS; i++)
  {
    if (seen[i] != 1)
    {
      LOG_ERROR("Error: thread % ran % times", FMT_UINT(i), FMT_UINT(seen[i]));
      errors++;
    }
  }

  if (errors == 0)
  {
    LOG_INFO("All thread indices 0-% are unique!", FMT_UINT(NUM_THREADS - 1));
  }

  return 0;
}
