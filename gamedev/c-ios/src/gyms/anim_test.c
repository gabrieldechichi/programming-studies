#include "../animation.h"
#include "../assets.h"
#include "../camera.h"
#include "../game.h"
#include "../input.h"
#include "../lib/array.h"
#include "../lib/handle.h"
#include "../lib/math.h"
#include "../lib/memory.h"
#include "../lib/typedefs.h"
#include "../renderer.h"
#include "../vendor/cglm/vec3.h"
#include "../vendor/stb/stb_image.h"

typedef struct {
  Handle_Array textures;
  Handle_Array meshes;
} StaticModel;
slice_define(StaticModel);

typedef struct {
  vec3 temp_pos;
  quaternion temp_rot;
  mat4 temp_model_matrix;
  SkinnedModel skinned_model;
  AnimationState animation_state;
} AnimatedModel;
slice_define(AnimatedModel);

#define MAX_ANIM_INSTANCES 65536

typedef struct {
  GameInput input;
  AssetSystem assets;

  DirectionalLightBlock directional_lights;
  PointLightsBlock point_lights;

  Camera camera;

  StaticModel_Slice static_models;
  AnimatedModel_Slice animated_models;
} GameState;

// global data
global GameContext *ctx = NULL;
global GameState *g_game_state = NULL;
global Model3DData_Handle model_asset_handle = {0};
global Image_Handle tex_asset_handle = {0};
global AnimationAsset_Handle test_anim_asset_handle = {0};
global Image *tex1_data = NULL;
global AnimationAsset *test_anim_asset = NULL;
global GameState *game_state;
global Model3DData *sphere_mesh = NULL;
global Handle_Array texture_handles = {0};
global Handle_Array material_handles = {0};
global Handle_Array mesh_handles = {0};
global Animation *anim = {0};

void gym_init(GameMemory *memory) {
  ctx = &memory->ctx;

  game_state = ALLOC(&ctx->allocator, GameState);
  g_game_state = game_state;
  assert(game_state);
  glm_vec3(cast(vec3){0.02, 1.35, 1}, game_state->camera.pos);
  game_state->camera.fov = 20;
  game_state->camera.pitch = 0;

  game_state->input.touches.cap = ARRAY_SIZE(game_state->input.touches.items);

  game_state->static_models =
      slice_new_ALLOC(&ctx->allocator, StaticModel, 10);
  game_state->animated_models =
      slice_new_ALLOC(&ctx->allocator, AnimatedModel, 10);

  asset_system_init(&game_state->assets, &ctx->allocator, 512);
  model_asset_handle =
      asset_request(Model3DData, &game_state->assets, "unichan_adult.hmobj");
  tex_asset_handle = asset_request(Image, &game_state->assets, "xbot_tex.png");
  test_anim_asset_handle = asset_request(AnimationAsset, &game_state->assets,
                                         "unichan_adult_tpose.hasset");
}

void gym_update_and_render(GameMemory *memory) {
  local_persist b32 did_load = false;
  AssetSystem *assets = &game_state->assets;
  asset_system_update(assets, &memory->ctx);

  // temp async load file code
  if (!did_load && asset_system_pending_count(assets) == 0) {
    did_load = true;
    sphere_mesh = asset_get_data(Model3DData, assets, model_asset_handle);
    test_anim_asset =
        asset_get_data(AnimationAsset, assets, test_anim_asset_handle);
    tex1_data = asset_get_data(Image, assets, tex_asset_handle);

    anim = animation_from_asset(test_anim_asset, sphere_mesh, &ctx->allocator);

    texture_handles = arr_new_ALLOC(&ctx->temp_allocator, Handle,
                                          sphere_mesh->num_meshes);
    material_handles = arr_new_ALLOC(&ctx->temp_allocator, Handle,
                                           sphere_mesh->num_meshes);
    mesh_handles = arr_new_ALLOC(&ctx->temp_allocator, Handle,
                                       sphere_mesh->num_meshes);

    ShaderDefine shader_defines[] = {
        {"HM_INSTANCING_ENABLED", 20, SHADER_DEFINE_BOOLEAN, .value.flag = false},
        {"HM_FOG_ENABLED", 13, SHADER_DEFINE_BOOLEAN, .value.flag = false},
        {"HM_SKINNING_ENABLED", 17, SHADER_DEFINE_BOOLEAN, .value.flag = true},
        {"HM_BLENDSHAPES_ENABLED", 20, SHADER_DEFINE_BOOLEAN, .value.flag = true}
    };
    Handle shader = load_shader((LoadShaderParams){
        .vert_shader_path = "materials/standard.vert",
        .frag_shader_path = "materials/standard.frag",
        .defines = shader_defines,
        .define_count = 4
    });
    for (u32 i = 0; i < sphere_mesh->num_meshes; i++) {
      MeshData *mesh = &sphere_mesh->meshes[i];
      bool32 success =
          renderer_create_skmesh_renderer(mesh, &mesh_handles.items[i]);
      success &= renderer_create_texture(tex1_data, &texture_handles.items[i]);

      // Check that texture and mesh creation succeeded before creating material
      assert(success);

      // Create material using dynamic properties system
      MaterialProperty props[] = {
          MAT_PROP_TEX("uTexture", texture_handles.items[i]),
          MAT_PROP_VEC3("uColor", 1.0f, 1.0f, 1.0f),
      };

      material_handles.items[i] = load_material(shader, props, 2);
    }

    // StaticModel static_model = {.texture = tex, .meshes = mesh_handles};
    // slice_append(game_state->static_models, static_model);

    anim = animation_from_asset(test_anim_asset, sphere_mesh, &ctx->allocator);

    AnimatedModel animated_model = {
        .temp_pos = {0, 0, 0},
        .skinned_model = {.meshes =
                              arr_new_ALLOC(&ctx->allocator, SkinnedMesh,
                                                  sphere_mesh->num_meshes),
                          .joint_matrices = arr_new_ALLOC(
                              &ctx->allocator, mat4, sphere_mesh->len_joints)},
        .animation_state = {
            .animation = anim, .speed = 1.0f, .weight = 1.0, .time = 0.0}};

    // Initialize each SkinnedMesh
    for (i32 i = 0; i < (i32)sphere_mesh->num_meshes; i++) {
      MeshData *mesh_data = &sphere_mesh->meshes[i];
      SkinnedMesh *skinned_mesh = &animated_model.skinned_model.meshes.items[i];

      skinned_mesh->mesh_handle = mesh_handles.items[i];
      skinned_mesh->material_handle = material_handles.items[i];

      if (mesh_data->len_blendshapes > 0) {
        skinned_mesh->blendshape_weights = arr_new_ALLOC(
            &ctx->allocator, f32, mesh_data->len_blendshapes);
        skinned_mesh->blendshape_weights.items[0] = 1.0f;
      } else {
        skinned_mesh->blendshape_weights = arr_new_zero(f32);
      }
    }
    animated_model.skinned_model.meshes.len = sphere_mesh->num_meshes;

    quat_from_euler((vec3){0, 0, 0}, animated_model.temp_rot);
    mat_trs(animated_model.temp_pos, animated_model.temp_rot,
            (vec3){0.01, 0.01, 0.01}, animated_model.temp_model_matrix);

    slice_append(game_state->animated_models, animated_model);
  }

  f32 dt = memory->time.dt;

  input_update(&game_state->input, &memory->input_events, memory->time.now);

  // controls (PC)
  camera_update(&game_state->camera, &game_state->input, dt);
  camera_update_uniforms(&game_state->camera, memory->canvas.width,
                         memory->canvas.height);

  // update camera uniforms
  // update light uniforms
  {
    game_state->directional_lights.count = 1;
    game_state->directional_lights.lights[0] = (DirectionalLight){
        .direction = {1, 1, 1}, .color = {1, 1, 1}, .intensity = 1};

    game_state->point_lights.count = 0;

    f32 time = memory->time.now;
    f32 angle1 = time * 0.5f;
    f32 radius = 1.5;
    f32 height = 0.25;
    f32 intensity = 0.1;

    game_state->point_lights.lights[0] = (PointLight){
        .position = {radius * cosf(angle1), height, 3.0f * sinf(angle1)},
        .color = {1.0, 1.0, 1.0},
        .intensity = intensity,
        .innerRadius = 0.5,
        .outerRadius = 3.0};

    renderer_set_lights(&game_state->directional_lights,
                        &game_state->point_lights);
  }

  arr_foreach_ptr(game_state->animated_models, animated_model) {
    animation_update(&animated_model->animation_state, dt);
    animation_evaluate(&animated_model->animation_state,
                       animated_model->skinned_model.joint_matrices);
    // arr_foreach_ptr(animated_model->skinned_model.joint_matrices, m) {
    //   mat4_identity(*m);
    // }
  }

  // render models
  {
    arr_foreach_ptr(game_state->animated_models, 
                    animated_model) {
      renderer_skm_draw(&ctx->temp_allocator, &animated_model->skinned_model,
                        animated_model->temp_model_matrix);
    }
  }

  // end frame
  input_end_frame(&game_state->input);
}
