// Include lib .c files directly
#include "lib/string.c"
#include "lib/common.c"
#include "lib/memory.c"
#include "lib/string_builder.c"
#include "lib/thread_context.h"
#include "os/os.h"
#include "os/os_wasm.c"
#include "lib/thread.c"
#include "lib/thread_context.c"
#include "lib/multicore_runtime.c"
#include "gpu.c"
#include "lib/math.h"

#define NUM_CUBES 64

// =============================================================================
// DEBUG TEST: Minimal barrier test without rendering
// =============================================================================

#define DEBUG_BARRIER_TEST 0

#if DEBUG_BARRIER_TEST

global Barrier frame_barrier;
global ThreadContext main_thread_ctx;

// Atomic counter: each thread increments this for each "cube" it processes
global u32 g_work_done_this_frame = 0;
// Frame sequence number set by main thread
global u32 g_frame_seq = 0;
// Track errors
global u32 g_error_count = 0;

typedef struct {
  ThreadContext *ctx;
} WorkerData;

void test_do_work(void) {
  Range_u64 range = lane_range(NUM_CUBES);
  u32 my_work = 0;

  for (u64 i = range.min; i < range.max; i++) {
    ins_atomic_u32_inc_eval(&g_work_done_this_frame);
    my_work++;
  }

  LOG_INFO("Frame %: Thread % did % work items (range %-%)",
           FMT_UINT(g_frame_seq),
           FMT_UINT(tctx_current()->thread_idx),
           FMT_UINT(my_work),
           FMT_UINT(range.min),
           FMT_UINT(range.max));
}

void worker_loop(void *arg) {
  WorkerData *data = (WorkerData *)arg;
  tctx_set_current(data->ctx);

  LOG_INFO("Worker % started, waiting for first barrier",
           FMT_UINT(tctx_current()->thread_idx));

  for (;;) {
    // Barrier 1: Wait for main thread to start frame
    lane_sync();

    // Do work
    test_do_work();

    // Barrier 2: Wait for all threads to finish work
    lane_sync();

    // Barrier 3: Wait for main thread to finish frame
    lane_sync();
  }
}

WASM_EXPORT(wasm_main)
int wasm_main(void) {
  LOG_INFO("=== BARRIER DEBUG TEST ===");
  LOG_INFO("Testing 3-barrier frame pattern with % work items", FMT_UINT(NUM_CUBES));

  // Setup arena allocator from heap
  u8 *heap = os_get_heap_base();
  ArenaAllocator arena = arena_from_buffer(heap, MB(16));

  u8 NUM_WORKERS = os_get_processor_count();
  LOG_INFO("os_get_processor_count() returned: %", FMT_UINT(NUM_WORKERS));
  LOG_INFO("Barrier will be created with count: %", FMT_UINT(NUM_WORKERS));
  LOG_INFO("Main thread = thread 0, workers = threads 1 to %", FMT_UINT(NUM_WORKERS - 1));

  // Allocate thread resources
  Thread *threads = ARENA_ALLOC_ARRAY(&arena, Thread, NUM_WORKERS);
  ThreadContext *thread_contexts =
      ARENA_ALLOC_ARRAY(&arena, ThreadContext, NUM_WORKERS);
  WorkerData *worker_data = ARENA_ALLOC_ARRAY(&arena, WorkerData, NUM_WORKERS);

  // Create barrier for all threads
  frame_barrier = barrier_alloc(NUM_WORKERS);

  // Setup main thread context (thread 0)
  main_thread_ctx = (ThreadContext){
      .thread_idx = 0,
      .thread_count = NUM_WORKERS,
      .barrier = &frame_barrier,
      .temp_arena =
          arena_from_buffer(ARENA_ALLOC_ARRAY(&arena, u8, KB(64)), KB(64)),
  };
  tctx_set_current(&main_thread_ctx);

  // Spawn worker threads (indices 1..N-1)
  for (u8 i = 1; i < NUM_WORKERS; i++) {
    thread_contexts[i] = (ThreadContext){
        .thread_idx = i,
        .thread_count = NUM_WORKERS,
        .barrier = &frame_barrier,
        .temp_arena =
            arena_from_buffer(ARENA_ALLOC_ARRAY(&arena, u8, KB(64)), KB(64)),
    };
    worker_data[i] = (WorkerData){.ctx = &thread_contexts[i]};
    threads[i] = thread_launch(worker_loop, &worker_data[i]);
  }

  LOG_INFO("All workers spawned. Ready for frames.");
  return 0;
}

WASM_EXPORT(wasm_frame)
void wasm_frame(void) {
  g_frame_seq++;

  // Reset work counter BEFORE barrier
  g_work_done_this_frame = 0;

  LOG_INFO("=== Frame % START (reset counter to 0) ===", FMT_UINT(g_frame_seq));

  // Barrier 1: Release workers to do work
  lane_sync();

  // Main thread also does work
  test_do_work();

  // Barrier 2: Wait for all threads to finish work
  lane_sync();

  // Check result
  u32 work_done = g_work_done_this_frame;
  if (work_done != NUM_CUBES) {
    LOG_ERROR("Frame %: FAIL! Expected % work items, got %",
              FMT_UINT(g_frame_seq), FMT_UINT(NUM_CUBES), FMT_UINT(work_done));
    g_error_count++;
  } else {
    LOG_INFO("Frame %: PASS! Work done = % (expected %)",
             FMT_UINT(g_frame_seq), FMT_UINT(work_done), FMT_UINT(NUM_CUBES));
  }

  // Barrier 3: Signal frame complete
  lane_sync();

  LOG_INFO("=== Frame % END (errors so far: %) ===",
           FMT_UINT(g_frame_seq), FMT_UINT(g_error_count));
}

#else
// Original rendering code below...

typedef struct {
  vec3 position;
  f32 rotation_rate;
} CubeData;

global CubeData cubes[NUM_CUBES];
global f32 g_time = 0.0f;
global Barrier frame_barrier;
global ThreadContext main_thread_ctx;

typedef struct {
  ThreadContext *ctx;
} WorkerData;

void app_update_and_render(void) {
  // Each thread processes a range of cubes
  Range_u64 range = lane_range(NUM_CUBES);

  for (u64 i = range.min; i < range.max; i++) {
    CubeData *cube = &cubes[i];

    mat4 model;
    mat4_identity(model);
    glm_translate(model, cube->position);

    f32 angle = g_time * cube->rotation_rate;
    glm_rotate(model, angle, (vec3){0, 1, 0});
    glm_rotate(model, angle * 0.7f, (vec3){1, 0, 0});
    glm_scale_uni(model, 0.3f);

    renderer_draw_mesh(model);
  }
}

void worker_loop(void *arg) {
  WorkerData *data = (WorkerData *)arg;
  tctx_set_current(data->ctx);

  for (;;) {
    // Barrier 1: Wait for main thread to call renderer_begin_frame()
    lane_sync();

    // All threads process their cube range
    app_update_and_render();

    // Barrier 2: Wait for all threads to finish work
    lane_sync();
  }
}

// =============================================================================
// Initialization
// =============================================================================

void init_cubes(void) {
  // Arrange cubes in a grid
  u32 grid_size = 8; // 8x8 = 64 cubes
  f32 spacing = 2.5f;
  f32 offset = (grid_size - 1) * spacing * 0.5f;

  for (u32 i = 0; i < NUM_CUBES; i++) {
    u32 x = i % grid_size;
    u32 z = i / grid_size;

    cubes[i].position[0] = x * spacing - offset;
    cubes[i].position[1] = 0.0f;
    cubes[i].position[2] = z * spacing - offset;

    // Each cube gets a different rotation rate
    cubes[i].rotation_rate = 0.5f + (f32)i * 0.05f;
  }
}

WASM_EXPORT(wasm_main)
int wasm_main(void) {
  LOG_INFO("Initializing GPU...");
  gpu_init();

  // Setup arena allocator from heap
  u8 *heap = os_get_heap_base();
  ArenaAllocator arena = arena_from_buffer(heap, MB(16));

  renderer_init(&arena);

  // Initialize cube data
  init_cubes();

  // Total threads = main thread (0) + worker threads (1..N)
  u8 NUM_WORKERS = os_get_processor_count();

  LOG_INFO("Spawning % worker threads...", FMT_UINT(NUM_WORKERS));

  // Allocate thread resources
  Thread *threads = ARENA_ALLOC_ARRAY(&arena, Thread, NUM_WORKERS);
  ThreadContext *thread_contexts =
      ARENA_ALLOC_ARRAY(&arena, ThreadContext, NUM_WORKERS);
  WorkerData *worker_data = ARENA_ALLOC_ARRAY(&arena, WorkerData, NUM_WORKERS);

  // Create barrier for all threads
  frame_barrier = barrier_alloc(NUM_WORKERS);

  // Setup main thread context (thread 0)
  main_thread_ctx = (ThreadContext){
      .thread_idx = 0,
      .thread_count = NUM_WORKERS,
      .barrier = &frame_barrier,
      .temp_arena =
          arena_from_buffer(ARENA_ALLOC_ARRAY(&arena, u8, KB(64)), KB(64)),
  };
  tctx_set_current(&main_thread_ctx);

  // Spawn worker threads (indices 1..N-1)
  for (u8 i = 1; i < NUM_WORKERS; i++) {
    thread_contexts[i] = (ThreadContext){
        .thread_idx = i,
        .thread_count = NUM_WORKERS,
        .barrier = &frame_barrier,
        .temp_arena =
            arena_from_buffer(ARENA_ALLOC_ARRAY(&arena, u8, KB(64)), KB(64)),
    };
    worker_data[i] = (WorkerData){.ctx = &thread_contexts[i]};
    threads[i] = thread_launch(worker_loop, &worker_data[i]);
  }

  LOG_INFO("Initialization complete. % cubes, % threads.", FMT_UINT(NUM_CUBES),
           FMT_UINT(NUM_WORKERS));
  return 0;
}

WASM_EXPORT(wasm_frame)
void wasm_frame(void) {
  LOG_INFO("Main thread: frame start");
  // Update time
  g_time += 0.016f; // ~60fps

  // Setup view and projection (main thread only, before barrier)
  mat4 view, proj;
  glm_lookat((vec3){0, 15, 25}, (vec3){0, 0, 0}, (vec3){0, 1, 0}, view);
  glm_perspective(RAD(45.0f), 16.0f / 9.0f, 0.1f, 100.0f, proj);

  // Begin frame (clears, sets view/proj, resets cmd queue)
  if (is_main_thread()) {
    renderer_begin_frame(view, proj, (GpuColor){0.05f, 0.05f, 0.08f, 1.0f});
  }

  // Sync with workers - start parallel work
  lane_sync();

  LOG_INFO("Main thread: update and render called");
  // All threads process their cube range
  app_update_and_render();

  // Sync with workers - wait for all to finish
  lane_sync();
  LOG_INFO("Main thread: update and render - all threads done");

  // End frame (main thread processes cmd queue, issues GPU calls)
  if (is_main_thread()) {
    renderer_end_frame();
  }
  LOG_INFO("Main thread: renderer_end_frame done");

  LOG_INFO("Main thread: end frame lane sync done");
}

#endif // DEBUG_BARRIER_TEST
