#include "context.h"
#include "lib/thread_context.h"
#include "lib/typedefs.h"
#include "os/os.h"
#include "lib/math.h"
#include "gpu.h"
#include "app.h"
#include "camera.h"
#include "renderer.h"
#include "shaders/triangle_mvp_vs.h"
#include "shaders/triangle_transform_fs.h"

typedef struct {
    GpuBuffer vbuf;
    GpuBuffer ibuf;
    GpuShader shader;
    GpuPipeline pipeline;
    Camera camera;
} TriangleState;

global TriangleState g_triangle;

void app_init(AppMemory *memory) {
    if (!is_main_thread()) return;

    AppContext *app_ctx = app_ctx_current();

    gpu_init(&app_ctx->arena, GPU_UNIFORM_BUFFER_SIZE, NULL);

    g_triangle.camera = camera_init(VEC3(0, 0, 3), VEC3(0, 0, 0), 60.0f);

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
        .vs_code = (const char *)triangle_mvp_vs,
        .fs_code = (const char *)triangle_transform_fs,
        .uniform_blocks = FIXED_ARRAY_DEFINE(GpuUniformBlockDesc,
            GLOBAL_UNIFORMS_DESC,
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
        .depth_test = true,
        .depth_write = true,
    });

    LOG_INFO("Triangle MVP demo initialized");
}

void app_update_and_render(AppMemory *memory) {
    if (!is_main_thread()) return;

    camera_update(&g_triangle.camera, memory->canvas_width, memory->canvas_height);

    gpu_begin_pass(&(GpuPassDesc){
        .clear_color = {0.2f, 0.2f, 0.3f, 1.0f},
        .clear_depth = 1.0f,
        .render_target = GPU_INVALID_HANDLE,
    });

    gpu_apply_pipeline(g_triangle.pipeline);

    GlobalUniforms uniforms;
    mat4_identity(uniforms.model);
    mat4_rotate(uniforms.model, memory->total_time, VEC3(0, 1, 0));
    memcpy(uniforms.view, g_triangle.camera.view, sizeof(mat4));
    memcpy(uniforms.proj, g_triangle.camera.proj, sizeof(mat4));
    memcpy(uniforms.view_proj, g_triangle.camera.view_proj, sizeof(mat4));
    glm_vec3_copy(g_triangle.camera.pos, uniforms.camera_pos);
    uniforms.time = memory->total_time;

    gpu_apply_uniforms(0, &uniforms, sizeof(GlobalUniforms));

    gpu_apply_bindings(&(GpuBindings){
        .vertex_buffers = {.items = {g_triangle.vbuf}, .len = 1},
        .index_buffer = g_triangle.ibuf,
        .index_format = GPU_INDEX_FORMAT_U16,
    });

    gpu_draw_indexed(3, 1);

    gpu_end_pass();
    gpu_commit();
}
