#ifndef GPU_BACKEND_H
#define GPU_BACKEND_H

#include "memory.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Opaque handle types
typedef struct gpu_device gpu_device_t;
typedef struct gpu_texture gpu_texture_t;
typedef struct gpu_readback_buffer gpu_readback_buffer_t;
typedef struct gpu_command_buffer gpu_command_buffer_t;
typedef struct gpu_pipeline gpu_pipeline_t;
typedef struct gpu_buffer gpu_buffer_t;
typedef struct gpu_render_encoder gpu_render_encoder_t;
typedef struct gpu_compute_pipeline gpu_compute_pipeline_t;

// Initialize the GPU backend
gpu_device_t *gpu_init(Allocator *permanent_allocator,
                       Allocator *temporary_allocator);

// Get the native device handle for Sokol initialization
void *gpu_get_native_device(gpu_device_t *device);

// Create a texture for rendering
gpu_texture_t *gpu_create_texture(gpu_device_t *device, int width, int height);

// Create a texture with initial data
gpu_texture_t *gpu_create_texture_with_data(gpu_device_t *device, int width,
                                            int height, const void *data,
                                            size_t data_size);

// Get native texture handle for Sokol wrapping
void *gpu_get_native_texture(gpu_texture_t *texture);

// Create a readback buffer for async GPU->CPU transfer
gpu_readback_buffer_t *gpu_create_readback_buffer(gpu_device_t *device,
                                                  size_t size);

// Start async readback from texture to buffer
// Returns a command buffer that must be submitted
gpu_command_buffer_t *gpu_readback_texture_async(gpu_device_t *device,
                                                 gpu_texture_t *texture,
                                                 gpu_readback_buffer_t *buffer,
                                                 int width, int height);

// Read multiple YUV textures into a single packed buffer (Y+U+V layout)
gpu_command_buffer_t *gpu_readback_yuv_textures_async(
    gpu_device_t *device, gpu_texture_t *y_texture, gpu_texture_t *u_texture,
    gpu_texture_t *v_texture, gpu_readback_buffer_t *buffer, int width,
    int height);

// Submit command buffer and optionally wait for completion
void gpu_submit_commands(gpu_command_buffer_t *cmd_buffer, bool wait);

// Check if a readback operation is complete
bool gpu_is_readback_complete(gpu_command_buffer_t *cmd_buffer);

// Get the data from a readback buffer (only valid after readback is complete)
void *gpu_get_readback_data(gpu_readback_buffer_t *buffer);

// Copy readback buffer data to CPU memory
void gpu_copy_readback_data(gpu_readback_buffer_t *buffer, void *dst,
                            size_t size);

// === Rendering Functions ===

// Vertex attribute description
typedef struct {
  int index;  // Attribute index (0, 1, 2, etc.)
  int offset; // Offset in bytes within vertex
  // todo: format should be a enum
  int format; // Format: 0=float2, 1=float3, 2=float4, 3=ubyte4
} gpu_vertex_attr_t;

// Vertex layout description
typedef struct {
  gpu_vertex_attr_t *attributes;
  int num_attributes;
  int stride; // Total size of one vertex in bytes
} gpu_vertex_layout_t;

// Shader stage flags
typedef enum {
  GPU_STAGE_VERTEX = 1,
  GPU_STAGE_FRAGMENT = 2,
  GPU_STAGE_COMPUTE = 4
} gpu_shader_stage_t;

// Uniform buffer descriptor
typedef struct {
  uint32_t binding; // Binding slot (0, 1, 2, etc.)
  size_t size;      // Size in bytes
  int stage_flags;  // Which shader stages use this (vertex, fragment, both)
} gpu_uniform_buffer_desc_t;

// Storage buffer descriptor
typedef struct {
  uint32_t binding;
  size_t size;
  int stage_flags;
} gpu_storage_buffer_desc_t;

// Texture/sampler descriptor
typedef struct {
  uint32_t binding;
  int stage_flags;
} gpu_texture_desc_t;

// Pipeline descriptor
typedef struct {
  const char *vertex_shader_path;
  const char *fragment_shader_path;
  gpu_vertex_layout_t *vertex_layout;

  // Uniform buffer layout
  gpu_uniform_buffer_desc_t *uniform_buffers;
  uint32_t num_uniform_buffers;

  // Storage buffer layout (for blendshapes, etc.)
  gpu_storage_buffer_desc_t *storage_buffers;
  uint32_t num_storage_buffers;

  // Texture/sampler layout
  gpu_texture_desc_t *texture_bindings;
  uint32_t num_texture_bindings;

  // Render state
  bool depth_test;
  bool depth_write;
  int cull_mode; // 0=none, 1=back, 2=front
} gpu_pipeline_desc_t;

// Create a render pipeline with descriptor
gpu_pipeline_t *gpu_create_pipeline_desc(gpu_device_t *device,
                                         const gpu_pipeline_desc_t *desc);

// DEPRECATED: Old pipeline creation functions (will be removed)
gpu_pipeline_t *gpu_create_pipeline(gpu_device_t *device,
                                    const char *shader_source,
                                    const char *vertex_function,
                                    const char *fragment_function,
                                    gpu_vertex_layout_t *vertex_layout);

gpu_pipeline_t *gpu_create_pipeline_with_camera(
    gpu_device_t *device, const char *vertex_shader_path,
    const char *fragment_shader_path, gpu_vertex_layout_t *vertex_layout);

// Create a vertex buffer
gpu_buffer_t *gpu_create_buffer(gpu_device_t *device, const void *data,
                                size_t size);

// Create a new render command buffer
gpu_command_buffer_t *gpu_begin_commands(gpu_device_t *device);

// Begin a render pass to a texture
gpu_render_encoder_t *gpu_begin_render_pass(gpu_command_buffer_t *cmd_buffer,
                                            gpu_texture_t *target);

// Set the pipeline for rendering
void gpu_set_pipeline(gpu_render_encoder_t *encoder, gpu_pipeline_t *pipeline,
                      float clear_color[4]);

// Set vertex buffer
void gpu_set_vertex_buffer(gpu_render_encoder_t *encoder, gpu_buffer_t *buffer,
                           int index);

// Set uniform data (for push constants)
void gpu_set_uniforms(gpu_render_encoder_t *encoder, int index,
                      const void *data, size_t size);

// Update uniform buffer at specific binding slot
void gpu_update_uniforms(gpu_pipeline_t *pipeline,
                         uint32_t binding, // Slot index (0, 1, 2, etc.)
                         const void *data, size_t size);

// Update storage buffer at specific binding
void gpu_update_storage_buffer(gpu_pipeline_t *pipeline, uint32_t binding,
                               const void *data, size_t size);

// Texture binding functions
void gpu_bind_texture(gpu_render_encoder_t *encoder, gpu_texture_t *texture,
                      uint32_t binding);

// Update texture in pipeline's descriptor set (must be done before rendering)
void gpu_update_pipeline_texture(gpu_pipeline_t *pipeline,
                                 gpu_texture_t *texture, uint32_t binding);

// DEPRECATED: Old uniform update functions (will be removed)
void gpu_update_camera_uniforms(gpu_pipeline_t *pipeline,
                                const void *camera_data, size_t size);
void gpu_update_toon_shader_uniforms(gpu_pipeline_t *pipeline,
                                     const void *camera_data,
                                     const void *joint_transforms,
                                     const void *model_matrix,
                                     const void *material_color,
                                     const void *lights_data,
                                     const void *blendshape_params);

// Draw primitives
void gpu_draw(gpu_render_encoder_t *encoder, int vertex_count);

// End render pass
void gpu_end_render_pass(gpu_render_encoder_t *encoder);

// Commit and optionally wait for command buffer
void gpu_commit_commands(gpu_command_buffer_t *cmd_buffer, bool wait);

// Wait for a command buffer to complete (for batched commands)
void gpu_wait_for_command_buffer(gpu_command_buffer_t *cmd_buffer);

// === Compute Functions ===

// Create compute pipeline with compute shader
gpu_compute_pipeline_t *
gpu_create_compute_pipeline(gpu_device_t *device,
                            const char *compute_shader_path, int max_frames);

// Create storage texture for compute shaders (read/write)
gpu_texture_t *gpu_create_storage_texture(gpu_device_t *device, int width,
                                          int height, int format);

// Dispatch compute work
void gpu_dispatch_compute(gpu_command_buffer_t *cmd_buffer,
                          gpu_compute_pipeline_t *pipeline,
                          gpu_texture_t **textures, int num_textures,
                          int groups_x, int groups_y, int groups_z);

// Cleanup functions
void gpu_destroy_command_buffer(gpu_command_buffer_t *cmd_buffer);
void gpu_destroy_texture(gpu_texture_t *texture);
void gpu_destroy_readback_buffer(gpu_readback_buffer_t *buffer);
void gpu_destroy_pipeline(gpu_pipeline_t *pipeline);
void gpu_destroy_compute_pipeline(gpu_compute_pipeline_t *pipeline);
void gpu_destroy_buffer(gpu_buffer_t *buffer);
void gpu_reset_command_pools(gpu_device_t *device);
void gpu_reset_compute_descriptor_pool(gpu_compute_pipeline_t *pipeline);
void gpu_destroy(gpu_device_t *device);

#endif // GPU_BACKEND_H
