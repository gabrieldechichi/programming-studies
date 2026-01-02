#include "gpu.h"
#include "gpu_backend.h"
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
  HandleArray_GpuRenderTargetSlot render_targets;

  // Uniform management (internal)
  GpuUniformBuffer uniforms;
  GpuPipeline current_pipeline;
  u32 uniform_offsets[GPU_MAX_UNIFORMBLOCK_SLOTS];
} GpuStateInternal;

// =============================================================================
// Global State
// =============================================================================

local_persist GpuStateInternal gpu_state;

#define GPU_INITIAL_BUFFER_CAPACITY 64
#define GPU_INITIAL_TEXTURE_CAPACITY 32
#define GPU_INITIAL_SHADER_CAPACITY 16
#define GPU_INITIAL_PIPELINE_CAPACITY 16
#define GPU_INITIAL_RENDER_TARGET_CAPACITY 8

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
    gpu_backend_upload_uniforms(ub->gpu_buf.idx, ub->arena.buffer,
                                (u32)ub->arena.offset);
  }
}

local_persist void uniform_buffer_reset(GpuUniformBuffer *ub) {
  arena_reset(&ub->arena);
}

// =============================================================================
// Public API
// =============================================================================

void gpu_init(ArenaAllocator *arena, u32 uniform_buffer_size, GpuPlatformDesc *desc) {
  Allocator alloc = make_arena_allocator(arena);
  gpu_state.buffers =
      ha_init(GpuBufferSlot, &alloc, GPU_INITIAL_BUFFER_CAPACITY);
  gpu_state.textures =
      ha_init(GpuTextureSlot, &alloc, GPU_INITIAL_TEXTURE_CAPACITY);
  gpu_state.shaders =
      ha_init(GpuShaderSlot, &alloc, GPU_INITIAL_SHADER_CAPACITY);
  gpu_state.pipelines =
      ha_init(GpuPipelineSlot, &alloc, GPU_INITIAL_PIPELINE_CAPACITY);
  gpu_state.render_targets =
      ha_init(GpuRenderTargetSlot, &alloc, GPU_INITIAL_RENDER_TARGET_CAPACITY);

  // Initialize internal uniform buffer
  uniform_buffer_init(&gpu_state.uniforms, arena, uniform_buffer_size);

  // Reset state
  gpu_state.current_pipeline = GPU_INVALID_HANDLE;
  for (u32 i = 0; i < GPU_MAX_UNIFORMBLOCK_SLOTS; i++) {
    gpu_state.uniform_offsets[i] = 0;
  }
}

GpuBuffer gpu_make_buffer(GpuBufferDesc *desc) {
  GpuBufferSlot slot = {0};
  GpuBuffer handle = ha_add(GpuBufferSlot, &gpu_state.buffers, slot);
  gpu_backend_make_buffer(handle.idx, desc);
  return handle;
}

void gpu_update_buffer(GpuBuffer buf, void *data, u32 size) {
  if (!ha_is_valid(GpuBufferSlot, &gpu_state.buffers, buf))
    return;
  gpu_backend_update_buffer(buf.idx, data, size);
}

void gpu_destroy_buffer(GpuBuffer buf) {
  if (!ha_is_valid(GpuBufferSlot, &gpu_state.buffers, buf))
    return;
  gpu_backend_destroy_buffer(buf.idx);
  ha_remove(GpuBufferSlot, &gpu_state.buffers, buf);
}

GpuShader gpu_make_shader(GpuShaderDesc *desc) {
  GpuShaderSlot slot = {0};
  slot.uniform_blocks.len = desc->uniform_blocks.len;
  for (u32 i = 0; i < desc->uniform_blocks.len; i++) {
    slot.uniform_blocks.items[i] = desc->uniform_blocks.items[i];
  }
  slot.storage_buffers.len = desc->storage_buffers.len;
  for (u32 i = 0; i < desc->storage_buffers.len; i++) {
    slot.storage_buffers.items[i] = desc->storage_buffers.items[i];
  }
  slot.texture_bindings.len = desc->texture_bindings.len;
  for (u32 i = 0; i < desc->texture_bindings.len; i++) {
    slot.texture_bindings.items[i] = desc->texture_bindings.items[i];
  }

  GpuShader handle = ha_add(GpuShaderSlot, &gpu_state.shaders, slot);
  gpu_backend_make_shader(handle.idx, desc);
  return handle;
}

void gpu_destroy_shader(GpuShader shd) {
  if (!ha_is_valid(GpuShaderSlot, &gpu_state.shaders, shd))
    return;
  gpu_backend_destroy_shader(shd.idx);
  ha_remove(GpuShaderSlot, &gpu_state.shaders, shd);
}

GpuPipeline gpu_make_pipeline(GpuPipelineDesc *desc) {
  GpuShaderSlot *shader_slot =
      ha_get(GpuShaderSlot, &gpu_state.shaders, desc->shader);
  assert(shader_slot != NULL);

  GpuPipelineSlot slot = {
      .shader = desc->shader,
      .uniform_block_count = (u32)shader_slot->uniform_blocks.len,
      .storage_buffer_count = (u32)shader_slot->storage_buffers.len,
      .texture_binding_count = (u32)shader_slot->texture_bindings.len,
  };
  GpuPipeline handle = ha_add(GpuPipelineSlot, &gpu_state.pipelines, slot);
  gpu_backend_make_pipeline(handle.idx, desc, shader_slot);
  return handle;
}

void gpu_destroy_pipeline(GpuPipeline pip) {
  if (!ha_is_valid(GpuPipelineSlot, &gpu_state.pipelines, pip))
    return;
  gpu_backend_destroy_pipeline(pip.idx);
  ha_remove(GpuPipelineSlot, &gpu_state.pipelines, pip);
}

void gpu_begin_pass(GpuPassDesc *desc) {
  // Reset uniform buffer and offsets for new frame
  uniform_buffer_reset(&gpu_state.uniforms);
  for (u32 i = 0; i < GPU_MAX_UNIFORMBLOCK_SLOTS; i++) {
    gpu_state.uniform_offsets[i] = 0;
  }
  gpu_backend_begin_pass(desc);
}

void gpu_apply_pipeline(GpuPipeline pip) {
  gpu_state.current_pipeline = pip;
  gpu_backend_apply_pipeline(pip.idx);
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
  // Flush uniform buffer before draw so D3D11 has the data
  uniform_buffer_flush(&gpu_state.uniforms);
  gpu_backend_apply_bindings(bindings, gpu_state.uniforms.gpu_buf.idx,
                             pip_slot->uniform_block_count, gpu_state.uniform_offsets);
}

void gpu_draw(u32 vertex_count, u32 instance_count) {
  gpu_backend_draw(vertex_count, instance_count);
}

void gpu_draw_indexed(u32 index_count, u32 instance_count) {
  gpu_backend_draw_indexed(index_count, instance_count);
}

void gpu_end_pass(void) { gpu_backend_end_pass(); }

void gpu_commit(void) {
  uniform_buffer_flush(&gpu_state.uniforms);
  gpu_backend_commit();
}

GpuTexture gpu_make_texture(const char *path) {
  GpuTextureSlot slot = {0};
  GpuTexture handle = ha_add(GpuTextureSlot, &gpu_state.textures, slot);
  gpu_backend_load_texture(handle.idx, path);
  return handle;
}

GpuTexture gpu_make_texture_data(u32 width, u32 height, u8 *data) {
  GpuTextureSlot slot = {0};
  GpuTexture handle = ha_add(GpuTextureSlot, &gpu_state.textures, slot);
  gpu_backend_make_texture_data(handle.idx, width, height, data);
  return handle;
}

b32 gpu_texture_is_ready(GpuTexture tex) {
  if (!ha_is_valid(GpuTextureSlot, &gpu_state.textures, tex))
    return false;
  return gpu_backend_texture_is_ready(tex.idx) != 0;
}

void gpu_destroy_texture(GpuTexture tex) {
  if (!ha_is_valid(GpuTextureSlot, &gpu_state.textures, tex))
    return;
  gpu_backend_destroy_texture(tex.idx);
  ha_remove(GpuTextureSlot, &gpu_state.textures, tex);
}

GpuRenderTarget gpu_make_render_target(u32 width, u32 height, GpuTextureFormat format) {
  GpuRenderTargetSlot slot = {
      .width = width,
      .height = height,
      .format = format,
  };
  GpuRenderTarget handle = ha_add(GpuRenderTargetSlot, &gpu_state.render_targets, slot);
  gpu_backend_make_render_target(handle.idx, width, height, format);
  return handle;
}

void gpu_resize_render_target(GpuRenderTarget rt, u32 width, u32 height) {
  GpuRenderTargetSlot *slot = ha_get(GpuRenderTargetSlot, &gpu_state.render_targets, rt);
  if (!slot) return;
  slot->width = width;
  slot->height = height;
  gpu_backend_resize_render_target(rt.idx, width, height);
}

void gpu_destroy_render_target(GpuRenderTarget rt) {
  if (!ha_is_valid(GpuRenderTargetSlot, &gpu_state.render_targets, rt))
    return;
  gpu_backend_destroy_render_target(rt.idx);
  ha_remove(GpuRenderTargetSlot, &gpu_state.render_targets, rt);
}

void gpu_blit_to_screen(GpuRenderTarget rt) {
  if (!ha_is_valid(GpuRenderTargetSlot, &gpu_state.render_targets, rt))
    return;
  gpu_backend_blit_to_screen(rt.idx);
}
