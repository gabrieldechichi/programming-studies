#include "gpu.h"
#include "os/os.h"

// JS imports - these are implemented in renderer.ts
WASM_IMPORT(js_gpu_init)
void js_gpu_init(void);

WASM_IMPORT(js_gpu_make_buffer)
u32 js_gpu_make_buffer(u32 type, u32 size, void *data);

WASM_IMPORT(js_gpu_update_buffer)
void js_gpu_update_buffer(u32 handle_idx, void *data, u32 size);

WASM_IMPORT(js_gpu_destroy_buffer)
void js_gpu_destroy_buffer(u32 handle_idx);

WASM_IMPORT(js_gpu_make_shader)
u32 js_gpu_make_shader(const char *vs_code, u32 vs_len, const char *fs_code,
                       u32 fs_len);

WASM_IMPORT(js_gpu_destroy_shader)
void js_gpu_destroy_shader(u32 handle_idx);

WASM_IMPORT(js_gpu_make_pipeline)
u32 js_gpu_make_pipeline(u32 shader_idx, void *vertex_layout, u32 primitive,
                         u32 depth_test, u32 depth_write);

WASM_IMPORT(js_gpu_destroy_pipeline)
void js_gpu_destroy_pipeline(u32 handle_idx);

WASM_IMPORT(js_gpu_begin_pass)
void js_gpu_begin_pass(f32 r, f32 g, f32 b, f32 a, f32 depth);

WASM_IMPORT(js_gpu_apply_pipeline)
void js_gpu_apply_pipeline(u32 handle_idx);

WASM_IMPORT(js_gpu_apply_bindings)
void js_gpu_apply_bindings(void *bindings_data);

WASM_IMPORT(js_gpu_draw)
void js_gpu_draw(u32 vertex_count, u32 instance_count);

WASM_IMPORT(js_gpu_draw_indexed)
void js_gpu_draw_indexed(u32 index_count, u32 instance_count);

WASM_IMPORT(js_gpu_end_pass)
void js_gpu_end_pass(void);

WASM_IMPORT(js_gpu_commit)
void js_gpu_commit(void);

WASM_IMPORT(js_gpu_upload_uniforms)
void js_gpu_upload_uniforms(u32 buf_idx, void *data, u32 size);

WASM_IMPORT(js_gpu_apply_bindings_dynamic)
void js_gpu_apply_bindings_dynamic(void *bindings_data, u32 uniform_buf_idx, u32 uniform_offset);

// Simple handle generation (JS side manages actual resources)
local_persist u32 next_buffer_gen = 1;
local_persist u32 next_shader_gen = 1;
local_persist u32 next_pipeline_gen = 1;

void gpu_init(void) { js_gpu_init(); }

GpuBuffer gpu_make_buffer(GpuBufferDesc *desc) {
  u32 idx = js_gpu_make_buffer(desc->type, desc->size, desc->data);
  return (GpuBuffer){.idx = idx, .gen = next_buffer_gen++};
}

void gpu_update_buffer(GpuBuffer buf, void *data, u32 size) {
  js_gpu_update_buffer(buf.idx, data, size);
}

void gpu_destroy_buffer(GpuBuffer buf) { js_gpu_destroy_buffer(buf.idx); }

GpuShader gpu_make_shader(GpuShaderDesc *desc) {
  // Calculate string lengths
  u32 vs_len = 0;
  u32 fs_len = 0;
  for (const char *p = desc->vs_code; *p; p++)
    vs_len++;
  for (const char *p = desc->fs_code; *p; p++)
    fs_len++;

  u32 idx = js_gpu_make_shader(desc->vs_code, vs_len, desc->fs_code, fs_len);
  return (GpuShader){.idx = idx, .gen = next_shader_gen++};
}

void gpu_destroy_shader(GpuShader shd) { js_gpu_destroy_shader(shd.idx); }

GpuPipeline gpu_make_pipeline(GpuPipelineDesc *desc) {
  // Pack vertex layout into a flat buffer for JS
  // Format: [stride, attr_count, (format, offset, location) * attr_count]
  u32 layout_data[2 + GPU_MAX_VERTEX_ATTRS * 3];
  layout_data[0] = desc->vertex_layout.stride;
  layout_data[1] = desc->vertex_layout.attr_count;
  for (u32 i = 0; i < desc->vertex_layout.attr_count; i++) {
    layout_data[2 + i * 3 + 0] = desc->vertex_layout.attrs[i].format;
    layout_data[2 + i * 3 + 1] = desc->vertex_layout.attrs[i].offset;
    layout_data[2 + i * 3 + 2] = desc->vertex_layout.attrs[i].shader_location;
  }

  u32 idx = js_gpu_make_pipeline(desc->shader.idx, layout_data, desc->primitive,
                                 desc->depth_test, desc->depth_write);
  return (GpuPipeline){.idx = idx, .gen = next_pipeline_gen++};
}

void gpu_destroy_pipeline(GpuPipeline pip) { js_gpu_destroy_pipeline(pip.idx); }

void gpu_begin_pass(GpuPassDesc *desc) {
  js_gpu_begin_pass(desc->clear_color.r, desc->clear_color.g,
                    desc->clear_color.b, desc->clear_color.a,
                    desc->clear_depth);
}

void gpu_apply_pipeline(GpuPipeline pip) { js_gpu_apply_pipeline(pip.idx); }

void gpu_apply_bindings(GpuBindings *bindings) {
  // Pack bindings into flat buffer for JS
  // Format: [vb_count, vb0_idx, vb1_idx, ..., ib_idx, ib_format, ub_idx]
  u32 bindings_data[GPU_MAX_VERTEX_BUFFERS + 4];
  bindings_data[0] = bindings->vertex_buffer_count;
  for (u32 i = 0; i < bindings->vertex_buffer_count; i++) {
    bindings_data[1 + i] = bindings->vertex_buffers[i].idx;
  }
  u32 offset = 1 + GPU_MAX_VERTEX_BUFFERS;
  bindings_data[offset + 0] = bindings->index_buffer.idx;
  bindings_data[offset + 1] = bindings->index_format;
  bindings_data[offset + 2] = bindings->uniform_buffer.idx;

  js_gpu_apply_bindings(bindings_data);
}

void gpu_draw(u32 vertex_count, u32 instance_count) {
  js_gpu_draw(vertex_count, instance_count);
}

void gpu_draw_indexed(u32 index_count, u32 instance_count) {
  js_gpu_draw_indexed(index_count, instance_count);
}

void gpu_end_pass(void) { js_gpu_end_pass(); }

void gpu_commit(void) { js_gpu_commit(); }

// =============================================================================
// Dynamic Uniform Buffer
// =============================================================================

void gpu_uniform_init(GpuUniformBuffer *ub, ArenaAllocator *parent_arena, u32 size) {
  // Allocate staging buffer with 256-byte alignment
  u8 *staging = (u8 *)arena_alloc_align(parent_arena, size, GPU_UNIFORM_ALIGNMENT);

  // Create sub-arena from aligned buffer
  ub->arena = arena_from_buffer(staging, size);

  // Create GPU buffer
  ub->gpu_buf = gpu_make_buffer(&(GpuBufferDesc){
      .type = GPU_BUFFER_UNIFORM,
      .size = size,
      .data = 0,
  });
}

u32 gpu_uniform_alloc(GpuUniformBuffer *ub, void *data, u32 size) {
  // Allocate with 256-byte alignment
  void *dst = arena_alloc_align(&ub->arena, size, GPU_UNIFORM_ALIGNMENT);
  assert(dst != NULL); // Buffer full - increase GPU_UNIFORM_BUFFER_SIZE

  // Offset from aligned base is also aligned
  u32 offset = (u32)((u8 *)dst - ub->arena.buffer);

  // Copy data to staging buffer
  memcpy(dst, data, size);

  return offset;
}

void gpu_uniform_flush(GpuUniformBuffer *ub) {
  if (ub->arena.offset > 0) {
    // Single upload of all uniforms to GPU
    js_gpu_upload_uniforms(ub->gpu_buf.idx, ub->arena.buffer, (u32)ub->arena.offset);
  }
}

void gpu_uniform_reset(GpuUniformBuffer *ub) { arena_reset(&ub->arena); }

void gpu_apply_bindings_dynamic(GpuBindings *bindings, GpuBuffer uniform_buf,
                                u32 uniform_offset) {
  // Pack bindings into flat buffer for JS
  // Format: [vb_count, vb0_idx, vb1_idx, vb2_idx, vb3_idx, ib_idx, ib_format]
  u32 bindings_data[GPU_MAX_VERTEX_BUFFERS + 3];
  bindings_data[0] = bindings->vertex_buffer_count;
  for (u32 i = 0; i < bindings->vertex_buffer_count; i++) {
    bindings_data[1 + i] = bindings->vertex_buffers[i].idx;
  }
  u32 offset = 1 + GPU_MAX_VERTEX_BUFFERS;
  bindings_data[offset + 0] = bindings->index_buffer.idx;
  bindings_data[offset + 1] = bindings->index_format;

  js_gpu_apply_bindings_dynamic(bindings_data, uniform_buf.idx, uniform_offset);
}

// =============================================================================
// High-level Renderer State
// =============================================================================

// WGSL Shaders
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

#define MAX_RENDER_CMDS 1024
#define MAX_MESHES 64

typedef struct {
  // Mesh storage
  HandleArray_Mesh meshes;

  // GPU resources (shader/pipeline)
  GpuShader shader;
  GpuPipeline pipeline;

  // Dynamic uniform buffer
  GpuUniformBuffer uniforms;

  // Per-frame state
  mat4 view;
  mat4 proj;
  mat4 view_proj;

  // Per-thread command queues (no atomics needed)
  u8 thread_count;
  DynArray(RenderCmd) thread_cmds[32];  // MAX_THREADS
} RendererState;

global RendererState g_renderer;

// Vertex layout constants (position vec3 + color vec4)
#define VERTEX_STRIDE 28        // 7 floats * 4 bytes
#define VERTEX_COLOR_OFFSET 12  // 3 floats * 4 bytes

void renderer_init(void *arena_ptr) {
  ArenaAllocator *arena = (ArenaAllocator *)arena_ptr;

  // Initialize mesh storage
  Allocator alloc = make_arena_allocator(arena);
  g_renderer.meshes = ha_init(Mesh, &alloc, MAX_MESHES);

  // Initialize dynamic uniform buffer
  gpu_uniform_init(&g_renderer.uniforms, arena, GPU_UNIFORM_BUFFER_SIZE);

  // Create shader and pipeline (fixed vertex format: position + color)
  g_renderer.shader = gpu_make_shader(&(GpuShaderDesc){
      .vs_code = cube_vs,
      .fs_code = cube_fs,
  });

  g_renderer.pipeline = gpu_make_pipeline(&(GpuPipelineDesc){
      .shader = g_renderer.shader,
      .vertex_layout =
          {
              .stride = VERTEX_STRIDE,
              .attrs =
                  {
                      {GPU_VERTEX_FORMAT_FLOAT3, 0, 0},                    // position
                      {GPU_VERTEX_FORMAT_FLOAT4, VERTEX_COLOR_OFFSET, 1},  // color
                  },
              .attr_count = 2,
          },
      .primitive = GPU_PRIMITIVE_TRIANGLES,
      .depth_test = true,
      .depth_write = true,
  });

  // Get thread count from current context
  g_renderer.thread_count = tctx_current()->thread_count;
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

Mesh_Handle renderer_upload_mesh(MeshDesc *desc) {
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
  Mesh mesh = {
      .vbuf = vbuf,
      .ibuf = ibuf,
      .index_count = desc->index_count,
      .index_format = desc->index_format,
  };

  return ha_add(Mesh, &g_renderer.meshes, mesh);
}

void renderer_begin_frame(mat4 view, mat4 proj, GpuColor clear_color) {
  // Store view/proj for MVP computation
  memcpy(g_renderer.view, view, sizeof(mat4));
  memcpy(g_renderer.proj, proj, sizeof(mat4));
  mat4_mul(proj, view, g_renderer.view_proj);

  // Reset all thread command arrays
  for (u8 i = 0; i < g_renderer.thread_count; i++) {
    g_renderer.thread_cmds[i].len = 0;
  }

  // Reset uniform buffer for new frame
  gpu_uniform_reset(&g_renderer.uniforms);

  // Begin GPU pass
  gpu_begin_pass(&(GpuPassDesc){
      .clear_color = clear_color,
      .clear_depth = 1.0f,
  });
}

void renderer_draw_mesh(Mesh_Handle mesh, mat4 model_matrix) {
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
  // Apply pipeline once
  gpu_apply_pipeline(g_renderer.pipeline);

  // Process commands from all threads
  for (u8 t = 0; t < g_renderer.thread_count; t++) {
    DynArray(RenderCmd) *cmds = &g_renderer.thread_cmds[t];

    for (u32 i = 0; i < cmds->len; i++) {
      RenderCmd *cmd = &cmds->items[i];

      if (cmd->type == RENDER_CMD_DRAW_MESH) {
        // Look up mesh from handle
        Mesh *mesh = ha_get(Mesh, &g_renderer.meshes, cmd->draw_mesh.mesh);
        if (!mesh) continue;

        // Compute MVP = view_proj * model
        mat4 mvp;
        mat4_mul(g_renderer.view_proj, cmd->draw_mesh.model_matrix, mvp);

        // Allocate uniform slot, get offset into staging buffer
        u32 uniform_offset =
            gpu_uniform_alloc(&g_renderer.uniforms, mvp, sizeof(mat4));

        // Apply bindings with dynamic uniform offset
        gpu_apply_bindings_dynamic(
            &(GpuBindings){
                .vertex_buffers = {mesh->vbuf},
                .vertex_buffer_count = 1,
                .index_buffer = mesh->ibuf,
                .index_format = mesh->index_format,
            },
            g_renderer.uniforms.gpu_buf, uniform_offset);

        gpu_draw_indexed(mesh->index_count, 1);
      }
    }
  }

  gpu_end_pass();

  // Single upload of all uniforms to GPU
  gpu_uniform_flush(&g_renderer.uniforms);

  gpu_commit();
}
