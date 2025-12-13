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
#include "lib/hash.h"
#include "lib/random.h"
#include "lib/random.c"
#include "cube.h"
#include "renderer.h"

#define NUM_CUBES 100000

// Collision constants
#define GRID_SIZE 8192
#define MAX_PER_BUCKET 64
#define CELL_SIZE 2.0f
#define CUBE_RADIUS 0.5f
#define BOUNDS 50.0f
#define CUBE_SPEED 10.0f

// Bucket entry for spatial hash grid - stores position for cache-friendly collision checks
typedef struct {
  f32 px, py, pz;  // Position (unpacked for cache efficiency)
  u32 cube_idx;    // Link back to full CubeData
} BucketEntry;

// Fixed-size bucket for cache-friendly traversal
typedef struct {
  u32 count;
  BucketEntry entries[MAX_PER_BUCKET];
} Bucket;

global Bucket g_buckets[GRID_SIZE];

// Instanced vertex shader - reads model matrices from storage buffer
static const char *instanced_vs =
    "struct GlobalUniforms {\n"
    "    model: mat4x4<f32>,\n"
    "    view: mat4x4<f32>,\n"
    "    proj: mat4x4<f32>,\n"
    "    view_proj: mat4x4<f32>,\n"
    "};\n"
    "\n"
    "struct InstanceData {\n"
    "    model: mat4x4<f32>,\n"
    "};\n"
    "\n"
    "@group(0) @binding(0) var<uniform> global: GlobalUniforms;\n"
    "@group(0) @binding(1) var<uniform> color: vec4<f32>;\n"
    "@group(1) @binding(0) var<storage, read> instances: array<InstanceData>;\n"
    "\n"
    "struct VertexInput {\n"
    "    @location(0) position: vec3<f32>,\n"
    "    @location(1) vertex_color: vec4<f32>,\n"
    "};\n"
    "\n"
    "struct VertexOutput {\n"
    "    @builtin(position) position: vec4<f32>,\n"
    "    @location(0) vertex_color: vec4<f32>,\n"
    "    @location(1) material_color: vec4<f32>,\n"
    "};\n"
    "\n"
    "@vertex\n"
    "fn vs_main(@builtin(instance_index) instance_idx: u32, in: VertexInput) -> VertexOutput {\n"
    "    var out: VertexOutput;\n"
    "    let model = instances[instance_idx].model;\n"
    "    let mvp = global.view_proj * model;\n"
    "    out.position = mvp * vec4<f32>(in.position, 1.0);\n"
    "    out.vertex_color = vec4(1.0);\n"
    "    out.material_color = color;\n"
    "    return out;\n"
    "}\n";

static const char *default_fs =
    "@fragment\n"
    "fn fs_main(@location(0) vertex_color: vec4<f32>, @location(1) material_color: vec4<f32>) "
    "-> @location(0) vec4<f32> {\n"
    "    return vertex_color * material_color;\n"
    "}\n";

typedef struct {
  vec3 position;
  vec3 velocity;
  f32 rotation_rate;
} CubeData;

global CubeData cubes[NUM_CUBES];

global f32 g_time = 0.0f;
global GpuMesh_Handle g_cube_mesh;
global Material_Handle g_cube_material;
global InstanceBuffer_Handle g_instance_buffer;
global mat4 g_instance_data[NUM_CUBES];  // CPU-side instance matrices

global Barrier frame_barrier;
global ThreadContext main_thread_ctx;

typedef struct {
  ThreadContext *ctx;
} WorkerData;

// =============================================================================
// Collision System
// =============================================================================

// Clear grid buckets (parallel)
void collision_clear_grid(void) {
  Range_u64 range = lane_range(GRID_SIZE);
  for (u64 i = range.min; i < range.max; i++) {
    g_buckets[i].count = 0;
  }
}

// Insert cubes into grid (parallel with atomics)
void collision_insert_cubes(void) {
  Range_u64 range = lane_range(NUM_CUBES);

  for (u64 i = range.min; i < range.max; i++) {
    CubeData *cube = &cubes[i];
    u32 hash = spatial_hash_3f(cube->position[0], cube->position[1],
                                cube->position[2], CELL_SIZE) % GRID_SIZE;

    // Atomic increment to get our slot
    u32 slot = ins_atomic_u32_inc_eval(&g_buckets[hash].count) - 1;

    if (slot < MAX_PER_BUCKET) {
      BucketEntry *entry = &g_buckets[hash].entries[slot];
      entry->px = cube->position[0];
      entry->py = cube->position[1];
      entry->pz = cube->position[2];
      entry->cube_idx = (u32)i;
    }
    // Overflow: cube won't collide this frame (acceptable)
  }
}

// Check collision between two cubes, update only cube A (parallel-safe)
force_inline void resolve_collision(u32 idx_a, f32 ax, f32 ay, f32 az,
                                     u32 idx_b, f32 bx, f32 by, f32 bz) {
  // Skip self
  if (idx_a == idx_b) return;

  // Distance check
  f32 dx = bx - ax;
  f32 dy = by - ay;
  f32 dz = bz - az;
  f32 dist_sq = dx * dx + dy * dy + dz * dz;

  f32 min_dist = CUBE_RADIUS * 2.0f;
  if (dist_sq >= min_dist * min_dist || dist_sq < 0.0001f) return;

  // Collision detected
  f32 dist = sqrtf(dist_sq);
  f32 inv_dist = 1.0f / dist;

  // Collision normal (from A to B)
  f32 nx = dx * inv_dist;
  f32 ny = dy * inv_dist;
  f32 nz = dz * inv_dist;

  CubeData *cube_a = &cubes[idx_a];
  CubeData *cube_b = &cubes[idx_b];

  // Relative velocity of A with respect to B
  f32 rel_vx = cube_a->velocity[0] - cube_b->velocity[0];
  f32 rel_vy = cube_a->velocity[1] - cube_b->velocity[1];
  f32 rel_vz = cube_a->velocity[2] - cube_b->velocity[2];

  // Velocity component along collision normal
  f32 vn = rel_vx * nx + rel_vy * ny + rel_vz * nz;

  // Only respond if moving toward each other
  if (vn >= 0) return;

  // Elastic collision: A gets the full impulse (B will handle its own)
  // For equal mass elastic collision: v_a' = v_a - vn * n
  cube_a->velocity[0] -= vn * nx;
  cube_a->velocity[1] -= vn * ny;
  cube_a->velocity[2] -= vn * nz;

  // Separate positions (push A away by half the overlap)
  f32 overlap = min_dist - dist;
  cube_a->position[0] -= nx * overlap * 0.5f;
  cube_a->position[1] -= ny * overlap * 0.5f;
  cube_a->position[2] -= nz * overlap * 0.5f;
}

// Detect and respond to collisions (parallel)
void collision_detect_and_respond(void) {
  Range_u64 range = lane_range(NUM_CUBES);

  for (u64 i = range.min; i < range.max; i++) {
    CubeData *cube = &cubes[i];
    f32 px = cube->position[0];
    f32 py = cube->position[1];
    f32 pz = cube->position[2];

    // Get cell coordinates
    i32 cx, cy, cz;
    spatial_cell_coords(px, py, pz, CELL_SIZE, &cx, &cy, &cz);

    // Check 27 neighboring cells (including own cell)
    for (i32 dx = -1; dx <= 1; dx++) {
      for (i32 dy = -1; dy <= 1; dy++) {
        for (i32 dz = -1; dz <= 1; dz++) {
          u32 hash = spatial_hash_3i(cx + dx, cy + dy, cz + dz) % GRID_SIZE;
          Bucket *bucket = &g_buckets[hash];

          // Walk bucket entries (cache-friendly sequential access)
          u32 count = bucket->count;
          if (count > MAX_PER_BUCKET) count = MAX_PER_BUCKET;

          for (u32 j = 0; j < count; j++) {
            BucketEntry *entry = &bucket->entries[j];
            resolve_collision((u32)i, px, py, pz,
                              entry->cube_idx, entry->px, entry->py, entry->pz);
          }
        }
      }
    }
  }
}

// Integrate velocity and handle boundary collisions (parallel)
void collision_integrate_and_boundary(f32 dt) {
  Range_u64 range = lane_range(NUM_CUBES);
  f32 bound_min = -BOUNDS + CUBE_RADIUS;
  f32 bound_max = BOUNDS - CUBE_RADIUS;

  for (u64 i = range.min; i < range.max; i++) {
    CubeData *cube = &cubes[i];

    // Integrate position
    cube->position[0] += cube->velocity[0] * dt;
    cube->position[1] += cube->velocity[1] * dt;
    cube->position[2] += cube->velocity[2] * dt;

    // Boundary collision (reflect velocity)
    for (u32 axis = 0; axis < 3; axis++) {
      if (cube->position[axis] < bound_min) {
        cube->position[axis] = bound_min;
        cube->velocity[axis] = -cube->velocity[axis];
      } else if (cube->position[axis] > bound_max) {
        cube->position[axis] = bound_max;
        cube->velocity[axis] = -cube->velocity[axis];
      }
    }
  }
}

// =============================================================================
// Frame Update - called by all threads
// =============================================================================

void app_update_and_render(f32 dt) {
  // Phase 1: Clear grid
  collision_clear_grid();
  lane_sync();

  // Phase 2: Insert cubes into grid
  collision_insert_cubes();
  lane_sync();

  // Phase 3: Detect and respond to collisions
  collision_detect_and_respond();

  // Phase 4: Integrate velocity and handle boundaries
  collision_integrate_and_boundary(dt);

  // Phase 5: Build instance matrices
  Range_u64 range = lane_range(NUM_CUBES);
  for (u64 i = range.min; i < range.max; i++) {
    CubeData *cube = &cubes[i];

    mat4 *model = &g_instance_data[i];
    mat4_identity(*model);

    mat4_translate(*model, cube->position);

    f32 angle = g_time * cube->rotation_rate;
    mat4_rotate(*model, angle, VEC3(0, 1, 0));
    mat4_rotate(*model, angle * 0.7f, VEC3(1, 0, 0));

    mat4_scale_uni(*model, CUBE_RADIUS);  // Scale to actual cube size
  }
}

global f32 g_dt = 0.016f;  // Shared dt for workers

void worker_loop(void *arg) {
  WorkerData *data = (WorkerData *)arg;
  tctx_set_current(data->ctx);

  for (;;) {
    // Barrier 1: Wait for main thread to call renderer_begin_frame()
    lane_sync();

    // All threads process their cube range
    app_update_and_render(g_dt);

    // Barrier 2: Wait for all threads to finish work
    lane_sync();
  }
}

// =============================================================================
// Initialization
// =============================================================================

void init_cubes(PCG32_State *rng) {
  // Pack cubes in a small volume at center, then explode outward
  f32 pack_size = 10.0f;  // Initial packed volume: 20m x 20m x 20m

  for (u32 i = 0; i < NUM_CUBES; i++) {
    // Random position in packed volume
    cubes[i].position[0] = pcg32_next_f32_range(rng, -pack_size, pack_size);
    cubes[i].position[1] = pcg32_next_f32_range(rng, -pack_size, pack_size);
    cubes[i].position[2] = pcg32_next_f32_range(rng, -pack_size, pack_size);

    // Velocity pointing outward from center (explosion)
    f32 dx = cubes[i].position[0];
    f32 dy = cubes[i].position[1];
    f32 dz = cubes[i].position[2];
    f32 len = sqrtf(dx * dx + dy * dy + dz * dz);

    if (len > 0.001f) {
      f32 inv_len = CUBE_SPEED / len;
      cubes[i].velocity[0] = dx * inv_len;
      cubes[i].velocity[1] = dy * inv_len;
      cubes[i].velocity[2] = dz * inv_len;
    } else {
      // Cube at center: random direction
      cubes[i].velocity[0] = pcg32_next_f32_range(rng, -1.0f, 1.0f) * CUBE_SPEED;
      cubes[i].velocity[1] = pcg32_next_f32_range(rng, -1.0f, 1.0f) * CUBE_SPEED;
      cubes[i].velocity[2] = pcg32_next_f32_range(rng, -1.0f, 1.0f) * CUBE_SPEED;
    }

    // Random rotation rate
    cubes[i].rotation_rate = 0.5f + pcg32_next_f32(rng) * 2.0f;
  }
}

WASM_EXPORT(wasm_main)
int wasm_main(void) {
  LOG_INFO("Initializing GPU...");

  // Setup arena allocator from heap
  u8 *heap = os_get_heap_base();
  ArenaAllocator arena = arena_from_buffer(heap, MB(16));

  // Initialize RNG and cube data
  PCG32_State rng = pcg32_new(12345, 1);
  init_cubes(&rng);

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
  renderer_init(&arena, NUM_WORKERS);

  // Upload cube mesh
  g_cube_mesh = renderer_upload_mesh(&(MeshDesc){
      .vertices = cube_vertices,
      .vertex_size = sizeof(cube_vertices),
      .indices = cube_indices,
      .index_size = sizeof(cube_indices),
      .index_count = CUBE_INDEX_COUNT,
      .index_format = GPU_INDEX_FORMAT_U16,
  });

  // Create instance buffer for all cube model matrices
  g_instance_buffer = renderer_create_instance_buffer(&(InstanceBufferDesc){
      .stride = sizeof(mat4),
      .max_instances = NUM_CUBES,
  });

  // Create instanced material with storage buffer for instance data
  g_cube_material = renderer_create_material(&(MaterialDesc){
      .shader_desc =
          (GpuShaderDesc){
              .vs_code = instanced_vs,
              .fs_code = default_fs,
              .uniform_blocks =
                  {
                      {.stage = GPU_STAGE_VERTEX,
                       .size = sizeof(GlobalUniforms),
                       .binding = 0},
                      {.stage = GPU_STAGE_VERTEX,
                       .size = sizeof(vec4),
                       .binding = 1},
                  },
              .uniform_block_count = 2,
              .storage_buffers =
                  {
                      {.stage = GPU_STAGE_VERTEX, .binding = 0, .readonly = true},
                  },
              .storage_buffer_count = 1,
          },
      .vertex_layout =
          {
              .stride = VERTEX_STRIDE,
              .attrs =
                  {
                      {GPU_VERTEX_FORMAT_FLOAT3, 0, 0}, // position
                      {GPU_VERTEX_FORMAT_FLOAT4, VERTEX_COLOR_OFFSET,
                       1}, // color
                  },
              .attr_count = 2,
          },
      .primitive = GPU_PRIMITIVE_TRIANGLES,
      .depth_test = true,
      .depth_write = true,
      .properties =
          {
              {.name = "color", .type = MAT_PROP_VEC4, .binding = 1},
          },
      .property_count = 1,
  });

  // Set material color to red
  material_set_vec4(g_cube_material, "color", (vec4){1.0f, 0.0f, 0.0f, 1.0f});

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
void wasm_frame(f32 dt, f32 total_time, f32 canvas_width, f32 canvas_height, f32 dpr) {
  // Share dt with workers
  g_dt = dt;
  g_time = total_time;

  // Calculate aspect ratio from canvas dimensions
  f32 aspect = canvas_width / canvas_height;

  // Setup view and projection (main thread only, before barrier)
  // Camera positioned to see the full 100x100x100 bounding box
  mat4 view, proj;
  mat4_lookat(VEC3(0, 80, 120), VEC3(0, 0, 0), VEC3(0, 1, 0), view);
  mat4_perspective(RAD(45.0f), aspect, 0.1f, 300.0f, proj);

  // Begin frame (clears, sets view/proj, resets cmd queue)
  if (is_main_thread()) {
    renderer_begin_frame(view, proj, (GpuColor){0.1f, 0.1f, 0.15f, 1.0f});
  }

  // Sync with workers - start parallel work
  lane_sync();

  // All threads: collision detection + physics + matrix building
  app_update_and_render(dt);

  // Sync with workers - wait for all to finish
  lane_sync();

  // Main thread: upload instance data and issue single instanced draw call
  if (is_main_thread()) {
    // Upload all instance matrices to GPU
    renderer_update_instance_buffer(g_instance_buffer, g_instance_data, NUM_CUBES);

    // Single draw call for all cubes
    renderer_draw_mesh_instanced(g_cube_mesh, g_cube_material, g_instance_buffer);

    renderer_end_frame();
  }
}
