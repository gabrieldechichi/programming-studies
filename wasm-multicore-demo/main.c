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
#include "renderer.h"
#include "input.h"
#include "camera.h"
#include "camera.c"

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

// Vertex layout constants (position vec3 + normal vec3 + color vec4)
#define VERTEX_STRIDE 40        // 10 floats * 4 bytes
#define VERTEX_NORMAL_OFFSET 12 // after position (3 floats)
#define VERTEX_COLOR_OFFSET 24  // after position + normal (6 floats)

#define MAX_CUBES 64 // One cube per thread, max 64 threads

typedef struct {
  ArenaAllocator arena;
  u8 num_threads;
  Camera camera;

  GpuMesh_Handle cube_mesh;
  Material_Handle cube_material;
  InstanceBuffer_Handle instance_buffer;
  mat4 instance_data[MAX_CUBES];
} GameState;

global GameState g_state;
global Barrier frame_barrier;
global ThreadContext main_thread_ctx;
global AppMemory *g_memory; // Set by main thread each frame, read by workers

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
  ThreadContext *ctx;
} WorkerData;

void app_init(AppMemory *memory) {
  if (!is_main_thread()) {
    return;
  }

  // Initialize camera - position back to see all cubes
  g_state.camera = camera_init(VEC3(0, 5, 30), VEC3(0, 0, 0), 45.0f);

  // Initialize renderer
  renderer_init(&g_state.arena, g_state.num_threads);

  // Upload cube mesh
  g_state.cube_mesh = renderer_upload_mesh(&(MeshDesc){
      .vertices = cube_vertices,
      .vertex_size = sizeof(cube_vertices),
      .indices = cube_indices,
      .index_size = sizeof(cube_indices),
      .index_count = CUBE_INDEX_COUNT,
      .index_format = GPU_INDEX_FORMAT_U16,
  });

  // Create instance buffer for cube matrices
  g_state.instance_buffer =
      renderer_create_instance_buffer(&(InstanceBufferDesc){
          .stride = sizeof(mat4),
          .max_instances = MAX_CUBES,
      });

  // Create instanced material
  g_state.cube_material = renderer_create_material(&(MaterialDesc){
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
                      {GPU_VERTEX_FORMAT_FLOAT3, 0, 0},
                      {GPU_VERTEX_FORMAT_FLOAT3, VERTEX_NORMAL_OFFSET, 1},
                      {GPU_VERTEX_FORMAT_FLOAT4, VERTEX_COLOR_OFFSET, 2},
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

  material_set_vec4(g_state.cube_material, "color",
                    (vec4){1.0f, 0.0f, 0.0f, 1.0f});

  LOG_INFO("Initialization complete. % cubes (one per thread).",
           FMT_UINT(g_state.num_threads));
}

void app_update_and_render(void) {
  ThreadContext *tctx = tctx_current();
  u32 thread_idx = tctx->thread_idx;

  // Each thread builds matrix for its own cube
  if (thread_idx < g_state.num_threads) {
    mat4 *model = &g_state.instance_data[thread_idx];
    mat4_identity(*model);

    // Position cubes in a row along X axis
    f32 spacing = 3.0f;
    f32 offset = (g_state.num_threads - 1) * spacing * 0.5f;
    vec3 pos = {thread_idx * spacing - offset, 0.0f, 0.0f};
    mat4_translate(*model, pos);

    // Rotate based on time (read from global after barrier sync)
    f32 angle = g_memory->total_time + thread_idx * 0.5f;
    mat4_rotate(*model, angle, VEC3(0, 1, 0));
    mat4_rotate(*model, angle * 0.7f, VEC3(1, 0, 0));
  }

  lane_sync();
  if (is_main_thread()) {
    camera_update(&g_state.camera, g_memory->canvas_width, g_memory->canvas_height);

    renderer_begin_frame(g_state.camera.view, g_state.camera.proj,
                         (GpuColor){0.1f, 0.1f, 0.15f, 1.0f});

    renderer_update_instance_buffer(g_state.instance_buffer,
                                    g_state.instance_data, g_state.num_threads);
    renderer_draw_mesh_instanced(g_state.cube_mesh, g_state.cube_material,
                                 g_state.instance_buffer);
    renderer_end_frame();
  }
}

void worker_loop(void *arg) {
  WorkerData *data = (WorkerData *)arg;
  tctx_set_current(data->ctx);

  lane_sync(); // Wait for init
  app_init(g_memory);
  lane_sync(); // Init done

  for (;;) {
    lane_sync(); // Wait for frame start (main sets g_memory before this)
    app_update_and_render();
    lane_sync(); // Frame done
  }
}

// =============================================================================
// Initialization
// =============================================================================

WASM_EXPORT(wasm_main)
int wasm_main(AppMemory *memory) {
  LOG_INFO("Initializing...");

  g_memory = memory;
  g_state.arena = arena_from_buffer(memory->heap, memory->heap_size);

  g_state.num_threads = os_get_processor_count();
  g_state.num_threads = g_state.num_threads; // One cube per thread

  LOG_INFO("Spawning % worker threads...", FMT_UINT(g_state.num_threads));

  // Allocate thread resources
  Thread *threads =
      ARENA_ALLOC_ARRAY(&g_state.arena, Thread, g_state.num_threads);
  ThreadContext *thread_contexts =
      ARENA_ALLOC_ARRAY(&g_state.arena, ThreadContext, g_state.num_threads);
  WorkerData *worker_data =
      ARENA_ALLOC_ARRAY(&g_state.arena, WorkerData, g_state.num_threads);

  // Create barrier for all threads
  frame_barrier = barrier_alloc(g_state.num_threads);

  // Setup main thread context (thread 0)
  main_thread_ctx = (ThreadContext){
      .thread_idx = 0,
      .thread_count = g_state.num_threads,
      .barrier = &frame_barrier,
      .temp_arena = arena_from_buffer(
          ARENA_ALLOC_ARRAY(&g_state.arena, u8, KB(64)), KB(64)),
  };
  tctx_set_current(&main_thread_ctx);

  // Spawn worker threads (indices 1..N-1)
  for (u8 i = 1; i < g_state.num_threads; i++) {
    thread_contexts[i] = (ThreadContext){
        .thread_idx = i,
        .thread_count = g_state.num_threads,
        .barrier = &frame_barrier,
        .temp_arena = arena_from_buffer(
            ARENA_ALLOC_ARRAY(&g_state.arena, u8, KB(64)), KB(64)),
    };
    worker_data[i] = (WorkerData){.ctx = &thread_contexts[i]};
    threads[i] = thread_launch(worker_loop, &worker_data[i]);
  }

  // Sync with workers for init phase
  lane_sync();
  app_init(memory);
  lane_sync();

  return 0;
}

WASM_EXPORT(wasm_frame)
void wasm_frame(AppMemory *memory) {

  lane_sync(); // Release workers - they can now read g_memory

  app_update_and_render();

  lane_sync(); // Wait for all threads to finish
}
