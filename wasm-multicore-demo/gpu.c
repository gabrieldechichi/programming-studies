#include "gpu.h"
#include "lib/string.h"
#include "os/os.h"

// JS imports - these are implemented in renderer.ts
// Note: make functions receive idx from C (C manages handles, JS just stores at given index)
WASM_IMPORT(js_gpu_init)
void js_gpu_init(void);

WASM_IMPORT(js_gpu_make_buffer)
void js_gpu_make_buffer(u32 idx, u32 type, u32 size, void *data);

WASM_IMPORT(js_gpu_update_buffer)
void js_gpu_update_buffer(u32 idx, void *data, u32 size);

WASM_IMPORT(js_gpu_destroy_buffer)
void js_gpu_destroy_buffer(u32 idx);

WASM_IMPORT(js_gpu_make_shader)
void js_gpu_make_shader(u32 idx, const char *vs_code, u32 vs_len,
                        const char *fs_code, u32 fs_len);

WASM_IMPORT(js_gpu_destroy_shader)
void js_gpu_destroy_shader(u32 idx);

WASM_IMPORT(js_gpu_make_pipeline)
void js_gpu_make_pipeline(u32 idx, u32 shader_idx, u32 stride, u32 attr_count,
                          u32 *attr_formats, u32 *attr_offsets,
                          u32 *attr_locations, u32 primitive, u32 depth_test,
                          u32 depth_write);

WASM_IMPORT(js_gpu_destroy_pipeline)
void js_gpu_destroy_pipeline(u32 idx);

WASM_IMPORT(js_gpu_begin_pass)
void js_gpu_begin_pass(f32 r, f32 g, f32 b, f32 a, f32 depth);

WASM_IMPORT(js_gpu_apply_pipeline)
void js_gpu_apply_pipeline(u32 handle_idx);

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
void js_gpu_apply_bindings_dynamic(u32 vb_count, u32 *vb_indices, u32 ib_idx,
                                   u32 ib_format, u32 uniform_buf_idx,
                                   u32 uniform_offset);

// Global GPU state - C manages handles, JS uses indices provided by C
local_persist GpuState gpu_state;

#define GPU_INITIAL_BUFFER_CAPACITY 64
#define GPU_INITIAL_SHADER_CAPACITY 16
#define GPU_INITIAL_PIPELINE_CAPACITY 16

void gpu_init(Allocator *allocator) {
    gpu_state.buffers = ha_init(GpuBufferSlot, allocator, GPU_INITIAL_BUFFER_CAPACITY);
    gpu_state.shaders = ha_init(GpuShaderSlot, allocator, GPU_INITIAL_SHADER_CAPACITY);
    gpu_state.pipelines = ha_init(GpuPipelineSlot, allocator, GPU_INITIAL_PIPELINE_CAPACITY);
    js_gpu_init();
}

GpuBuffer gpu_make_buffer(GpuBufferDesc *desc) {
  GpuBufferSlot slot = {0};
  GpuBuffer handle = ha_add(GpuBufferSlot, &gpu_state.buffers, slot);
  js_gpu_make_buffer(handle.idx, desc->type, desc->size, desc->data);
  return handle;
}

void gpu_update_buffer(GpuBuffer buf, void *data, u32 size) {
  if (!ha_is_valid(GpuBufferSlot, &gpu_state.buffers, buf)) return;
  js_gpu_update_buffer(buf.idx, data, size);
}

void gpu_destroy_buffer(GpuBuffer buf) {
  if (!ha_is_valid(GpuBufferSlot, &gpu_state.buffers, buf)) return;
  js_gpu_destroy_buffer(buf.idx);
  ha_remove(GpuBufferSlot, &gpu_state.buffers, buf);
}

GpuShader gpu_make_shader(GpuShaderDesc *desc) {
  GpuShaderSlot slot = {0};
  GpuShader handle = ha_add(GpuShaderSlot, &gpu_state.shaders, slot);

  u32 vs_len = str_len(desc->vs_code);
  u32 fs_len = str_len(desc->fs_code);
  js_gpu_make_shader(handle.idx, desc->vs_code, vs_len, desc->fs_code, fs_len);
  return handle;
}

void gpu_destroy_shader(GpuShader shd) {
  if (!ha_is_valid(GpuShaderSlot, &gpu_state.shaders, shd)) return;
  js_gpu_destroy_shader(shd.idx);
  ha_remove(GpuShaderSlot, &gpu_state.shaders, shd);
}

GpuPipeline gpu_make_pipeline(GpuPipelineDesc *desc) {
  GpuPipelineSlot slot = {0};
  GpuPipeline handle = ha_add(GpuPipelineSlot, &gpu_state.pipelines, slot);

  u32 attr_formats[GPU_MAX_VERTEX_ATTRS];
  u32 attr_offsets[GPU_MAX_VERTEX_ATTRS];
  u32 attr_locations[GPU_MAX_VERTEX_ATTRS];
  for (u32 i = 0; i < desc->vertex_layout.attr_count; i++) {
    attr_formats[i] = desc->vertex_layout.attrs[i].format;
    attr_offsets[i] = desc->vertex_layout.attrs[i].offset;
    attr_locations[i] = desc->vertex_layout.attrs[i].shader_location;
  }

  js_gpu_make_pipeline(handle.idx, desc->shader.idx, desc->vertex_layout.stride,
                       desc->vertex_layout.attr_count, attr_formats,
                       attr_offsets, attr_locations, desc->primitive,
                       desc->depth_test, desc->depth_write);
  return handle;
}

void gpu_destroy_pipeline(GpuPipeline pip) {
  if (!ha_is_valid(GpuPipelineSlot, &gpu_state.pipelines, pip)) return;
  js_gpu_destroy_pipeline(pip.idx);
  ha_remove(GpuPipelineSlot, &gpu_state.pipelines, pip);
}

void gpu_begin_pass(GpuPassDesc *desc) {
  js_gpu_begin_pass(desc->clear_color.r, desc->clear_color.g,
                    desc->clear_color.b, desc->clear_color.a,
                    desc->clear_depth);
}

void gpu_apply_pipeline(GpuPipeline pip) { js_gpu_apply_pipeline(pip.idx); }

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

void gpu_uniform_init(GpuUniformBuffer *ub, ArenaAllocator *parent_arena,
                      u32 size) {
  // Allocate staging buffer with 256-byte alignment
  u8 *staging =
      (u8 *)arena_alloc_align(parent_arena, size, GPU_UNIFORM_ALIGNMENT);

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
    js_gpu_upload_uniforms(ub->gpu_buf.idx, ub->arena.buffer,
                           (u32)ub->arena.offset);
  }
}

void gpu_uniform_reset(GpuUniformBuffer *ub) { arena_reset(&ub->arena); }

void gpu_apply_bindings_dynamic(GpuBindings *bindings, GpuBuffer uniform_buf,
                                u32 uniform_offset) {
  // Extract vertex buffer indices into flat array
  u32 vb_indices[GPU_MAX_VERTEX_BUFFERS];
  for (u32 i = 0; i < bindings->vertex_buffer_count; i++) {
    vb_indices[i] = bindings->vertex_buffers[i].idx;
  }

  js_gpu_apply_bindings_dynamic(bindings->vertex_buffer_count, vb_indices,
                                bindings->index_buffer.idx,
                                bindings->index_format, uniform_buf.idx,
                                uniform_offset);
}
