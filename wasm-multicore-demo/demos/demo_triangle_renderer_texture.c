#include "context.h"
#include "lib/thread_context.h"
#include "lib/typedefs.h"
#include "os/os.h"
#include "renderer.h"
#include "camera.h"
#include "app.h"
#include "shaders/triangle_texture_vs.h"
#include "shaders/triangle_texture_fs.h"

typedef struct {
    Camera camera;
    GpuMesh_Handle mesh;
    Material_Handle material;
    GpuTexture texture;
} DemoState;

global DemoState g_demo;

void app_init(AppMemory *memory) {
    if (!is_main_thread()) return;

    AppContext *app_ctx = app_ctx_current();
    renderer_init(&app_ctx->arena, app_ctx->num_threads,
                  (u32)memory->canvas_width, (u32)memory->canvas_height, 4);

    g_demo.camera = camera_init(VEC3(0, 0, 3), VEC3(0, 0, 0), 60.0f);

    f32 vertices[] = {
        // position (x, y, z), uv (u, v)
        -0.5f, -0.5f, 0.0f,   0.0f, 1.0f,
         0.5f, -0.5f, 0.0f,   1.0f, 1.0f,
         0.0f,  0.5f, 0.0f,   0.5f, 0.0f,
    };

    u16 indices[] = {0, 1, 2};

    g_demo.mesh = renderer_upload_mesh(&(MeshDesc){
        .vertices = vertices,
        .vertex_size = sizeof(vertices),
        .indices = indices,
        .index_size = sizeof(indices),
        .index_count = 3,
        .index_format = GPU_INDEX_FORMAT_U16,
    });

    g_demo.texture = gpu_make_texture("public/cube_albedo.png");

    g_demo.material = renderer_create_material(&(MaterialDesc){
        .shader_desc = (GpuShaderDesc){
            .vs_code = (const char *)triangle_texture_vs,
            .fs_code = (const char *)triangle_texture_fs,
            .uniform_blocks = FIXED_ARRAY_DEFINE(GpuUniformBlockDesc,
                GLOBAL_UNIFORMS_DESC,
            ),
            .texture_bindings = FIXED_ARRAY_DEFINE(GpuTextureBindingDesc,
                GPU_TEXTURE_BINDING_FRAG(1, 0),
            ),
        },
        .vertex_layout = {
            .stride = sizeof(f32) * 5,
            .attrs = FIXED_ARRAY_DEFINE(GpuVertexAttr,
                {GPU_VERTEX_FORMAT_FLOAT3, 0, 0},
                {GPU_VERTEX_FORMAT_FLOAT2, sizeof(f32) * 3, 1},
            ),
        },
        .primitive = GPU_PRIMITIVE_TRIANGLES,
        .depth_test = true,
        .depth_write = true,
        .properties = FIXED_ARRAY_DEFINE(MaterialPropertyDesc,
            {.name = "uTexture", .type = MAT_PROP_TEXTURE, .binding = 0},
        ),
    });

    material_set_texture(g_demo.material, "uTexture", g_demo.texture);

    LOG_INFO("Triangle renderer texture demo initialized");
}

void app_update_and_render(AppMemory *memory) {
    if (!is_main_thread()) return;

    camera_update(&g_demo.camera, memory->canvas_width, memory->canvas_height);

    renderer_begin_frame(g_demo.camera.view, g_demo.camera.proj,
                         (GpuColor){0.2f, 0.2f, 0.3f, 1.0f}, memory->total_time);

    mat4 model;
    mat4_identity(model);
    mat4_rotate(model, memory->total_time, VEC3(0, 1, 0));

    renderer_draw_mesh(g_demo.mesh, g_demo.material, model);

    renderer_end_frame();
}
