#include "renderer.h"
#include "lib/thread_context.h"

// =============================================================================
// Shaders (WGSL)
// =============================================================================

static const char *default_vs =
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

static const char *default_fs =
    "@fragment\n"
    "fn fs_main(@location(0) color: vec4<f32>) -> @location(0) vec4<f32> {\n"
    "    return color;\n"
    "}\n";

#define MAX_RENDER_CMDS 1024
#define MAX_MESHES 64

//todo: fix hardcoded vertex format
// Vertex layout constants (position vec3 + color vec4)
#define VERTEX_STRIDE 28       // 7 floats * 4 bytes
#define VERTEX_COLOR_OFFSET 12 // 3 floats * 4 bytes

typedef struct {
  HandleArray_GpuMesh meshes;

  //todo: fix hardcoded single shader
  GpuShader shader;
  //todo: fix hardcoded single pipeline
  GpuPipeline pipeline;

  // Per-frame state
  // todo: move to camera uniforms, send to shader
  mat4 view;
  mat4 proj;
  mat4 view_proj;

  // Per-thread command queues (no atomics needed)
  u8 thread_count;
  //todo: maybe pass thread_count on renderer_init?
  DynArray(RenderCmd) thread_cmds[32]; // MAX_THREADS
} RendererState;

global RendererState g_renderer;

void renderer_init(ArenaAllocator *arena, u8 thread_count) {
  Allocator alloc = make_arena_allocator(arena);

  gpu_init(arena, GPU_UNIFORM_BUFFER_SIZE);
  g_renderer.meshes = ha_init(GpuMesh, &alloc, MAX_MESHES);

  //todo: shader and pipeline dynamic creation
  // Create shader with uniform block description
  g_renderer.shader = gpu_make_shader(&(GpuShaderDesc){
      .vs_code = default_vs,
      .fs_code = default_fs,
      .uniform_blocks =
          {
              {.stage = GPU_STAGE_VERTEX, .size = sizeof(mat4), .binding = 0},
          },
      .uniform_block_count = 1,
  });

  g_renderer.pipeline = gpu_make_pipeline(&(GpuPipelineDesc){
      .shader = g_renderer.shader,
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
  });

  // Get thread count from current context
  g_renderer.thread_count = thread_count;
  assert_msg(g_renderer.thread_count > 0, "Thread count can't be zero");

  // Allocate command arrays for each thread
  u32 cmds_per_thread = MAX_RENDER_CMDS / g_renderer.thread_count;
  for (u8 i = 0; i < g_renderer.thread_count; i++) {
    g_renderer.thread_cmds[i] = (RenderCmd_DynArray){
        .len = 0,
        .cap = cmds_per_thread,
        .items = ARENA_ALLOC_ARRAY(arena, RenderCmd, cmds_per_thread),
    };
  }
}

GpuMesh_Handle renderer_upload_mesh(MeshDesc *desc) {
  // Create GPU buffers
  GpuBuffer vbuf = gpu_make_buffer(&(GpuBufferDesc){
      .type = GPU_BUFFER_VERTEX,
      .size = desc->vertex_size,
      .data = desc->vertices,
  });

  GpuBuffer ibuf = gpu_make_buffer(&(GpuBufferDesc){
      .type = GPU_BUFFER_INDEX,
      .size = desc->index_size,
      .data = desc->indices,
  });

  // Create mesh and add to storage
  GpuMesh mesh = {
      .vbuf = vbuf,
      .ibuf = ibuf,
      .index_count = desc->index_count,
      .index_format = desc->index_format,
  };

  return ha_add(Mesh, &g_renderer.meshes, mesh);
}

// todo: separate call for camera uniforms
void renderer_begin_frame(mat4 view, mat4 proj, GpuColor clear_color) {
  debug_assert_msg(
      is_main_thread(),
      "renderer_begin_frame can only be called from the main thread");

  // Store view/proj for MVP computation
  memcpy(g_renderer.view, view, sizeof(mat4));
  memcpy(g_renderer.proj, proj, sizeof(mat4));
  mat4_mul(proj, view, g_renderer.view_proj);

  // Reset all thread command arrays
  for (u8 i = 0; i < g_renderer.thread_count; i++) {
    g_renderer.thread_cmds[i].len = 0;
  }

  // Begin GPU pass (also resets internal uniform buffer)
  gpu_begin_pass(&(GpuPassDesc){
      .clear_color = clear_color,
      .clear_depth = 1.0f,
  });
}

void renderer_draw_mesh(GpuMesh_Handle mesh, mat4 model_matrix) {
  RenderCmd cmd = {
      .type = RENDER_CMD_DRAW_MESH,
      .draw_mesh.mesh = mesh,
  };
  memcpy(cmd.draw_mesh.model_matrix, model_matrix, sizeof(mat4));

  // Append to current thread's command array (no atomic!)
  u8 tid = tctx_current()->thread_idx;
  arr_append(g_renderer.thread_cmds[tid], cmd);
}

void renderer_end_frame(void) {
  debug_assert_msg(
      is_main_thread(),
      "renderer_end_frame can only be called from the main thread");

  gpu_apply_pipeline(g_renderer.pipeline);

  // Process commands from all threads
  for (u8 t = 0; t < g_renderer.thread_count; t++) {
    DynArray(RenderCmd) *cmds = &g_renderer.thread_cmds[t];

    for (u32 i = 0; i < cmds->len; i++) {
      RenderCmd *cmd = &cmds->items[i];

      if (cmd->type == RENDER_CMD_DRAW_MESH) {
        GpuMesh *mesh =
            ha_get(GpuMesh, &g_renderer.meshes, cmd->draw_mesh.mesh);
        if (!mesh)
          continue;

        // Compute MVP = view_proj * model
        mat4 mvp;
        mat4_mul(g_renderer.view_proj, cmd->draw_mesh.model_matrix, mvp);

        // Set uniform data for slot 0 (MVP matrix)
        gpu_apply_uniforms(0, mvp, sizeof(mat4));

        // Apply vertex/index buffer bindings
        gpu_apply_bindings(&(GpuBindings){
            .vertex_buffers = {mesh->vbuf},
            .vertex_buffer_count = 1,
            .index_buffer = mesh->ibuf,
            .index_format = mesh->index_format,
        });

        gpu_draw_indexed(mesh->index_count, 1);
      }
    }
  }

  gpu_end_pass();
  gpu_commit();  // Flushes uniforms internally before submit
}
