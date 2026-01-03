#include "gpu_backend.h"
#include "gpu.h"
#include "lib/string.h"
#include "os/os.h"

// JS Imports - implemented in renderer.ts
WASM_IMPORT(js_gpu_init) void js_gpu_init(void);
WASM_IMPORT(js_gpu_make_buffer) void js_gpu_make_buffer(u32 idx, u32 type, u32 size, void *data);
WASM_IMPORT(js_gpu_update_buffer) void js_gpu_update_buffer(u32 idx, void *data, u32 size);
WASM_IMPORT(js_gpu_destroy_buffer) void js_gpu_destroy_buffer(u32 idx);
WASM_IMPORT(js_gpu_make_shader) void js_gpu_make_shader(u32 idx, const char *vs_code, u32 vs_len, const char *fs_code, u32 fs_len);
WASM_IMPORT(js_gpu_destroy_shader) void js_gpu_destroy_shader(u32 idx);
WASM_IMPORT(js_gpu_make_pipeline) void js_gpu_make_pipeline(u32 idx, u32 shader_idx, u32 stride, u32 attr_count,
    u32 *attr_formats, u32 *attr_offsets, u32 *attr_locations,
    u32 ub_count, u32 *ub_stages, u32 *ub_sizes, u32 *ub_bindings,
    u32 sb_count, u32 *sb_stages, u32 *sb_bindings, u32 *sb_readonly,
    u32 tex_count, u32 *tex_stages, u32 *tex_sampler_bindings, u32 *tex_texture_bindings,
    u32 primitive, u32 cull_mode, u32 face_winding, u32 depth_compare, u32 depth_test, u32 depth_write,
    u32 blend_enabled, u32 blend_src, u32 blend_dst, u32 blend_op,
    u32 blend_src_alpha, u32 blend_dst_alpha, u32 blend_op_alpha);
WASM_IMPORT(js_gpu_destroy_pipeline) void js_gpu_destroy_pipeline(u32 idx);
WASM_IMPORT(js_gpu_begin_pass) void js_gpu_begin_pass(f32 r, f32 g, f32 b, f32 a, f32 depth, u32 rt_idx);
WASM_IMPORT(js_gpu_apply_pipeline) void js_gpu_apply_pipeline(u32 handle_idx);
WASM_IMPORT(js_gpu_draw) void js_gpu_draw(u32 vertex_count, u32 instance_count);
WASM_IMPORT(js_gpu_draw_indexed) void js_gpu_draw_indexed(u32 index_count, u32 instance_count);
WASM_IMPORT(js_gpu_end_pass) void js_gpu_end_pass(void);
WASM_IMPORT(js_gpu_commit) void js_gpu_commit(void);
WASM_IMPORT(js_gpu_upload_uniforms) void js_gpu_upload_uniforms(u32 buf_idx, void *data, u32 size);
WASM_IMPORT(js_gpu_apply_bindings) void js_gpu_apply_bindings(u32 vb_count, u32 *vb_indices, u32 ib_idx,
    u32 ib_format, u32 uniform_buf_idx, u32 ub_count, u32 *ub_offsets,
    u32 sb_count, u32 *sb_indices, u32 tex_count, u32 *tex_indices);
WASM_IMPORT(js_gpu_load_texture) void js_gpu_load_texture(u32 idx, const char *path, u32 path_len);
WASM_IMPORT(js_gpu_make_texture_data) void js_gpu_make_texture_data(u32 idx, u32 width, u32 height, u8 *data);
WASM_IMPORT(js_gpu_texture_is_ready) u32 js_gpu_texture_is_ready(u32 idx);
WASM_IMPORT(js_gpu_destroy_texture) void js_gpu_destroy_texture(u32 idx);
WASM_IMPORT(js_gpu_make_render_target) void js_gpu_make_render_target(u32 idx, u32 width, u32 height, u32 format, u32 sample_count);
WASM_IMPORT(js_gpu_resize_render_target) void js_gpu_resize_render_target(u32 idx, u32 width, u32 height, u32 sample_count);
WASM_IMPORT(js_gpu_destroy_render_target) void js_gpu_destroy_render_target(u32 idx);
WASM_IMPORT(js_gpu_blit_to_screen) void js_gpu_blit_to_screen(u32 rt_idx);

// Backend implementation

void gpu_backend_init(GpuPlatformDesc *desc) {
    UNUSED(desc);
    js_gpu_init();
}

void gpu_backend_shutdown(void) {}

void gpu_backend_make_buffer(u32 idx, GpuBufferDesc *desc) {
    js_gpu_make_buffer(idx, desc->type, desc->size, desc->data);
}

void gpu_backend_update_buffer(u32 idx, void *data, u32 size) {
    js_gpu_update_buffer(idx, data, size);
}

void gpu_backend_destroy_buffer(u32 idx) {
    js_gpu_destroy_buffer(idx);
}

void gpu_backend_make_shader(u32 idx, GpuShaderDesc *desc) {
    u32 vs_len = str_len(desc->vs_code);
    u32 fs_len = str_len(desc->fs_code);
    js_gpu_make_shader(idx, desc->vs_code, vs_len, desc->fs_code, fs_len);
}

void gpu_backend_destroy_shader(u32 idx) {
    js_gpu_destroy_shader(idx);
}

void gpu_backend_make_pipeline(u32 idx, GpuPipelineDesc *desc, GpuShaderSlot *shader) {
    u32 attr_formats[GPU_MAX_VERTEX_ATTRS];
    u32 attr_offsets[GPU_MAX_VERTEX_ATTRS];
    u32 attr_locations[GPU_MAX_VERTEX_ATTRS];
    for (u32 i = 0; i < desc->vertex_layout.attrs.len; i++) {
        attr_formats[i] = desc->vertex_layout.attrs.items[i].format;
        attr_offsets[i] = desc->vertex_layout.attrs.items[i].offset;
        attr_locations[i] = desc->vertex_layout.attrs.items[i].shader_location;
    }

    u32 ub_stages[GPU_MAX_UNIFORMBLOCK_SLOTS];
    u32 ub_sizes[GPU_MAX_UNIFORMBLOCK_SLOTS];
    u32 ub_bindings[GPU_MAX_UNIFORMBLOCK_SLOTS];
    for (u32 i = 0; i < shader->uniform_blocks.len; i++) {
        ub_stages[i] = shader->uniform_blocks.items[i].stage;
        ub_sizes[i] = shader->uniform_blocks.items[i].size;
        ub_bindings[i] = shader->uniform_blocks.items[i].binding;
    }

    u32 sb_stages[GPU_MAX_STORAGE_BUFFER_SLOTS];
    u32 sb_bindings[GPU_MAX_STORAGE_BUFFER_SLOTS];
    u32 sb_readonly[GPU_MAX_STORAGE_BUFFER_SLOTS];
    for (u32 i = 0; i < shader->storage_buffers.len; i++) {
        sb_stages[i] = shader->storage_buffers.items[i].stage;
        sb_bindings[i] = shader->storage_buffers.items[i].binding;
        sb_readonly[i] = shader->storage_buffers.items[i].readonly;
    }

    u32 tex_stages[GPU_MAX_TEXTURE_SLOTS];
    u32 tex_sampler_bindings[GPU_MAX_TEXTURE_SLOTS];
    u32 tex_texture_bindings[GPU_MAX_TEXTURE_SLOTS];
    for (u32 i = 0; i < shader->texture_bindings.len; i++) {
        tex_stages[i] = shader->texture_bindings.items[i].stage;
        tex_sampler_bindings[i] = shader->texture_bindings.items[i].sampler_binding;
        tex_texture_bindings[i] = shader->texture_bindings.items[i].texture_binding;
    }

    js_gpu_make_pipeline(idx, desc->shader.idx, desc->vertex_layout.stride,
        (u32)desc->vertex_layout.attrs.len, attr_formats, attr_offsets, attr_locations,
        (u32)shader->uniform_blocks.len, ub_stages, ub_sizes, ub_bindings,
        (u32)shader->storage_buffers.len, sb_stages, sb_bindings, sb_readonly,
        (u32)shader->texture_bindings.len, tex_stages, tex_sampler_bindings, tex_texture_bindings,
        desc->primitive, desc->cull_mode, desc->face_winding, desc->depth_compare,
        desc->depth_test, desc->depth_write,
        desc->blend.enabled, desc->blend.src_factor, desc->blend.dst_factor, desc->blend.op,
        desc->blend.src_factor_alpha, desc->blend.dst_factor_alpha, desc->blend.op_alpha);
}

void gpu_backend_destroy_pipeline(u32 idx) {
    js_gpu_destroy_pipeline(idx);
}

void gpu_backend_begin_pass(GpuPassDesc *desc) {
    u32 rt_idx = handle_equals(desc->render_target, INVALID_HANDLE) ? 0xFFFFFFFF : desc->render_target.idx;
    js_gpu_begin_pass(desc->clear_color.r, desc->clear_color.g,
                      desc->clear_color.b, desc->clear_color.a,
                      desc->clear_depth, rt_idx);
}

void gpu_backend_apply_pipeline(u32 handle_idx) {
    js_gpu_apply_pipeline(handle_idx);
}

void gpu_backend_end_pass(void) {
    js_gpu_end_pass();
}

void gpu_backend_commit(void) {
    js_gpu_commit();
}

void gpu_backend_upload_uniforms(u32 buf_idx, void *data, u32 size) {
    js_gpu_upload_uniforms(buf_idx, data, size);
}

void gpu_backend_apply_bindings(GpuBindings *bindings, u32 ub_idx, u32 ub_count, u32 *ub_offsets) {
    u32 vb_indices[GPU_MAX_VERTEX_BUFFERS];
    for (u32 i = 0; i < bindings->vertex_buffers.len; i++) {
        vb_indices[i] = bindings->vertex_buffers.items[i].idx;
    }

    u32 sb_indices[GPU_MAX_STORAGE_BUFFER_SLOTS];
    for (u32 i = 0; i < bindings->storage_buffers.len; i++) {
        sb_indices[i] = bindings->storage_buffers.items[i].idx;
    }

    u32 tex_indices[GPU_MAX_TEXTURE_SLOTS];
    for (u32 i = 0; i < bindings->textures.len; i++) {
        tex_indices[i] = bindings->textures.items[i].idx;
    }

    js_gpu_apply_bindings((u32)bindings->vertex_buffers.len, vb_indices,
        bindings->index_buffer.idx, bindings->index_format,
        ub_idx, ub_count, ub_offsets,
        (u32)bindings->storage_buffers.len, sb_indices,
        (u32)bindings->textures.len, tex_indices);
}

void gpu_backend_draw(u32 vertex_count, u32 instance_count) {
    js_gpu_draw(vertex_count, instance_count);
}

void gpu_backend_draw_indexed(u32 index_count, u32 instance_count) {
    js_gpu_draw_indexed(index_count, instance_count);
}

void gpu_backend_load_texture(u32 idx, const char *path) {
    js_gpu_load_texture(idx, path, str_len(path));
}

void gpu_backend_make_texture_data(u32 idx, u32 width, u32 height, u8 *data) {
    js_gpu_make_texture_data(idx, width, height, data);
}

u32 gpu_backend_texture_is_ready(u32 idx) {
    return js_gpu_texture_is_ready(idx);
}

void gpu_backend_destroy_texture(u32 idx) {
    js_gpu_destroy_texture(idx);
}

void gpu_backend_make_render_target(u32 idx, u32 width, u32 height, u32 format, u32 sample_count) {
    js_gpu_make_render_target(idx, width, height, format, sample_count);
}

void gpu_backend_resize_render_target(u32 idx, u32 width, u32 height, u32 sample_count) {
    js_gpu_resize_render_target(idx, width, height, sample_count);
}

void gpu_backend_destroy_render_target(u32 idx) {
    js_gpu_destroy_render_target(idx);
}

void gpu_backend_blit_to_screen(u32 rt_idx) {
    js_gpu_blit_to_screen(rt_idx);
}
