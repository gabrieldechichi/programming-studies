#include "context.h"
#include "lib/thread_context.h"
#include "lib/typedefs.h"
#include "lib/math.h"
#include "os/os.h"
#include "app.h"
#include "mesh.h"
#include "renderer.h"
#include "camera.h"
#include "mesh.h"

static const char *pbr_vs =
    "struct GlobalUniforms {\n"
    "    model: mat4x4<f32>,\n"
    "    view: mat4x4<f32>,\n"
    "    proj: mat4x4<f32>,\n"
    "    view_proj: mat4x4<f32>,\n"
    "    camera_pos: vec3<f32>,\n"
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
    "    @location(1) world_normal: vec3<f32>,\n"
    "    @location(2) world_position: vec3<f32>,\n"
    "};\n"
    "\n"
    "@vertex\n"
    "fn vs_main(in: VertexInput) -> VertexOutput {\n"
    "    var out: VertexOutput;\n"
    "    let world_pos = global.model * vec4<f32>(in.position, 1.0);\n"
    "    out.position = global.view_proj * world_pos;\n"
    "    out.world_position = world_pos.xyz;\n"
    "    out.uv = in.uv;\n"
    "    let normal_matrix = mat3x3<f32>(global.model[0].xyz, "
    "global.model[1].xyz, global.model[2].xyz);\n"
    "    out.world_normal = normalize(normal_matrix * in.normal);\n"
    "    return out;\n"
    "}\n";

static const char *pbr_fs =
    "struct MaterialUniforms {\n"
    "    metallic: f32,\n"
    "    smoothness: f32,\n"
    "};\n"
    "@group(0) @binding(1) var<uniform> material: MaterialUniforms;\n"
    "\n"
    "@group(2) @binding(0) var albedo_sampler: sampler;\n"
    "@group(2) @binding(1) var albedo_texture: texture_2d<f32>;\n"
    "\n"
    "const PI: f32 = 3.14159265359;\n"
    "const LIGHT_DIR: vec3<f32> = vec3<f32>(0.5, 0.8, 0.3);\n"
    "const LIGHT_COLOR: vec3<f32> = vec3<f32>(0.663, 0.973, 1.0);\n"
    "const AMBIENT: vec3<f32> = vec3<f32>(0.2, 0.2, 0.2);\n"
    "const DIELECTRIC_F0: f32 = 0.04;\n"
    "\n"
    "struct FragmentInput {\n"
    "    @location(0) uv: vec2<f32>,\n"
    "    @location(1) world_normal: vec3<f32>,\n"
    "    @location(2) world_position: vec3<f32>,\n"
    "};\n"
    "\n"
    "fn fresnelSchlick(cosTheta: f32, F0: vec3<f32>) -> vec3<f32> {\n"
    "    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);\n"
    "}\n"
    "\n"
    "@fragment\n"
    "fn fs_main(in: FragmentInput) -> @location(0) vec4<f32> {\n"
    "    let albedo = textureSample(albedo_texture, albedo_sampler, in.uv).rgb;\n"
    "    let metallic = material.metallic;\n"
    "    let smoothness = material.smoothness;\n"
    "    let perceptual_roughness = 1.0 - smoothness;\n"
    "    let roughness = perceptual_roughness * perceptual_roughness;\n"
    "    let roughness2 = roughness * roughness;\n"
    "\n"
    "    let one_minus_reflectivity = (1.0 - DIELECTRIC_F0) * (1.0 - metallic);\n"
    "    let diffuse_color = albedo * one_minus_reflectivity;\n"
    "    let specular_color = mix(vec3<f32>(DIELECTRIC_F0), albedo, metallic);\n"
    "\n"
    "    let N = normalize(in.world_normal);\n"
    "    let V = normalize(global.camera_pos - in.world_position);\n"
    "    let L = normalize(LIGHT_DIR);\n"
    "    let H = normalize(L + V);\n"
    "\n"
    "    let NdotL = max(dot(N, L), 0.0);\n"
    "    let NdotV = max(dot(N, V), 0.0001);\n"
    "    let NdotH = max(dot(N, H), 0.0);\n"
    "    let LdotH = max(dot(L, H), 0.0);\n"
    "\n"
    "    // GGX Distribution (D term)\n"
    "    let d = NdotH * NdotH * (roughness2 - 1.0) + 1.0;\n"
    "    let D = roughness2 / (PI * d * d);\n"
    "\n"
    "    // Fresnel (F term)\n"
    "    let F = fresnelSchlick(LdotH, specular_color);\n"
    "\n"
    "    // Visibility approximation (simplified V term from URP)\n"
    "    let normalization = roughness * 4.0 + 2.0;\n"
    "    let specular_term = D / (max(LdotH * LdotH, 0.1) * normalization);\n"
    "\n"
    "    // Direct lighting\n"
    "    let radiance = LIGHT_COLOR * NdotL;\n"
    "    let direct = (diffuse_color + specular_term * F) * radiance;\n"
    "\n"
    "    // Ambient (simple)\n"
    "    let ambient = AMBIENT * albedo;\n"
    "\n"
    "    let final_color = ambient + direct;\n"
    "    return vec4<f32>(final_color, 1.0);\n"
    "}\n";

typedef struct {
  f32 metallic;
  f32 smoothness;
} MaterialUniforms;

global OsFileOp *g_file_op;
global Camera g_camera;
global GpuMesh_Handle g_mesh;
global Material_Handle g_material;
global GpuTexture g_albedo_tex = {0};
global f32 g_rotation;

void app_init(AppMemory *memory)
{
  UNUSED(memory);
  if (!is_main_thread())
  {
    return;
  }

  AppContext *app_ctx = app_ctx_current();

  g_camera = camera_init(VEC3(0, 0, 1.5), VEC3(0, 0, 0), 60.0f);
  renderer_init(&app_ctx->arena, app_ctx->num_threads);

  g_albedo_tex = gpu_make_texture("fishAlbedo2.png");

  ThreadContext *tctx = tctx_current();
  g_file_op = os_start_read_file("fish.hasset", tctx->task_system);
}

void app_update_and_render(AppMemory *memory)
{
  if (!is_main_thread())
  {
    return;
  }

  local_persist b32 loaded = false;

  if (!loaded)
  {
    OsFileReadState state = os_check_read_file(g_file_op);
    if (state != OS_FILE_READ_STATE_COMPLETED)
    {
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
                .vs_code = pbr_vs,
                .fs_code = pbr_fs,
                .uniform_blocks = FIXED_ARRAY_DEFINE(GpuUniformBlockDesc,
                                                     {.stage = GPU_STAGE_VERTEX_FRAGMENT, .size = sizeof(GlobalUniforms), .binding = 0},
                                                     GPU_UNIFORM_DESC_FRAG(MaterialUniforms, 1), ),
                .texture_bindings = FIXED_ARRAY_DEFINE(GpuTextureBindingDesc,
                                                       GPU_TEXTURE_BINDING_FRAG(1, 0), ),
            },
        .vertex_layout = STATIC_MESH_VERTEX_LAYOUT,
        .primitive = GPU_PRIMITIVE_TRIANGLES,
        .depth_test = true,
        .depth_write = true,
        .properties = FIXED_ARRAY_DEFINE(MaterialPropertyDesc,
                                         {.name = "albedo", .type = MAT_PROP_TEXTURE, .binding = 0},
                                         {.name = "metallic", .type = MAT_PROP_FLOAT, .binding = 1,
                                          .offset = offsetof(MaterialUniforms, metallic)},
                                         {.name = "smoothness", .type = MAT_PROP_FLOAT, .binding = 1,
                                          .offset = offsetof(MaterialUniforms, smoothness)}, ),
    });

    material_set_texture(g_material, "albedo", g_albedo_tex);
    material_set_float(g_material, "metallic", 0.5f);
    material_set_float(g_material, "smoothness", 0.5f);

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
  // glm_rotate_y(model, g_rotation, model);
  // glm_rotate_y(model, g_rotation, model);
  // glm_scale(model, VEC3(0.01, 0.01, 0.01));
  mat_trs_euler(VEC3_ZERO, VEC3(RAD(90), RAD(55), 0), VEC3(0.01, 0.01, 0.01), model);

  renderer_draw_mesh(g_mesh, g_material, model);

  renderer_end_frame();
}
