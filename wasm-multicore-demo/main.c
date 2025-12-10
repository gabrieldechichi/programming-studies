// Include lib .c files directly
#include "lib/string.c"
#include "lib/common.c"
#include "lib/memory.c"
#include "lib/string_builder.c"
#include "os/os_wasm.c"
#include "lib/thread.c"
#include "lib/thread_context.c"
#include "lib/multicore_runtime.c"
#include "gpu.c"
#include "lib/math.h"

// WGSL Shaders as C strings
static const char *cube_vs =
    "struct Uniforms {\n"
    "    mvp: mat4x4<f32>,\n"
    "};\n"
    "@group(0) @binding(0) var<uniform> uniforms: Uniforms;\n"
    "\n"
    "struct VertexInput {\n"
    "    @location(0) position: vec3<f32>,\n"
    "    @location(1) color: vec4<f32>,\n"
    "};\n"
    "\n"
    "struct VertexOutput {\n"
    "    @builtin(position) position: vec4<f32>,\n"
    "    @location(0) color: vec4<f32>,\n"
    "};\n"
    "\n"
    "@vertex\n"
    "fn vs_main(in: VertexInput) -> VertexOutput {\n"
    "    var out: VertexOutput;\n"
    "    out.position = uniforms.mvp * vec4<f32>(in.position, 1.0);\n"
    "    out.color = in.color;\n"
    "    return out;\n"
    "}\n";

static const char *cube_fs =
    "@fragment\n"
    "fn fs_main(@location(0) color: vec4<f32>) -> @location(0) vec4<f32> {\n"
    "    return color;\n"
    "}\n";

// Vertex structure: position (vec3) + color (vec4)
typedef struct {
  f32 x, y, z;    // position
  f32 r, g, b, a; // color
} Vertex;

// Cube vertices with colors per face
static Vertex cube_vertices[] = {
    // Front face (red)
    {-1, -1, 1, 1, 0, 0, 1},
    {1, -1, 1, 1, 0, 0, 1},
    {1, 1, 1, 1, 0, 0, 1},
    {-1, 1, 1, 1, 0, 0, 1},
    // Back face (green)
    {-1, -1, -1, 0, 1, 0, 1},
    {-1, 1, -1, 0, 1, 0, 1},
    {1, 1, -1, 0, 1, 0, 1},
    {1, -1, -1, 0, 1, 0, 1},
    // Top face (blue)
    {-1, 1, -1, 0, 0, 1, 1},
    {-1, 1, 1, 0, 0, 1, 1},
    {1, 1, 1, 0, 0, 1, 1},
    {1, 1, -1, 0, 0, 1, 1},
    // Bottom face (yellow)
    {-1, -1, -1, 1, 1, 0, 1},
    {1, -1, -1, 1, 1, 0, 1},
    {1, -1, 1, 1, 1, 0, 1},
    {-1, -1, 1, 1, 1, 0, 1},
    // Right face (magenta)
    {1, -1, -1, 1, 0, 1, 1},
    {1, 1, -1, 1, 0, 1, 1},
    {1, 1, 1, 1, 0, 1, 1},
    {1, -1, 1, 1, 0, 1, 1},
    // Left face (cyan)
    {-1, -1, -1, 0, 1, 1, 1},
    {-1, -1, 1, 0, 1, 1, 1},
    {-1, 1, 1, 0, 1, 1, 1},
    {-1, 1, -1, 0, 1, 1, 1},
};

static u16 cube_indices[] = {
    0,  1,  2,  0,  2,  3,  // front
    4,  5,  6,  4,  6,  7,  // back
    8,  9,  10, 8,  10, 11, // top
    12, 13, 14, 12, 14, 15, // bottom
    16, 17, 18, 16, 18, 19, // right
    20, 21, 22, 20, 22, 23, // left
};

// GPU resources
static GpuBuffer vbuf;
static GpuBuffer ibuf;
static GpuBuffer ubuf;
static GpuShader shader;
static GpuPipeline pipeline;

// Animation state
static f32 rotation = 0.0f;

// Threading state
static Barrier frame_barrier;

// Worker thread data
typedef struct {
  ThreadContext *ctx;
} WorkerData;

void worker_loop(void *arg) {
  WorkerData *data = (WorkerData *)arg;
  tctx_set_current(data->ctx);

  u32 iterations = 0;

  for (;;) {
    // Wait for frame start signal
    barrier_wait(frame_barrier);

    // Do parallel work here
    iterations++;
    if (iterations % 100 == 0) {
      u8 idx = tctx_current()->thread_idx;
      LOG_INFO("Worker % iteration %", FMT_UINT(idx), FMT_UINT(iterations));
    }

    // Signal work complete
    barrier_wait(frame_barrier);
  }
}

WASM_EXPORT(wasm_main)
int wasm_main(void) {
  LOG_INFO("Initializing GPU...");
  gpu_init();

  // Create vertex buffer
  vbuf = gpu_make_buffer(&(GpuBufferDesc){
      .type = GPU_BUFFER_VERTEX,
      .size = sizeof(cube_vertices),
      .data = cube_vertices,
  });

  // Create index buffer
  ibuf = gpu_make_buffer(&(GpuBufferDesc){
      .type = GPU_BUFFER_INDEX,
      .size = sizeof(cube_indices),
      .data = cube_indices,
  });

  // Create uniform buffer (MVP matrix = 64 bytes)
  ubuf = gpu_make_buffer(&(GpuBufferDesc){
      .type = GPU_BUFFER_UNIFORM,
      .size = 64,
      .data = 0,
  });

  // Create shader
  shader = gpu_make_shader(&(GpuShaderDesc){
      .vs_code = cube_vs,
      .fs_code = cube_fs,
  });

  // Create pipeline
  pipeline = gpu_make_pipeline(&(GpuPipelineDesc){
      .shader = shader,
      .vertex_layout =
          {
              .stride = sizeof(Vertex),
              .attrs =
                  {
                      {GPU_VERTEX_FORMAT_FLOAT3, 0, 0}, // position
                      {GPU_VERTEX_FORMAT_FLOAT4, offsetof(Vertex, r),
                       1}, // color
                  },
              .attr_count = 2,
          },
      .primitive = GPU_PRIMITIVE_TRIANGLES,
      .depth_test = true,
      .depth_write = true,
  });

  u8 NUM_THREADS = os_get_processor_count();

  // Setup threading
  LOG_INFO("Spawning % worker threads...", FMT_UINT(NUM_THREADS));

  // Setup arena allocator from heap
  u8 *heap = os_get_heap_base();
  ArenaAllocator arena = arena_from_buffer(heap, MB(16));

  // Allocate thread resources
  Thread *threads = ARENA_ALLOC_ARRAY(&arena, Thread, NUM_THREADS);
  ThreadContext *thread_contexts =
      ARENA_ALLOC_ARRAY(&arena, ThreadContext, NUM_THREADS);
  WorkerData *worker_data = ARENA_ALLOC_ARRAY(&arena, WorkerData, NUM_THREADS);

  // Create barrier for NUM_THREADS workers + 1 main thread
  frame_barrier = barrier_alloc(NUM_THREADS + 1);

  // Spawn worker threads
  for (u8 i = 0; i < NUM_THREADS; i++) {
    thread_contexts[i] = (ThreadContext){
        .thread_idx = i,
        .thread_count = NUM_THREADS,
        .barrier = &frame_barrier,
        .temp_arena =
            arena_from_buffer(ARENA_ALLOC_ARRAY(&arena, u8, KB(64)), KB(64)),
    };
    worker_data[i] = (WorkerData){.ctx = &thread_contexts[i]};
    threads[i] = thread_launch(worker_loop, &worker_data[i]);
  }

  LOG_INFO("GPU resources created, starting render loop");
  return 0;
}

WASM_EXPORT(wasm_frame)
void wasm_frame(void) {
  // Signal workers to start frame work
  barrier_wait(frame_barrier);

  // Workers are doing parallel work...

  // Wait for workers to finish
  barrier_wait(frame_barrier);

  // Now safe to render
  rotation += 0.01f;

  // Build MVP matrix
  mat4 model, view, proj, mvp;

  // Model: rotate around Y and X axes
  mat4_identity(model);
  glm_rotate(model, rotation, (vec3){0, 1, 0});
  glm_rotate(model, rotation * 0.7f, (vec3){1, 0, 0});

  // View: camera at (0, 0, 5) looking at origin
  glm_lookat((vec3){0, 0, 5}, (vec3){0, 0, 0}, (vec3){0, 1, 0}, view);

  // Projection: perspective
  glm_perspective(RAD(45.0f), 16.0f / 9.0f, 0.1f, 100.0f, proj);

  // MVP = proj * view * model
  mat4 tmp;
  mat4_mul(proj, view, tmp);
  mat4_mul(tmp, model, mvp);

  // Update uniform buffer
  gpu_update_buffer(ubuf, mvp, sizeof(mvp));

  // Render
  gpu_begin_pass(&(GpuPassDesc){
      .clear_color = {0.1f, 0.1f, 0.15f, 1.0f},
      .clear_depth = 1.0f,
  });

  gpu_apply_pipeline(pipeline);

  gpu_apply_bindings(&(GpuBindings){
      .vertex_buffers = {vbuf},
      .vertex_buffer_count = 1,
      .index_buffer = ibuf,
      .index_format = GPU_INDEX_FORMAT_U16,
      .uniform_buffer = ubuf,
  });

  gpu_draw_indexed(36, 1);

  gpu_end_pass();
  gpu_commit();
}
