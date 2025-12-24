#include "context.h"
#include "lib/thread_context.h"
#include "lib/typedefs.h"
#include "lib/math.h"
#include "os/os.h"
#include "app.h"
#include "mesh.h"
#include "renderer.h"
#include "camera.h"

static const char *textured_vs =
    "struct GlobalUniforms {\n"
    "    model: mat4x4<f32>,\n"
    "    view: mat4x4<f32>,\n"
    "    proj: mat4x4<f32>,\n"
    "    view_proj: mat4x4<f32>,\n"
    "};\n"
    "\n"
    "@group(0) @binding(0) var<uniform> global: GlobalUniforms;\n"
    "\n"
    "struct VertexInput {\n"
    "    @location(0) position: vec3<f32>,\n"
    "    @location(1) normal: vec3<f32>,\n"
    "    @location(2) tangent: vec4<f32>,\n"
    "    @location(3) uv: vec2<f32>,\n"
    "};\n"
    "\n"
    "struct VertexOutput {\n"
    "    @builtin(position) position: vec4<f32>,\n"
    "    @location(0) uv: vec2<f32>,\n"
    "    @location(1) world_tangent: vec3<f32>,\n"
    "    @location(2) world_bitangent: vec3<f32>,\n"
    "    @location(3) world_normal: vec3<f32>,\n"
    "};\n"
    "\n"
    "@vertex\n"
    "fn vs_main(in: VertexInput) -> VertexOutput {\n"
    "    var out: VertexOutput;\n"
    "    let mvp = global.view_proj * global.model;\n"
    "    out.position = mvp * vec4<f32>(in.position, 1.0);\n"
    "    out.uv = in.uv;\n"
    "    let normal_matrix = mat3x3<f32>(global.model[0].xyz, global.model[1].xyz, global.model[2].xyz);\n"
    "    out.world_normal = normalize(normal_matrix * in.normal);\n"
    "    out.world_tangent = normalize(normal_matrix * in.tangent.xyz);\n"
    "    out.world_bitangent = cross(out.world_normal, out.world_tangent) * in.tangent.w;\n"
    "    return out;\n"
    "}\n";

static const char *textured_fs =
    "@group(2) @binding(0) var albedo_sampler: sampler;\n"
    "@group(2) @binding(1) var albedo_texture: texture_2d<f32>;\n"
    "@group(2) @binding(2) var normal_sampler: sampler;\n"
    "@group(2) @binding(3) var normal_texture: texture_2d<f32>;\n"
    "\n"
    "const LIGHT_DIR: vec3<f32> = vec3<f32>(0.5, 0.8, 0.3);\n"
    "const AMBIENT: f32 = 0.15;\n"
    "\n"
    "struct FragmentInput {\n"
    "    @location(0) uv: vec2<f32>,\n"
    "    @location(1) world_tangent: vec3<f32>,\n"
    "    @location(2) world_bitangent: vec3<f32>,\n"
    "    @location(3) world_normal: vec3<f32>,\n"
    "};\n"
    "\n"
    "@fragment\n"
    "fn fs_main(in: FragmentInput) -> @location(0) vec4<f32> {\n"
    "    let albedo = textureSample(albedo_texture, albedo_sampler, in.uv).rgb;\n"
    "    let normal_sample = textureSample(normal_texture, normal_sampler, in.uv).rgb;\n"
    "    let tangent_normal = normal_sample * 2.0 - 1.0;\n"
    "    let tbn = mat3x3<f32>(normalize(in.world_tangent), normalize(in.world_bitangent), normalize(in.world_normal));\n"
    "    let world_normal = normalize(tbn * tangent_normal);\n"
    "    let light_dir = normalize(LIGHT_DIR);\n"
    "    let ndotl = max(dot(world_normal, light_dir), 0.0);\n"
    "    let diffuse = AMBIENT + (1.0 - AMBIENT) * ndotl;\n"
    "    return vec4<f32>(albedo * diffuse, 1.0);\n"
    "}\n";

global OsFileOp *g_file_op;
global Camera g_camera;
global GpuMesh_Handle g_mesh;
global Material_Handle g_material;
global GpuTexture g_albedo_tex = {0};
global GpuTexture g_normal_tex = {0};
global f32 g_rotation;

void app_init(AppMemory *memory) {
  UNUSED(memory);
  if (!is_main_thread()) {
    return;
  }

  AppContext *app_ctx = app_ctx_current();

  g_camera = camera_init(VEC3(0, 0, 5), VEC3(0, 0, 0), 45.0f);
  renderer_init(&app_ctx->arena, app_ctx->num_threads);

  g_albedo_tex = gpu_make_texture("cube_albedo.png");
  g_normal_tex = gpu_make_texture("cube_normal.png");

  ThreadContext *tctx = tctx_current();
  g_file_op = os_start_read_file("cube.hasset", tctx->task_system);
}

void app_update_and_render(AppMemory *memory) {
  if (!is_main_thread()) {
    return;
  }

  local_persist b32 loaded = false;

  if (!loaded) {
    OsFileReadState state = os_check_read_file(g_file_op);
    if (state != OS_FILE_READ_STATE_COMPLETED) {
      return;
    }

    AppContext *app_ctx = app_ctx_current();
    Allocator alloc = make_arena_allocator(&app_ctx->arena);

    PlatformFileData file_data = {0};
    os_get_file_data(g_file_op, &file_data, &alloc);

    ModelBlobAsset *model = (ModelBlobAsset *)file_data.buffer;
    MeshBlobAsset *mesh_asset =
        (MeshBlobAsset *)(file_data.buffer + model->meshes.offset);

    MeshDesc mesh_desc = mesh_asset_to_mesh(mesh_asset, &alloc);
    g_mesh = renderer_upload_mesh(&mesh_desc);

    g_material = renderer_create_material(&(MaterialDesc){
        .shader_desc =
            (GpuShaderDesc){
                .vs_code = textured_vs,
                .fs_code = textured_fs,
                .uniform_blocks =
                    {
                        {.stage = GPU_STAGE_VERTEX,
                         .size = sizeof(GlobalUniforms),
                         .binding = 0},
                    },
                .uniform_block_count = 1,
                .texture_bindings =
                    {
                        {.stage = GPU_STAGE_FRAGMENT,
                         .sampler_binding = 0,
                         .texture_binding = 1},
                        {.stage = GPU_STAGE_FRAGMENT,
                         .sampler_binding = 2,
                         .texture_binding = 3},
                    },
                .texture_binding_count = 2,
            },
        .vertex_layout =
            (GpuVertexLayout){
                .stride = MESH_VERTEX_STRIDE,
                .attrs =
                    {
                        {.format = GPU_VERTEX_FORMAT_FLOAT3,
                         .offset = 0,
                         .shader_location = 0},
                        {.format = GPU_VERTEX_FORMAT_FLOAT3,
                         .offset = 12,
                         .shader_location = 1},
                        {.format = GPU_VERTEX_FORMAT_FLOAT4,
                         .offset = 24,
                         .shader_location = 2},
                        {.format = GPU_VERTEX_FORMAT_FLOAT2,
                         .offset = 40,
                         .shader_location = 3},
                    },
                .attr_count = 4,
            },
        .primitive = GPU_PRIMITIVE_TRIANGLES,
        .depth_test = true,
        .depth_write = true,
        .properties =
            {
                {.name = "albedo", .type = MAT_PROP_TEXTURE, .binding = 0},
                {.name = "normal", .type = MAT_PROP_TEXTURE, .binding = 1},
            },
        .property_count = 2,
    });

    material_set_texture(g_material, "albedo", g_albedo_tex);
    material_set_texture(g_material, "normal", g_normal_tex);

    char *name = string_blob_get(mesh_asset, mesh_asset->name);
    LOG_INFO("Loaded mesh '%'", FMT_STR(name));

    loaded = true;
  }

  g_rotation += memory->dt * 0.5f;

  camera_update(&g_camera, memory->canvas_width, memory->canvas_height);

  renderer_begin_frame(g_camera.view, g_camera.proj,
                       (GpuColor){0.1f, 0.1f, 0.15f, 1.0f});

  mat4 model;
  glm_mat4_identity(model);
  glm_rotate_y(model, g_rotation, model);

  renderer_draw_mesh(g_mesh, g_material, model);

  renderer_end_frame();
}
