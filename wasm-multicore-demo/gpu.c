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
