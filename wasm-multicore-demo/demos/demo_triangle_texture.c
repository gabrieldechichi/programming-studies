#include "context.h"
#include "lib/thread_context.h"
#include "lib/typedefs.h"
#include "os/os.h"
#include "lib/math.h"
#include "gpu.h"
#include "app.h"
#include "shaders/triangle_texture_vs.h"
#include "shaders/triangle_texture_fs.h"

typedef struct {
    GpuBuffer vbuf;
    GpuBuffer ibuf;
    GpuShader shader;
    GpuPipeline pipeline;
    GpuTexture texture;
} TriangleTextureState;

global TriangleTextureState g_state;

void app_init(AppMemory *memory) {
    if (!is_main_thread()) return;

    AppContext *app_ctx = app_ctx_current();

    gpu_init(&app_ctx->arena, GPU_UNIFORM_BUFFER_SIZE, NULL);

    f32 vertices[] = {
        // position (x, y, z), uv (u, v)
        -0.5f, -0.5f, 0.5f,   0.0f, 1.0f,
         0.5f, -0.5f, 0.5f,   1.0f, 1.0f,
         0.0f,  0.5f, 0.5f,   0.5f, 0.0f,
    };

    u16 indices[] = {0, 1, 2};

    g_state.vbuf = gpu_make_buffer(&(GpuBufferDesc){
        .type = GPU_BUFFER_VERTEX,
        .size = sizeof(vertices),
        .data = vertices,
    });

    g_state.ibuf = gpu_make_buffer(&(GpuBufferDesc){
        .type = GPU_BUFFER_INDEX,
        .size = sizeof(indices),
        .data = indices,
    });

    g_state.texture = gpu_make_texture("public/cube_albedo.png");

    g_state.shader = gpu_make_shader(&(GpuShaderDesc){
        .vs_code = (const char *)triangle_texture_vs,
        .fs_code = (const char *)triangle_texture_fs,
        .uniform_blocks = {.len = 0},
        .texture_bindings = {
            .items = {{
                .stage = GPU_STAGE_FRAGMENT,
                .sampler_binding = 0,
                .texture_binding = 0,
            }},
            .len = 1,
        },
    });

    g_state.pipeline = gpu_make_pipeline(&(GpuPipelineDesc){
        .shader = g_state.shader,
        .vertex_layout = {
            .stride = sizeof(f32) * 5,
            .attrs = FIXED_ARRAY_DEFINE(GpuVertexAttr,
                {GPU_VERTEX_FORMAT_FLOAT3, 0, 0},
                {GPU_VERTEX_FORMAT_FLOAT2, sizeof(f32) * 3, 1},
            ),
        },
        .primitive = GPU_PRIMITIVE_TRIANGLES,
        .depth_test = false,
        .depth_write = false,
    });

    LOG_INFO("Triangle texture demo initialized");
}

void app_update_and_render(AppMemory *memory) {
    if (!is_main_thread()) return;

    gpu_begin_pass(&(GpuPassDesc){
        .clear_color = {0.2f, 0.2f, 0.3f, 1.0f},
        .clear_depth = 1.0f,
        .render_target = GPU_INVALID_HANDLE,
    });

    gpu_apply_pipeline(g_state.pipeline);

    gpu_apply_bindings(&(GpuBindings){
        .vertex_buffers = {.items = {g_state.vbuf}, .len = 1},
        .index_buffer = g_state.ibuf,
        .index_format = GPU_INDEX_FORMAT_U16,
        .textures = {.items = {g_state.texture}, .len = 1},
    });

    gpu_draw_indexed(3, 1);

    gpu_end_pass();
    gpu_commit();
}
