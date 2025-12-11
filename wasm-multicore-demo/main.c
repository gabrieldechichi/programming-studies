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
#include "renderer.c"
#include "lib/handle.c"
#include "lib/math.h"
#include "cube.h"

#define NUM_CUBES 1024

typedef struct {
  vec3 position;
  f32 rotation_rate;
} CubeData;

global CubeData cubes[NUM_CUBES];

global f32 g_time = 0.0f;
global Mesh_Handle g_cube_mesh;

global Barrier frame_barrier;
global ThreadContext main_thread_ctx;

typedef struct {
  ThreadContext *ctx;
} WorkerData;

// =============================================================================
// Frame Update - called by all threads
// =============================================================================

void app_update_and_render(void) {
  // Each thread processes a range of cubes
  Range_u64 range = lane_range(NUM_CUBES);

  for (u64 i = range.min; i < range.max; i++) {
    CubeData *cube = &cubes[i];

    // Build model matrix for this cube
    mat4 model;
    mat4_identity(model);

    // Translate to cube position
    glm_translate(model, cube->position);

    // Rotate based on time and per-cube rotation rate
    f32 angle = g_time * cube->rotation_rate;
    glm_rotate(model, angle, (vec3){0, 1, 0});
    glm_rotate(model, angle * 0.7f, (vec3){1, 0, 0});

    // Scale down cubes a bit
    glm_scale_uni(model, 0.3f);

    // Submit draw command (lock-free)
    renderer_draw_mesh(g_cube_mesh, model);
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

  // Initialize renderer (needs thread context for thread_count)
  renderer_init(&arena);

  // Upload cube mesh
  g_cube_mesh = renderer_upload_mesh(&(MeshDesc){
      .vertices = cube_vertices,
      .vertex_size = sizeof(cube_vertices),
      .indices = cube_indices,
      .index_size = sizeof(cube_indices),
      .index_count = CUBE_INDEX_COUNT,
      .index_format = GPU_INDEX_FORMAT_U16,
  });

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

  // All threads process their cube range
  app_update_and_render();

  // Sync with workers - wait for all to finish
  lane_sync();

  // End frame (main thread processes cmd queue, issues GPU calls)
  if (is_main_thread()) {
    renderer_end_frame();
  }
}
