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
#include "shaders/fish_vs.h"
#include "shaders/fish_fs.h"

typedef struct {
  vec4 tint_color;
  f32 tint_offset;
  f32 metallic;
  f32 smoothness;
  f32 wave_frequency;
  f32 wave_speed;
  f32 wave_distance;
  f32 wave_offset;
} MaterialUniforms;

global OsFileOp *g_file_op;
global Camera g_camera;
global GpuMesh_Handle g_mesh;
global Material_Handle g_material;
global GpuTexture g_albedo_tex = {0};
global GpuTexture g_tint_tex = {0};
global GpuTexture g_metallic_gloss_tex = {0};
global f32 g_rotation;

void app_init(AppMemory *memory)
{
  if (!is_main_thread())
  {
    return;
  }

  AppContext *app_ctx = app_ctx_current();

  g_camera = camera_init(VEC3(0, 0, 1.5), VEC3(0, 0, 0), 60.0f);
  renderer_init(&app_ctx->arena, app_ctx->num_threads, (u32)memory->canvas_width, (u32)memory->canvas_height);

  g_albedo_tex = gpu_make_texture("fishAlbedo2.png");
  g_tint_tex = gpu_make_texture("tints.png");
  g_metallic_gloss_tex = gpu_make_texture("fishMetallicGloss.png");

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
    // LOG_INFO("fish_vs: \n %", FMT_STR(fish_vs));
    // LOG_INFO("fish_fs:\n %", FMT_STR(fish_fs));

    g_material = renderer_create_material(&(MaterialDesc){
        .shader_desc =
            (GpuShaderDesc){
                .vs_code = (const char *)fish_vs,
                .fs_code = (const char *)fish_fs,
                .uniform_blocks = FIXED_ARRAY_DEFINE(GpuUniformBlockDesc,
                                                     {.stage = GPU_STAGE_VERTEX_FRAGMENT, .size = sizeof(GlobalUniforms), .binding = 0},
                                                     {.stage = GPU_STAGE_VERTEX_FRAGMENT, .size = sizeof(MaterialUniforms), .binding = 1}, ),
                .texture_bindings = FIXED_ARRAY_DEFINE(GpuTextureBindingDesc,
                                                       GPU_TEXTURE_BINDING_FRAG(1, 0),
                                                       GPU_TEXTURE_BINDING_FRAG(3, 2),
                                                       GPU_TEXTURE_BINDING_FRAG(5, 4), ),
            },
        .vertex_layout = STATIC_MESH_VERTEX_LAYOUT,
        .primitive = GPU_PRIMITIVE_TRIANGLES,
        .depth_test = true,
        .depth_write = true,
        .properties = FIXED_ARRAY_DEFINE(MaterialPropertyDesc,
                                         {.name = "albedo", .type = MAT_PROP_TEXTURE, .binding = 0},
                                         {.name = "tint", .type = MAT_PROP_TEXTURE, .binding = 1},
                                         {.name = "metallic_gloss", .type = MAT_PROP_TEXTURE, .binding = 2},
                                         {.name = "tint_color", .type = MAT_PROP_VEC4, .binding = 1,
                                          .offset = offsetof(MaterialUniforms, tint_color)},
                                         {.name = "tint_offset", .type = MAT_PROP_FLOAT, .binding = 1,
                                          .offset = offsetof(MaterialUniforms, tint_offset)},
                                         {.name = "metallic", .type = MAT_PROP_FLOAT, .binding = 1,
                                          .offset = offsetof(MaterialUniforms, metallic)},
                                         {.name = "smoothness", .type = MAT_PROP_FLOAT, .binding = 1,
                                          .offset = offsetof(MaterialUniforms, smoothness)},
                                         {.name = "wave_frequency", .type = MAT_PROP_FLOAT, .binding = 1,
                                          .offset = offsetof(MaterialUniforms, wave_frequency)},
                                         {.name = "wave_speed", .type = MAT_PROP_FLOAT, .binding = 1,
                                          .offset = offsetof(MaterialUniforms, wave_speed)},
                                         {.name = "wave_distance", .type = MAT_PROP_FLOAT, .binding = 1,
                                          .offset = offsetof(MaterialUniforms, wave_distance)},
                                         {.name = "wave_offset", .type = MAT_PROP_FLOAT, .binding = 1,
                                          .offset = offsetof(MaterialUniforms, wave_offset)}, ),
    });

    material_set_texture(g_material, "albedo", g_albedo_tex);
    material_set_texture(g_material, "tint", g_tint_tex);
    material_set_texture(g_material, "metallic_gloss", g_metallic_gloss_tex);
    material_set_vec4(g_material, "tint_color", (vec4){1.0f, 1.0f, 1.0f, 1.0f});
    material_set_float(g_material, "tint_offset", 0.0f);
    material_set_float(g_material, "metallic", 0.636f);
    material_set_float(g_material, "smoothness", 0.848f);
    material_set_float(g_material, "wave_frequency", 0.03f);
    material_set_float(g_material, "wave_speed", 10.0f);
    material_set_float(g_material, "wave_distance", 3.0f);
    material_set_float(g_material, "wave_offset", 0.0f);

    char *name = string_blob_get(mesh_asset, mesh_asset->name);
    LOG_INFO("Loaded mesh '%'", FMT_STR(name));

    loaded = true;
  }

  g_rotation += memory->dt * 0.5f;

  camera_update(&g_camera, memory->canvas_width, memory->canvas_height);

  renderer_begin_frame(g_camera.view, g_camera.proj,
                       (GpuColor){0.1f, 0.1f, 0.15f, 1.0f}, memory->total_time);

  mat4 model;
  glm_mat4_identity(model);
  // glm_rotate_y(model, g_rotation, model);
  // glm_rotate_y(model, g_rotation, model);
  // glm_scale(model, VEC3(0.01, 0.01, 0.01));
  mat_trs_euler(VEC3_ZERO, VEC3(RAD(90), RAD(55), 0), VEC3(0.01, 0.01, 0.01), model);

  renderer_draw_mesh(g_mesh, g_material, model);

  renderer_end_frame();
}
