#include "context.h"
#include "lib/thread_context.h"
#include "lib/typedefs.h"
#include "os/os.h"
#include "lib/math.h"
#include "gpu.h"
#include "app.h"
#include "shaders/triangle_transform_vs.h"
#include "shaders/triangle_transform_fs.h"

typedef struct {
    mat4 model;
} TriangleUniforms;

typedef struct {
    GpuBuffer vbuf;
    GpuBuffer ibuf;
    GpuShader shader;
    GpuPipeline pipeline;
} TriangleState;

global TriangleState g_triangle;

void app_init(AppMemory *memory) {
    if (!is_main_thread()) return;

    AppContext *app_ctx = app_ctx_current();

    gpu_init(&app_ctx->arena, GPU_UNIFORM_BUFFER_SIZE, NULL);

    f32 vertices[] = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.0f,  0.5f, 0.0f,
    };

    u16 indices[] = {0, 1, 2, 0};

    g_triangle.vbuf = gpu_make_buffer(&(GpuBufferDesc){
        .type = GPU_BUFFER_VERTEX,
        .size = sizeof(vertices),
        .data = vertices,
    });

    g_triangle.ibuf = gpu_make_buffer(&(GpuBufferDesc){
        .type = GPU_BUFFER_INDEX,
        .size = sizeof(indices),
        .data = indices,
    });

    g_triangle.shader = gpu_make_shader(&(GpuShaderDesc){
        .vs_code = (const char *)triangle_transform_vs,
        .fs_code = (const char *)triangle_transform_fs,
        .uniform_blocks = FIXED_ARRAY_DEFINE(GpuUniformBlockDesc,
            {.stage = GPU_STAGE_VERTEX, .size = sizeof(TriangleUniforms), .binding = 0},
        ),
    });

    g_triangle.pipeline = gpu_make_pipeline(&(GpuPipelineDesc){
        .shader = g_triangle.shader,
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

    LOG_INFO("Triangle transform demo initialized");
}

void app_update_and_render(AppMemory *memory) {
    if (!is_main_thread()) return;

    gpu_begin_pass(&(GpuPassDesc){
        .clear_color = {0.2f, 0.2f, 0.3f, 1.0f},
        .clear_depth = 1.0f,
        .render_target = GPU_INVALID_HANDLE,
    });

    gpu_apply_pipeline(g_triangle.pipeline);

    TriangleUniforms uniforms;
    mat4_identity(uniforms.model);
    mat4_rotate(uniforms.model, memory->total_time, VEC3(0, 0, 1));

    gpu_apply_uniforms(0, &uniforms, sizeof(TriangleUniforms));

    gpu_apply_bindings(&(GpuBindings){
        .vertex_buffers = {.items = {g_triangle.vbuf}, .len = 1},
        .index_buffer = g_triangle.ibuf,
        .index_format = GPU_INDEX_FORMAT_U16,
    });

    gpu_draw_indexed(3, 1);

    gpu_end_pass();
    gpu_commit();
}
