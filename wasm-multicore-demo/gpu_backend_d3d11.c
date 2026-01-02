#include "gpu_backend.h"
#include "gpu.h"
#include "os/os.h"

// D3D11 backend - stub implementation

void gpu_backend_init(GpuPlatformDesc *desc) {
    UNUSED(desc);
    LOG_INFO("D3D11 backend: init (stub)");
}

void gpu_backend_shutdown(void) {}

void gpu_backend_make_buffer(u32 idx, GpuBufferDesc *desc) {
    UNUSED(idx); UNUSED(desc);
}

void gpu_backend_update_buffer(u32 idx, void *data, u32 size) {
    UNUSED(idx); UNUSED(data); UNUSED(size);
}

void gpu_backend_destroy_buffer(u32 idx) {
    UNUSED(idx);
}

void gpu_backend_make_shader(u32 idx, GpuShaderDesc *desc) {
    UNUSED(idx); UNUSED(desc);
}

void gpu_backend_destroy_shader(u32 idx) {
    UNUSED(idx);
}

void gpu_backend_make_pipeline(u32 idx, GpuPipelineDesc *desc, GpuShaderSlot *shader) {
    UNUSED(idx); UNUSED(desc); UNUSED(shader);
}

void gpu_backend_destroy_pipeline(u32 idx) {
    UNUSED(idx);
}

void gpu_backend_begin_pass(GpuPassDesc *desc) {
    UNUSED(desc);
}

void gpu_backend_apply_pipeline(u32 handle_idx) {
    UNUSED(handle_idx);
}

void gpu_backend_end_pass(void) {}

void gpu_backend_commit(void) {}

void gpu_backend_upload_uniforms(u32 buf_idx, void *data, u32 size) {
    UNUSED(buf_idx); UNUSED(data); UNUSED(size);
}

void gpu_backend_apply_bindings(GpuBindings *bindings, u32 ub_idx, u32 ub_count, u32 *ub_offsets) {
    UNUSED(bindings); UNUSED(ub_idx); UNUSED(ub_count); UNUSED(ub_offsets);
}

void gpu_backend_draw(u32 vertex_count, u32 instance_count) {
    UNUSED(vertex_count); UNUSED(instance_count);
}

void gpu_backend_draw_indexed(u32 index_count, u32 instance_count) {
    UNUSED(index_count); UNUSED(instance_count);
}

void gpu_backend_load_texture(u32 idx, const char *path) {
    UNUSED(idx); UNUSED(path);
}

void gpu_backend_make_texture_data(u32 idx, u32 width, u32 height, u8 *data) {
    UNUSED(idx); UNUSED(width); UNUSED(height); UNUSED(data);
}

u32 gpu_backend_texture_is_ready(u32 idx) {
    UNUSED(idx);
    return 1;
}

void gpu_backend_destroy_texture(u32 idx) {
    UNUSED(idx);
}

void gpu_backend_make_render_target(u32 idx, u32 width, u32 height, u32 format) {
    UNUSED(idx); UNUSED(width); UNUSED(height); UNUSED(format);
}

void gpu_backend_resize_render_target(u32 idx, u32 width, u32 height) {
    UNUSED(idx); UNUSED(width); UNUSED(height);
}

void gpu_backend_destroy_render_target(u32 idx) {
    UNUSED(idx);
}

void gpu_backend_blit_to_screen(u32 rt_idx) {
    UNUSED(rt_idx);
}
