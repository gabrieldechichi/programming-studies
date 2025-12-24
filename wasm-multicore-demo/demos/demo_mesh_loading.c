#include "context.h"
#include "lib/thread_context.h"
#include "lib/typedefs.h"
#include "lib/math.h"
#include "os/os.h"
#include "app.h"
#include "mesh.h"
#include "renderer.h"
#include "camera.h"

#define VERTEX_STRIDE 40

static const char *simple_vs =
    "struct GlobalUniforms {\n"
    "    model: mat4x4<f32>,\n"
    "    view: mat4x4<f32>,\n"
    "    proj: mat4x4<f32>,\n"
    "    view_proj: mat4x4<f32>,\n"
    "};\n"
    "\n"
    "@group(0) @binding(0) var<uniform> global: GlobalUniforms;\n"
    "@group(0) @binding(1) var<uniform> color: vec4<f32>;\n"
    "\n"
    "struct VertexInput {\n"
    "    @location(0) position: vec3<f32>,\n"
    "    @location(1) normal: vec3<f32>,\n"
    "    @location(2) vertex_color: vec4<f32>,\n"
    "};\n"
    "\n"
    "struct VertexOutput {\n"
    "    @builtin(position) position: vec4<f32>,\n"
    "    @location(0) world_normal: vec3<f32>,\n"
    "    @location(1) material_color: vec4<f32>,\n"
    "};\n"
    "\n"
    "@vertex\n"
    "fn vs_main(in: VertexInput) -> VertexOutput {\n"
    "    var out: VertexOutput;\n"
    "    let mvp = global.view_proj * global.model;\n"
    "    out.position = mvp * vec4<f32>(in.position, 1.0);\n"
    "    let normal_matrix = mat3x3<f32>(global.model[0].xyz, "
    "global.model[1].xyz, "
    "global.model[2].xyz);\n"
    "    out.world_normal = normalize(normal_matrix * in.normal);\n"
    "    out.material_color = color;\n"
    "    return out;\n"
    "}\n";

static const char *default_fs =
    "const LIGHT_DIR: vec3<f32> = vec3<f32>(0.5, 0.8, 0.3);\n"
    "const AMBIENT: f32 = 0.15;\n"
    "\n"
    "@fragment\n"
    "fn fs_main(@location(0) world_normal: vec3<f32>, @location(1) "
    "material_color: vec4<f32>) "
    "-> @location(0) vec4<f32> {\n"
    "    let light_dir = normalize(LIGHT_DIR);\n"
    "    let n = normalize(world_normal);\n"
    "    let ndotl = max(dot(n, light_dir), 0.0);\n"
    "    let diffuse = AMBIENT + (1.0 - AMBIENT) * ndotl;\n"
    "    return vec4<f32>(material_color.rgb * diffuse, material_color.a);\n"
    "}\n";

typedef enum {
    LOAD_STATE_IDLE,
    LOAD_STATE_LOADING,
    LOAD_STATE_CREATING_GPU_RESOURCES,
    LOAD_STATE_DONE,
    LOAD_STATE_ERROR,
} LoadState;

typedef struct {
    LoadState load_state;
    OsFileOp *file_op;
    u8 *asset_data;
    u32 asset_size;

    Camera camera;
    GpuMesh_Handle mesh;
    Material_Handle material;
    f32 rotation;
} GameState;

global GameState g_state;

void app_init(AppMemory *memory) {
    UNUSED(memory);
    if (!is_main_thread()) {
        return;
    }

    AppContext *app_ctx = app_ctx_current();

    g_state.load_state = LOAD_STATE_IDLE;
    g_state.rotation = 0.0f;

    g_state.camera = camera_init(VEC3(0, 0, 5), VEC3(0, 0, 0), 45.0f);
    renderer_init(&app_ctx->arena, app_ctx->num_threads);

    LOG_INFO("Mesh loading demo initialized");
    LOG_INFO("Starting to load cube.hasset...");

    ThreadContext *tctx = tctx_current();
    g_state.file_op = os_start_read_file("cube.hasset", tctx->task_system);
    if (g_state.file_op) {
        g_state.load_state = LOAD_STATE_LOADING;
    } else {
        LOG_ERROR("Failed to start file read");
        g_state.load_state = LOAD_STATE_ERROR;
    }
}

void create_gpu_resources_from_asset(void) {
    AppContext *app_ctx = app_ctx_current();
    Allocator alloc = make_arena_allocator(&app_ctx->arena);

    ModelBlobAsset *model = (ModelBlobAsset *)g_state.asset_data;
    MeshBlobAsset *meshes = (MeshBlobAsset *)(g_state.asset_data + model->meshes.offset);
    MeshBlobAsset *mesh = &meshes[0];

    f32 *positions = (f32 *)blob_array_get(mesh, mesh->positions);
    f32 *normals = (f32 *)blob_array_get(mesh, mesh->normals);

    u32 vertex_count = mesh->vertex_count;
    u32 vertex_buffer_size = vertex_count * VERTEX_STRIDE;
    f32 *vertices = ALLOC_ARRAY(&alloc, f32, vertex_count * 10);

    for (u32 i = 0; i < vertex_count; i++) {
        u32 dst = i * 10;
        u32 src3 = i * 3;

        vertices[dst + 0] = positions[src3 + 0];
        vertices[dst + 1] = positions[src3 + 1];
        vertices[dst + 2] = positions[src3 + 2];

        vertices[dst + 3] = normals[src3 + 0];
        vertices[dst + 4] = normals[src3 + 1];
        vertices[dst + 5] = normals[src3 + 2];

        vertices[dst + 6] = 1.0f;
        vertices[dst + 7] = 1.0f;
        vertices[dst + 8] = 1.0f;
        vertices[dst + 9] = 1.0f;
    }

    void *indices = blob_array_get(mesh, mesh->indices);
    GpuIndexFormat index_format = (mesh->index_format == INDEX_FORMAT_U16)
        ? GPU_INDEX_FORMAT_U16
        : GPU_INDEX_FORMAT_U32;

    g_state.mesh = renderer_upload_mesh(&(MeshDesc){
        .vertices = vertices,
        .vertex_size = vertex_buffer_size,
        .indices = indices,
        .index_size = mesh->indices.size,
        .index_count = mesh->index_count,
        .index_format = index_format,
    });

    g_state.material = renderer_create_material(&(MaterialDesc){
        .shader_desc =
            (GpuShaderDesc){
                .vs_code = simple_vs,
                .fs_code = default_fs,
                .uniform_blocks =
                    {
                        {.stage = GPU_STAGE_VERTEX,
                         .size = sizeof(GlobalUniforms),
                         .binding = 0},
                        {.stage = GPU_STAGE_VERTEX,
                         .size = sizeof(vec4),
                         .binding = 1},
                    },
                .uniform_block_count = 2,
            },
        .vertex_layout =
            (GpuVertexLayout){
                .stride = VERTEX_STRIDE,
                .attrs =
                    {
                        {.format = GPU_VERTEX_FORMAT_FLOAT3, .offset = 0, .shader_location = 0},
                        {.format = GPU_VERTEX_FORMAT_FLOAT3, .offset = 12, .shader_location = 1},
                        {.format = GPU_VERTEX_FORMAT_FLOAT4, .offset = 24, .shader_location = 2},
                    },
                .attr_count = 3,
            },
        .primitive = GPU_PRIMITIVE_TRIANGLES,
        .depth_test = true,
        .depth_write = true,
        .properties =
            {
                {.name = "color", .type = MAT_PROP_VEC4, .binding = 1},
            },
        .property_count = 1,
    });

    material_set_vec4(g_state.material, "color", (vec4){0.2f, 0.6f, 1.0f, 1.0f});

    char *name = string_blob_get(mesh, mesh->name);
    LOG_INFO("Created GPU mesh '%' with % vertices, % indices",
             FMT_STR(name), FMT_UINT(vertex_count), FMT_UINT(mesh->index_count));

    g_state.load_state = LOAD_STATE_DONE;
}

void app_update_and_render(AppMemory *memory) {
    if (!is_main_thread()) {
        return;
    }

    if (g_state.load_state == LOAD_STATE_LOADING) {
        OsFileReadState read_state = os_check_read_file(g_state.file_op);

        if (read_state == OS_FILE_READ_STATE_COMPLETED) {
            AppContext *app_ctx = app_ctx_current();
            Allocator alloc = make_arena_allocator(&app_ctx->arena);

            PlatformFileData file_data = {0};
            if (os_get_file_data(g_state.file_op, &file_data, &alloc)) {
                g_state.asset_data = file_data.buffer;
                g_state.asset_size = file_data.buffer_len;
                LOG_INFO("File loaded: % bytes", FMT_UINT(g_state.asset_size));
                g_state.load_state = LOAD_STATE_CREATING_GPU_RESOURCES;
            } else {
                LOG_ERROR("Failed to get file data");
                g_state.load_state = LOAD_STATE_ERROR;
            }
        } else if (read_state == OS_FILE_READ_STATE_ERROR) {
            LOG_ERROR("File read error");
            g_state.load_state = LOAD_STATE_ERROR;
        }
        return;
    }

    if (g_state.load_state == LOAD_STATE_CREATING_GPU_RESOURCES) {
        create_gpu_resources_from_asset();
        return;
    }

    if (g_state.load_state != LOAD_STATE_DONE) {
        return;
    }

    g_state.rotation += memory->dt * 0.5f;

    camera_update(&g_state.camera, memory->canvas_width, memory->canvas_height);

    renderer_begin_frame(g_state.camera.view, g_state.camera.proj, (GpuColor){0.1f, 0.1f, 0.15f, 1.0f});

    mat4 model;
    glm_mat4_identity(model);
    glm_rotate_y(model, g_state.rotation, model);

    renderer_draw_mesh(g_state.mesh, g_state.material, model);

    renderer_end_frame();
}
