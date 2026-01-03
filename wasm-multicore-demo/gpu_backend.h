#ifndef H_GPU_BACKEND
#define H_GPU_BACKEND

#include "lib/typedefs.h"

// Forward declarations (defined in gpu.h)
typedef struct GpuPlatformDesc GpuPlatformDesc;
typedef struct GpuBufferDesc GpuBufferDesc;
typedef struct GpuShaderDesc GpuShaderDesc;
typedef struct GpuPipelineDesc GpuPipelineDesc;
typedef struct GpuPassDesc GpuPassDesc;
typedef struct GpuBindings GpuBindings;
typedef struct GpuShaderSlot GpuShaderSlot;

// Backend interface - implemented per platform

void gpu_backend_init(GpuPlatformDesc *desc);
void gpu_backend_shutdown(void);

// Buffers
void gpu_backend_make_buffer(u32 idx, GpuBufferDesc *desc);
void gpu_backend_update_buffer(u32 idx, void *data, u32 size);
void gpu_backend_destroy_buffer(u32 idx);

// Shaders
void gpu_backend_make_shader(u32 idx, GpuShaderDesc *desc);
void gpu_backend_destroy_shader(u32 idx);

// Pipelines
void gpu_backend_make_pipeline(u32 idx, GpuPipelineDesc *desc, GpuShaderSlot *shader);
void gpu_backend_destroy_pipeline(u32 idx);

// Render pass
void gpu_backend_begin_pass(GpuPassDesc *desc);
void gpu_backend_apply_pipeline(u32 handle_idx);
void gpu_backend_end_pass(void);
void gpu_backend_commit(void);

// Uniforms
void gpu_backend_upload_uniforms(u32 buf_idx, void *data, u32 size);

// Bindings
void gpu_backend_apply_bindings(GpuBindings *bindings, u32 ub_idx, u32 ub_count, u32 *ub_offsets);

// Draw
void gpu_backend_draw(u32 vertex_count, u32 instance_count);
void gpu_backend_draw_indexed(u32 index_count, u32 instance_count);

// Textures
void gpu_backend_load_texture(u32 idx, const char *path);
void gpu_backend_make_texture_data(u32 idx, u32 width, u32 height, u8 *data);
u32 gpu_backend_texture_is_ready(u32 idx);
void gpu_backend_destroy_texture(u32 idx);

// Render targets
void gpu_backend_make_render_target(u32 idx, u32 width, u32 height, u32 format, u32 sample_count);
void gpu_backend_resize_render_target(u32 idx, u32 width, u32 height, u32 sample_count);
void gpu_backend_destroy_render_target(u32 idx);
void gpu_backend_blit_to_screen(u32 rt_idx);

#endif
