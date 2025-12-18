#include "context.h"
#include "lib/thread_context.h"
#include "os/os.h"
#include "app.h"

global Barrier frame_barrier;
global ThreadContext main_thread_ctx;
global AppMemory *g_memory;
global AppContext g_app_ctx;

typedef struct {
  ThreadContext *ctx;
} WorkerData;

void worker_loop(void *arg) {
  WorkerData *data = (WorkerData *)arg;
  tctx_set_current(data->ctx);

  lane_sync(); // Wait for init
  app_init(g_memory);
  arena_reset(&tctx_current()->temp_arena);
  lane_sync(); // Init done

  for (;;) {
    lane_sync(); // Wait for frame start (main sets g_memory before this)
    app_update_and_render(g_memory);
  arena_reset(&tctx_current()->temp_arena);
    lane_sync(); // Frame done
  }
}

WASM_EXPORT(wasm_main)
int wasm_main(AppMemory *memory) {
  LOG_INFO("Initializing...");

  g_memory = memory;

  g_app_ctx.arena = arena_from_buffer(memory->heap, memory->heap_size);
  g_app_ctx.num_threads = os_get_processor_count();
  app_ctx_set(&g_app_ctx);

  LOG_INFO("Spawning % worker threads...", FMT_UINT(g_app_ctx.num_threads));

  // Allocate thread resources
  Thread *threads =
      ARENA_ALLOC_ARRAY(&g_app_ctx.arena, Thread, g_app_ctx.num_threads);
  ThreadContext *thread_contexts =
      ARENA_ALLOC_ARRAY(&g_app_ctx.arena, ThreadContext, g_app_ctx.num_threads);
  WorkerData *worker_data =
      ARENA_ALLOC_ARRAY(&g_app_ctx.arena, WorkerData, g_app_ctx.num_threads);

  // Create barrier for all threads
  frame_barrier = barrier_alloc(g_app_ctx.num_threads);

  // Setup main thread context (thread 0)
  main_thread_ctx = (ThreadContext){
      .thread_idx = 0,
      .thread_count = g_app_ctx.num_threads,
      .barrier = &frame_barrier,
      .temp_arena = arena_from_buffer(
          ARENA_ALLOC_ARRAY(&g_app_ctx.arena, u8, KB(64)), KB(64)),
  };
  tctx_set_current(&main_thread_ctx);

  // Spawn worker threads (indices 1..N-1)
  for (u8 i = 1; i < g_app_ctx.num_threads; i++) {
    thread_contexts[i] = (ThreadContext){
        .thread_idx = i,
        .thread_count = g_app_ctx.num_threads,
        .barrier = &frame_barrier,
        .temp_arena = arena_from_buffer(
            ARENA_ALLOC_ARRAY(&g_app_ctx.arena, u8, KB(64)), KB(64)),
    };
    worker_data[i] = (WorkerData){.ctx = &thread_contexts[i]};
    threads[i] = thread_launch(worker_loop, &worker_data[i]);
  }

  // Sync with workers for init phase
  lane_sync();
  app_init(memory);
  arena_reset(&tctx_current()->temp_arena);
  lane_sync();

  return 0;
}

WASM_EXPORT(wasm_frame)
void wasm_frame(AppMemory *memory) {

  lane_sync(); // Release workers - they can now read g_memory

  app_update_and_render(memory);
  arena_reset(&tctx_current()->temp_arena);

  lane_sync(); // Wait for all threads to finish
}
