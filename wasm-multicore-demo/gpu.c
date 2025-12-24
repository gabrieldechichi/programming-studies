#include "gpu.h"
#include "lib/handle.h"
#include "lib/string.h"
#include "os/os.h"

// =============================================================================
// Internal Types
// =============================================================================

typedef struct {
  ArenaAllocator arena;  // CPU-side staging (256-aligned base, reset each frame)
  GpuBuffer gpu_buf;     // GPU-side buffer
} GpuUniformBuffer;

typedef struct {
  HandleArray_GpuBufferSlot buffers;
  HandleArray_GpuTextureSlot textures;
  HandleArray_GpuShaderSlot shaders;
  HandleArray_GpuPipelineSlot pipelines;

  // Uniform management (internal)
  GpuUniformBuffer uniforms;
  GpuPipeline current_pipeline;
  u32 uniform_offsets[GPU_MAX_UNIFORMBLOCK_SLOTS];
} GpuStateInternal;

// =============================================================================
// JS Imports - implemented in renderer.ts
// =============================================================================

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

// Pipeline creation with flat arrays for all bind group entries
WASM_IMPORT(js_gpu_make_pipeline)
void js_gpu_make_pipeline(u32 idx, u32 shader_idx, u32 stride, u32 attr_count,
                          u32 *attr_formats, u32 *attr_offsets,
                          u32 *attr_locations,
                          u32 ub_count, u32 *ub_stages, u32 *ub_sizes, u32 *ub_bindings,
                          u32 sb_count, u32 *sb_stages, u32 *sb_bindings, u32 *sb_readonly,
                          u32 tex_count, u32 *tex_stages, u32 *tex_sampler_bindings,
                          u32 *tex_texture_bindings,
                          u32 primitive, u32 depth_test, u32 depth_write);

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

// Bindings: vertex buffers, index buffer, uniforms, storage buffers, and textures
WASM_IMPORT(js_gpu_apply_bindings)
void js_gpu_apply_bindings(u32 vb_count, u32 *vb_indices, u32 ib_idx,
                           u32 ib_format, u32 uniform_buf_idx, u32 ub_count,
                           u32 *ub_offsets, u32 sb_count, u32 *sb_indices,
                           u32 tex_count, u32 *tex_indices);

// Texture functions
WASM_IMPORT(js_gpu_load_texture)
void js_gpu_load_texture(u32 idx, const char *path, u32 path_len);

WASM_IMPORT(js_gpu_texture_is_ready)
u32 js_gpu_texture_is_ready(u32 idx);

WASM_IMPORT(js_gpu_destroy_texture)
void js_gpu_destroy_texture(u32 idx);

// =============================================================================
// Global State
// =============================================================================

local_persist GpuStateInternal gpu_state;

#define GPU_INITIAL_BUFFER_CAPACITY 64
#define GPU_INITIAL_TEXTURE_CAPACITY 32
#define GPU_INITIAL_SHADER_CAPACITY 16
#define GPU_INITIAL_PIPELINE_CAPACITY 16

// =============================================================================
// Internal Helpers
// =============================================================================

local_persist void uniform_buffer_init(GpuUniformBuffer *ub,
                                       ArenaAllocator *parent_arena, u32 size) {
  u8 *staging =
      (u8 *)arena_alloc_align(parent_arena, size, GPU_UNIFORM_ALIGNMENT);
  ub->arena = arena_from_buffer(staging, size);
  ub->gpu_buf = gpu_make_buffer(&(GpuBufferDesc){
      .type = GPU_BUFFER_UNIFORM,
      .size = size,
      .data = 0,
  });
}

local_persist u32 uniform_buffer_append(GpuUniformBuffer *ub, void *data,
                                       u32 size) {
  void *dst = arena_alloc_align(&ub->arena, size, GPU_UNIFORM_ALIGNMENT);
  assert(dst != NULL);  // Buffer full - increase uniform buffer size
  u32 offset = (u32)((u8 *)dst - ub->arena.buffer);
  memcpy(dst, data, size);
  return offset;
}

local_persist void uniform_buffer_flush(GpuUniformBuffer *ub) {
  if (ub->arena.offset > 0) {
    js_gpu_upload_uniforms(ub->gpu_buf.idx, ub->arena.buffer,
                           (u32)ub->arena.offset);
  }
}

local_persist void uniform_buffer_reset(GpuUniformBuffer *ub) {
  arena_reset(&ub->arena);
}

// =============================================================================
// Public API
// =============================================================================

void gpu_init(ArenaAllocator *arena, u32 uniform_buffer_size) {
  Allocator alloc = make_arena_allocator(arena);
  gpu_state.buffers =
      ha_init(GpuBufferSlot, &alloc, GPU_INITIAL_BUFFER_CAPACITY);
  gpu_state.textures =
      ha_init(GpuTextureSlot, &alloc, GPU_INITIAL_TEXTURE_CAPACITY);
  gpu_state.shaders =
      ha_init(GpuShaderSlot, &alloc, GPU_INITIAL_SHADER_CAPACITY);
  gpu_state.pipelines =
      ha_init(GpuPipelineSlot, &alloc, GPU_INITIAL_PIPELINE_CAPACITY);

  // Initialize internal uniform buffer
  uniform_buffer_init(&gpu_state.uniforms, arena, uniform_buffer_size);

  // Reset state
  gpu_state.current_pipeline = GPU_INVALID_HANDLE;
  for (u32 i = 0; i < GPU_MAX_UNIFORMBLOCK_SLOTS; i++) {
    gpu_state.uniform_offsets[i] = 0;
  }

  js_gpu_init();
}

GpuBuffer gpu_make_buffer(GpuBufferDesc *desc) {
  GpuBufferSlot slot = {0};
  GpuBuffer handle = ha_add(GpuBufferSlot, &gpu_state.buffers, slot);
  js_gpu_make_buffer(handle.idx, desc->type, desc->size, desc->data);
  return handle;
}

void gpu_update_buffer(GpuBuffer buf, void *data, u32 size) {
  if (!ha_is_valid(GpuBufferSlot, &gpu_state.buffers, buf))
    return;
  js_gpu_update_buffer(buf.idx, data, size);
}

void gpu_destroy_buffer(GpuBuffer buf) {
  if (!ha_is_valid(GpuBufferSlot, &gpu_state.buffers, buf))
    return;
  js_gpu_destroy_buffer(buf.idx);
  ha_remove(GpuBufferSlot, &gpu_state.buffers, buf);
}

GpuShader gpu_make_shader(GpuShaderDesc *desc) {
  GpuShaderSlot slot = {0};
  slot.uniform_block_count = desc->uniform_block_count;
  for (u32 i = 0; i < desc->uniform_block_count; i++) {
    slot.uniform_blocks[i] = desc->uniform_blocks[i];
  }
  slot.storage_buffer_count = desc->storage_buffer_count;
  for (u32 i = 0; i < desc->storage_buffer_count; i++) {
    slot.storage_buffers[i] = desc->storage_buffers[i];
  }
  slot.texture_binding_count = desc->texture_binding_count;
  for (u32 i = 0; i < desc->texture_binding_count; i++) {
    slot.texture_bindings[i] = desc->texture_bindings[i];
  }

  GpuShader handle = ha_add(GpuShaderSlot, &gpu_state.shaders, slot);

  u32 vs_len = str_len(desc->vs_code);
  u32 fs_len = str_len(desc->fs_code);
  js_gpu_make_shader(handle.idx, desc->vs_code, vs_len, desc->fs_code, fs_len);
  return handle;
}

void gpu_destroy_shader(GpuShader shd) {
  if (!ha_is_valid(GpuShaderSlot, &gpu_state.shaders, shd))
    return;
  js_gpu_destroy_shader(shd.idx);
  ha_remove(GpuShaderSlot, &gpu_state.shaders, shd);
}

GpuPipeline gpu_make_pipeline(GpuPipelineDesc *desc) {
  GpuShaderSlot *shader_slot =
      ha_get(GpuShaderSlot, &gpu_state.shaders, desc->shader);
  assert(shader_slot != NULL);

  GpuPipelineSlot slot = {
      .shader = desc->shader,
      .uniform_block_count = shader_slot->uniform_block_count,
      .storage_buffer_count = shader_slot->storage_buffer_count,
      .texture_binding_count = shader_slot->texture_binding_count,
  };
  GpuPipeline handle = ha_add(GpuPipelineSlot, &gpu_state.pipelines, slot);

  u32 attr_formats[GPU_MAX_VERTEX_ATTRS];
  u32 attr_offsets[GPU_MAX_VERTEX_ATTRS];
  u32 attr_locations[GPU_MAX_VERTEX_ATTRS];
  for (u32 i = 0; i < desc->vertex_layout.attr_count; i++) {
    attr_formats[i] = desc->vertex_layout.attrs[i].format;
    attr_offsets[i] = desc->vertex_layout.attrs[i].offset;
    attr_locations[i] = desc->vertex_layout.attrs[i].shader_location;
  }

  u32 ub_stages[GPU_MAX_UNIFORMBLOCK_SLOTS];
  u32 ub_sizes[GPU_MAX_UNIFORMBLOCK_SLOTS];
  u32 ub_bindings[GPU_MAX_UNIFORMBLOCK_SLOTS];
  for (u32 i = 0; i < shader_slot->uniform_block_count; i++) {
    ub_stages[i] = shader_slot->uniform_blocks[i].stage;
    ub_sizes[i] = shader_slot->uniform_blocks[i].size;
    ub_bindings[i] = shader_slot->uniform_blocks[i].binding;
  }

  u32 sb_stages[GPU_MAX_STORAGE_BUFFER_SLOTS];
  u32 sb_bindings[GPU_MAX_STORAGE_BUFFER_SLOTS];
  u32 sb_readonly[GPU_MAX_STORAGE_BUFFER_SLOTS];
  for (u32 i = 0; i < shader_slot->storage_buffer_count; i++) {
    sb_stages[i] = shader_slot->storage_buffers[i].stage;
    sb_bindings[i] = shader_slot->storage_buffers[i].binding;
    sb_readonly[i] = shader_slot->storage_buffers[i].readonly;
  }

  u32 tex_stages[GPU_MAX_TEXTURE_SLOTS];
  u32 tex_sampler_bindings[GPU_MAX_TEXTURE_SLOTS];
  u32 tex_texture_bindings[GPU_MAX_TEXTURE_SLOTS];
  for (u32 i = 0; i < shader_slot->texture_binding_count; i++) {
    tex_stages[i] = shader_slot->texture_bindings[i].stage;
    tex_sampler_bindings[i] = shader_slot->texture_bindings[i].sampler_binding;
    tex_texture_bindings[i] = shader_slot->texture_bindings[i].texture_binding;
  }

  js_gpu_make_pipeline(handle.idx, desc->shader.idx, desc->vertex_layout.stride,
                       desc->vertex_layout.attr_count, attr_formats,
                       attr_offsets, attr_locations,
                       shader_slot->uniform_block_count, ub_stages, ub_sizes,
                       ub_bindings,
                       shader_slot->storage_buffer_count, sb_stages,
                       sb_bindings, sb_readonly,
                       shader_slot->texture_binding_count, tex_stages,
                       tex_sampler_bindings, tex_texture_bindings,
                       desc->primitive, desc->depth_test,
                       desc->depth_write);
  return handle;
}

void gpu_destroy_pipeline(GpuPipeline pip) {
  if (!ha_is_valid(GpuPipelineSlot, &gpu_state.pipelines, pip))
    return;
  js_gpu_destroy_pipeline(pip.idx);
  ha_remove(GpuPipelineSlot, &gpu_state.pipelines, pip);
}

void gpu_begin_pass(GpuPassDesc *desc) {
  // Reset uniform buffer and offsets for new frame
  uniform_buffer_reset(&gpu_state.uniforms);
  for (u32 i = 0; i < GPU_MAX_UNIFORMBLOCK_SLOTS; i++) {
    gpu_state.uniform_offsets[i] = 0;
  }

  js_gpu_begin_pass(desc->clear_color.r, desc->clear_color.g,
                    desc->clear_color.b, desc->clear_color.a,
                    desc->clear_depth);
}

void gpu_apply_pipeline(GpuPipeline pip) {
  gpu_state.current_pipeline = pip;
  js_gpu_apply_pipeline(pip.idx);
}

void gpu_apply_uniforms(u32 slot, void *data, u32 size) {
  assert(slot < GPU_MAX_UNIFORMBLOCK_SLOTS);
  u32 offset = uniform_buffer_append(&gpu_state.uniforms, data, size);
  gpu_state.uniform_offsets[slot] = offset;
}

void gpu_apply_bindings(GpuBindings *bindings) {
  assert(!handle_equals(gpu_state.current_pipeline, INVALID_HANDLE));
  GpuPipelineSlot *pip_slot =
      ha_get(GpuPipelineSlot, &gpu_state.pipelines, gpu_state.current_pipeline);
  assert(pip_slot != NULL);

  u32 vb_indices[GPU_MAX_VERTEX_BUFFERS];
  for (u32 i = 0; i < bindings->vertex_buffer_count; i++) {
    vb_indices[i] = bindings->vertex_buffers[i].idx;
  }

  u32 sb_indices[GPU_MAX_STORAGE_BUFFER_SLOTS];
  for (u32 i = 0; i < bindings->storage_buffer_count; i++) {
    sb_indices[i] = bindings->storage_buffers[i].idx;
  }

  u32 tex_indices[GPU_MAX_TEXTURE_SLOTS];
  for (u32 i = 0; i < bindings->texture_count; i++) {
    tex_indices[i] = bindings->textures[i].idx;
  }

  js_gpu_apply_bindings(bindings->vertex_buffer_count, vb_indices,
                        bindings->index_buffer.idx, bindings->index_format,
                        gpu_state.uniforms.gpu_buf.idx,
                        pip_slot->uniform_block_count, gpu_state.uniform_offsets,
                        bindings->storage_buffer_count, sb_indices,
                        bindings->texture_count, tex_indices);
}

void gpu_draw(u32 vertex_count, u32 instance_count) {
  js_gpu_draw(vertex_count, instance_count);
}

void gpu_draw_indexed(u32 index_count, u32 instance_count) {
  js_gpu_draw_indexed(index_count, instance_count);
}

void gpu_end_pass(void) { js_gpu_end_pass(); }

void gpu_commit(void) {
  uniform_buffer_flush(&gpu_state.uniforms);
  js_gpu_commit();
}

GpuTexture gpu_make_texture(const char *path) {
  GpuTextureSlot slot = {0};
  GpuTexture handle = ha_add(GpuTextureSlot, &gpu_state.textures, slot);
  js_gpu_load_texture(handle.idx, path, str_len(path));
  return handle;
}

b32 gpu_texture_is_ready(GpuTexture tex) {
  if (!ha_is_valid(GpuTextureSlot, &gpu_state.textures, tex))
    return false;
  return js_gpu_texture_is_ready(tex.idx) != 0;
}

void gpu_destroy_texture(GpuTexture tex) {
  if (!ha_is_valid(GpuTextureSlot, &gpu_state.textures, tex))
    return;
  js_gpu_destroy_texture(tex.idx);
  ha_remove(GpuTextureSlot, &gpu_state.textures, tex);
}
