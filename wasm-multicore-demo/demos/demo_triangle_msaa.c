#include "context.h"
#include "lib/thread_context.h"
#include "lib/typedefs.h"
#include "os/os.h"
#include "lib/math.h"
#include "gpu.h"
#include "app.h"
#include "shaders/triangle_vs.h"
#include "shaders/triangle_fs.h"

#define MSAA_SAMPLE_COUNT 4

typedef struct {
    GpuBuffer vbuf;
    GpuBuffer ibuf;
    GpuShader shader;
    GpuPipeline pipeline;
    GpuRenderTarget msaa_target;
    u32 width;
    u32 height;
} TriangleMsaaState;

global TriangleMsaaState g_state;

void app_init(AppMemory *memory) {
    if (!is_main_thread()) return;

    AppContext *app_ctx = app_ctx_current();

    gpu_init(&app_ctx->arena, GPU_UNIFORM_BUFFER_SIZE, NULL);

    g_state.width = (u32)memory->canvas_width;
    g_state.height = (u32)memory->canvas_height;

    g_state.msaa_target = gpu_make_render_target(
        g_state.width, g_state.height,
        GPU_TEXTURE_FORMAT_RGBA8,
        MSAA_SAMPLE_COUNT
    );

    f32 vertices[] = {
        -0.5f, -0.5f, 0.5f,
         0.5f, -0.5f, 0.5f,
         0.0f,  0.5f, 0.5f,
    };

    u16 indices[] = {0, 1, 2, 0};

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

    g_state.shader = gpu_make_shader(&(GpuShaderDesc){
        .vs_code = (const char *)triangle_vs,
        .fs_code = (const char *)triangle_fs,
        .uniform_blocks = {.len = 0},
    });

    g_state.pipeline = gpu_make_pipeline(&(GpuPipelineDesc){
        .shader = g_state.shader,
        .vertex_layout = {
            .stride = sizeof(f32) * 3,
            .attrs = FIXED_ARRAY_DEFINE(GpuVertexAttr,
                {GPU_VERTEX_FORMAT_FLOAT3, 0, 0},
            ),
        },
        .primitive = GPU_PRIMITIVE_TRIANGLES,
        .depth_test = false,
        .depth_write = false,
    });

    LOG_INFO("Triangle MSAA demo initialized (% samples)", FMT_UINT(MSAA_SAMPLE_COUNT));
}

void app_update_and_render(AppMemory *memory) {
    if (!is_main_thread()) return;

    u32 new_width = (u32)memory->canvas_width;
    u32 new_height = (u32)memory->canvas_height;
    if (new_width != g_state.width || new_height != g_state.height) {
        g_state.width = new_width;
        g_state.height = new_height;
        gpu_resize_render_target(g_state.msaa_target, new_width, new_height);
    }

    gpu_begin_pass(&(GpuPassDesc){
        .clear_color = {0.2f, 0.2f, 0.3f, 1.0f},
        .clear_depth = 1.0f,
        .render_target = g_state.msaa_target,
    });

    gpu_apply_pipeline(g_state.pipeline);

    gpu_apply_bindings(&(GpuBindings){
        .vertex_buffers = {.items = {g_state.vbuf}, .len = 1},
        .index_buffer = g_state.ibuf,
        .index_format = GPU_INDEX_FORMAT_U16,
    });

    gpu_draw_indexed(3, 1);

    gpu_end_pass();

    gpu_blit_to_screen(g_state.msaa_target);

    gpu_commit();
}
