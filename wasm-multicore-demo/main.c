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
#include "input.h"
#include "input.c"
#include "camera.h"
#include "camera.c"
#include "flycam.h"
#include "flycam.c"

typedef struct {
  f32 dt;
  f32 total_time;
  f32 canvas_width;
  f32 canvas_height;
  f32 dpr;

  AppInputEvents input_events;

  u8 *heap;
  size_t heap_size;
} AppMemory;

global InputSystem g_input;
global Camera g_camera;
global FlyCameraCtrl g_flycam;

// todo: fix hardcoded vertex format
//  Vertex layout constants (position vec3 + normal vec3 + color vec4)
#define VERTEX_STRIDE 40        // 10 floats * 4 bytes
#define VERTEX_NORMAL_OFFSET 12 // after position (3 floats)
#define VERTEX_COLOR_OFFSET 24  // after position + normal (6 floats)

#define NUM_CUBES 10000

// Collision constants
#define GRID_SIZE (8192 * 8)
#define MAX_PER_BUCKET 64
#define CELL_SIZE 2.0f
#define CUBE_RADIUS 0.5f
#define BOUNDS 125.0f
#define CUBE_SPEED 50.0f
#define RESTITUTION 0.5f // 0 = perfectly inelastic, 1 = perfectly elastic
// #define CUBE_SPEED 10.0f
// #define RESTITUTION 1.0f // 0 = perfectly inelastic, 1 = perfectly elastic

// Bucket entry for spatial hash grid - stores position for cache-friendly
// collision checks
typedef struct {
  f32 px, py, pz; // Position (unpacked for cache efficiency)
  u32 cube_idx;   // Link back to full CubeData
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
    "    @location(1) normal: vec3<f32>,\n"
    "    @location(2) vertex_color: vec4<f32>,\n"
    "};\n"
    "\n"
    "struct VertexOutput {\n"
    "    @builtin(position) position: vec4<f32>,\n"
    "    @location(0) world_normal: vec3<f32>,\n"
    "    @location(1) material_color: vec4<f32>,\n"
    "};\n"
    "\n"
    "@vertex\n"
    "fn vs_main(@builtin(instance_index) instance_idx: u32, in: VertexInput) "
    "-> VertexOutput {\n"
    "    var out: VertexOutput;\n"
    "    let model = instances[instance_idx].model;\n"
    "    let mvp = global.view_proj * model;\n"
    "    out.position = mvp * vec4<f32>(in.position, 1.0);\n"
    "    // Transform normal to world space (using upper-left 3x3 of model "
    "matrix)\n"
    "    let normal_matrix = mat3x3<f32>(model[0].xyz, model[1].xyz, "
    "model[2].xyz);\n"
    "    out.world_normal = normalize(normal_matrix * in.normal);\n"
    "    out.material_color = color;\n"
    "    return out;\n"
    "}\n";

static const char *default_fs =
    "// Directional light parameters\n"
    "const LIGHT_DIR: vec3<f32> = vec3<f32>(0.5, 0.8, 0.3);\n"
    "const AMBIENT: f32 = 0.15;\n"
    "\n"
    "@fragment\n"
    "fn fs_main(@location(0) world_normal: vec3<f32>, @location(1) "
    "material_color: vec4<f32>) "
    "-> @location(0) vec4<f32> {\n"
    "    let light_dir = normalize(LIGHT_DIR);\n"
    "    let n = normalize(world_normal);\n"
    "    let ndotl = max(dot(n, light_dir), 0.0);\n"
    "    let diffuse = AMBIENT + (1.0 - AMBIENT) * ndotl;\n"
    "    return vec4<f32>(material_color.rgb * diffuse, material_color.a);\n"
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
global mat4 g_instance_data[NUM_CUBES]; // CPU-side instance matrices

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
                               cube->position[2], CELL_SIZE) %
               GRID_SIZE;

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
  if (idx_a == idx_b)
    return;

  // Distance check
  f32 dx = bx - ax;
  f32 dy = by - ay;
  f32 dz = bz - az;
  f32 dist_sq = dx * dx + dy * dy + dz * dz;

  f32 min_dist = CUBE_RADIUS * 2.0f;
  if (dist_sq >= min_dist * min_dist || dist_sq < 0.0001f)
    return;

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
  if (vn >= 0)
    return;

  // Inelastic collision: scale impulse by restitution coefficient
  // restitution=1 is elastic, restitution=0 is perfectly inelastic
  cube_a->velocity[0] -= vn * nx * RESTITUTION;
  cube_a->velocity[1] -= vn * ny * RESTITUTION;
  cube_a->velocity[2] -= vn * nz * RESTITUTION;

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
          if (count > MAX_PER_BUCKET)
            count = MAX_PER_BUCKET;

          for (u32 j = 0; j < count; j++) {
            BucketEntry *entry = &bucket->entries[j];
            resolve_collision((u32)i, px, py, pz, entry->cube_idx, entry->px,
                              entry->py, entry->pz);
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

    mat4_scale_uni(*model, CUBE_RADIUS); // Scale to actual cube size
  }
}

global f32 g_dt = 0.016f;        // Shared dt for workers
global f32 g_accumulator = 0.0f; // Time accumulator for fixed timestep

#define FIXED_DT (1.0f / 20.0f) // Physics runs at 20Hz
#define MAX_FRAME_TIME 0.25f    // Cap to prevent spiral of death

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
  f32 pack_size = 10.0f; // Initial packed volume: 20m x 20m x 20m

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
      cubes[i].velocity[0] =
          pcg32_next_f32_range(rng, -1.0f, 1.0f) * CUBE_SPEED;
      cubes[i].velocity[1] =
          pcg32_next_f32_range(rng, -1.0f, 1.0f) * CUBE_SPEED;
      cubes[i].velocity[2] =
          pcg32_next_f32_range(rng, -1.0f, 1.0f) * CUBE_SPEED;
    }

    // Random rotation rate
    cubes[i].rotation_rate = 0.5f + pcg32_next_f32(rng) * 2.0f;
  }
}

WASM_EXPORT(wasm_main)
int wasm_main(AppMemory *memory) {
  LOG_INFO("Initializing GPU...");

  // Setup arena allocator from heap (JS sets memory->heap and memory->heap_size)
  ArenaAllocator arena = arena_from_buffer(memory->heap, memory->heap_size);

  // Initialize RNG and cube data
  PCG32_State rng = pcg32_new(12345, 1);
  init_cubes(&rng);

  // Initialize input system
  g_input = input_init();

  // Initialize camera at (0, 80, 120) looking at origin, 45° FOV
  // Pitch down ~33.7° to look at origin from that position
  g_camera = camera_init(VEC3(0, 80, 120), VEC3(-0.588f, 0, 0), 45.0f);

  // Initialize flycam controller
  glm_vec3_copy(VEC3(0, 80, 120), g_flycam.camera_pos);
  g_flycam.camera_yaw = 0.0f;
  g_flycam.camera_pitch = -0.588f;
  g_flycam.move_speed = 120.0f;
  flycam_update_camera_transform(&g_flycam, &g_camera);

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
                      {.stage = GPU_STAGE_VERTEX,
                       .binding = 0,
                       .readonly = true},
                  },
              .storage_buffer_count = 1,
          },
      .vertex_layout =
          {
              .stride = VERTEX_STRIDE,
              .attrs =
                  {
                      {GPU_VERTEX_FORMAT_FLOAT3, 0, 0}, // position
                      {GPU_VERTEX_FORMAT_FLOAT3, VERTEX_NORMAL_OFFSET,
                       1}, // normal
                      {GPU_VERTEX_FORMAT_FLOAT4, VERTEX_COLOR_OFFSET,
                       2}, // color
                  },
              .attr_count = 3,
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
void wasm_frame(AppMemory *memory) {
  f32 dt = memory->dt;
  f32 total_time = memory->total_time;
  f32 canvas_width = memory->canvas_width;
  f32 canvas_height = memory->canvas_height;

  // Update input state from events written by JS
  input_update(&g_input, &memory->input_events, total_time);

  // Update flycam (mouse look + WASD movement)
  flycam_update(&g_flycam, &g_camera, &g_input, dt);

  g_time = total_time;

  // Cap dt to prevent spiral of death (e.g., if tab was backgrounded)
  if (dt > MAX_FRAME_TIME) {
    dt = MAX_FRAME_TIME;
  }

  // Update camera matrices
  camera_update(&g_camera, canvas_width, canvas_height);

  // Begin frame (clears, sets view/proj, resets cmd queue)
  if (is_main_thread()) {
    renderer_begin_frame(g_camera.view, g_camera.proj,
                         (GpuColor){0.1f, 0.1f, 0.15f, 1.0f});
  }

  // Fixed timestep: accumulate real time, step physics in fixed increments
  g_accumulator += dt;

  // Cap max steps to prevent spiral of death (max 4 steps = catch up from
  // 15fps)
  u32 max_steps = 4;
  u32 step_count = 0;

  // Run physics steps until we've consumed accumulated time
  // Each step uses exactly FIXED_DT for deterministic simulation
  // Use do-while to guarantee at least one step (workers are waiting at
  // lane_sync)
  do {
    g_dt = FIXED_DT; // Workers see fixed dt

    // Sync with workers - start parallel physics step
    lane_sync();

    // All threads: collision detection + physics + matrix building
    app_update_and_render(FIXED_DT);

    // Sync with workers - wait for all to finish this step
    lane_sync();

    g_accumulator -= FIXED_DT;
    step_count++;
  } while (g_accumulator >= FIXED_DT && step_count < max_steps);

  // If we hit max steps, drain remaining accumulator to prevent buildup
  if (step_count >= max_steps && g_accumulator > FIXED_DT) {
    g_accumulator = 0.0f;
  }

  // Main thread: upload instance data and issue single instanced draw call
  if (is_main_thread()) {
    renderer_update_instance_buffer(g_instance_buffer, g_instance_data,
                                    NUM_CUBES);
    renderer_draw_mesh_instanced(g_cube_mesh, g_cube_material,
                                 g_instance_buffer);
    renderer_end_frame();
  }

  // Clear per-frame input flags
  input_end_frame(&g_input);
}
