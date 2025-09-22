#include "../animation.h"
#include "../assets.h"
#include "../camera.h"
#include "../game.h"
#include "../gameplay_lib.c"
#include "../lib/array.h"
#include "../lib/audio.h"
#include "../lib/lipsync.h"
#include "../lib/math.h"
#include "../lib/memory.h"
#include "../lib/typedefs.h"
#include "../platform.h"
#include "../renderer.h"
#include "../vendor/cglm/vec3.h"
#include "../vendor/stb/stb_image.h"

// Include the profile data
#include "lip_sync_profile.c"

// Include test data (generated from Unity)
#include "test_data.c"

static PhonemeBlendshapeDefinition phoneme_blendshape_definitions[] = {
    {"A", "MTH_A"}, {"I", "MTH_I"}, {"U", "MTH_U"},
    {"E", "MTH_E"}, {"O", "MTH_O"},
};

// Global MaterialAsset definitions - complete material definitions
static MaterialAsset g_eyebrow_material;
static MaterialAsset g_cloth_material;
static MaterialAsset g_cheek_material;
static MaterialAsset g_eyeline_material;
static MaterialAsset g_eye_base_material;
static MaterialAsset g_face_material;
static MaterialAsset g_eye_l_material;
static MaterialAsset g_eye_r_material;
static MaterialAsset g_hair_material;
static MaterialAsset g_face_blendshape_material;
static MaterialAsset g_skin_material;

// Mesh-to-material mapping (mesh index -> MaterialAsset)
static MaterialAsset *g_mesh_to_material[] = {
    &g_eyebrow_material,         // 0: BLW_DEF (eyebrows)
    &g_cloth_material,           // 1: button
    &g_cheek_material,           // 2: cheeks
    &g_eyeline_material,         // 3: EL_DEF (eyeline)
    &g_eye_base_material,        // 4: eye_base_old
    &g_face_material,            // 5: eyebase 2
    &g_eye_l_material,           // 6: eye l
    &g_eye_r_material,           // 7: eye r
    &g_cloth_material,           // 8: hair_accce
    &g_hair_material,            // 9: hair_front
    &g_hair_material,            // 10: hair_frontside
    &g_cloth_material,           // 11: hairband
    &g_face_material,            // 12: head_back
    &g_cloth_material,           // 13: Leg
    &g_face_blendshape_material, // 14: MTH_DEF (face with blendshapes)
    &g_cloth_material,           // 15: Shirts
    &g_cloth_material,           // 16: shirts_sode
    &g_cloth_material,           // 17: shirts_sode bk
    &g_skin_material,            // 18: skin
    &g_hair_material,            // 19: tail
    &g_hair_material,            // 20: tail bottom
    &g_cloth_material,           // 21: uwagi
    &g_cloth_material,           // 22: uwagi_bk
};
#define MODEL_MATERIAL_COUNT ARRAY_SIZE(g_mesh_to_material)

typedef struct {
  mat4 model_matrix;
  SkinnedModel skinned_model;
  AnimationState animation_state;
  LipsyncBlendshapeController face_blendshapes;
  LipSyncContext face_lipsync;
} Character;

typedef struct {
  AssetSystem asset_system;
  AudioState audio_system;

  // assets
  Model3DData_Handle model_asset_handle;
  AnimationAsset_Handle test_anim_asset_handle;
  Model3DData *model_data;
  AnimationAsset *test_anim_asset;
  Animation *anim;
  Material_Slice materials;
  WavFile_Handle wav_file_handle;

  // 3D scene data
  DirectionalLightBlock directional_lights;
  PointLightsBlock point_lights;
  Camera camera;
  GameInput input;

  vec3 directional_light;

  Character character;

  // Loading state
  bool32 assets_loaded;
  f32 next_play_time;
} GymState;

global GymState *g_lipsync_state;

void initialize_material_assets(Allocator *allocator) {
  // Create shader defines for materials (with and without blendshapes)
  ShaderDefine_Array standard_shader_defines = arr_from_c_array_alloc(
      ShaderDefine, allocator,
      ((ShaderDefine[]){SHADER_DEFINE_BOOL("HM_INSTANCING_ENABLED", false),
                        SHADER_DEFINE_BOOL("HM_FOG_ENABLED", false),
                        SHADER_DEFINE_BOOL("HM_SKINNING_ENABLED", true),
                        SHADER_DEFINE_BOOL("HM_BLENDSHAPES_ENABLED", false)}));

  ShaderDefine_Array blendshape_shader_defines = arr_from_c_array_alloc(
      ShaderDefine, allocator,
      ((ShaderDefine[]){SHADER_DEFINE_BOOL("HM_INSTANCING_ENABLED", false),
                        SHADER_DEFINE_BOOL("HM_FOG_ENABLED", false),
                        SHADER_DEFINE_BOOL("HM_SKINNING_ENABLED", true),
                        SHADER_DEFINE_BOOL("HM_BLENDSHAPES_ENABLED", true)}));
  // Initialize eyebrow material (transparent skin with eyeline texture)
  g_eyebrow_material = (MaterialAsset){
      .shader_path = STR_FROM_CSTR("materials/unitychan/unichan_skin.frag"),
      .transparent = true,
      .shader_defines = standard_shader_defines,
      .properties = arr_from_c_array_alloc(
          MaterialAssetProperty, allocator,
          ((MaterialAssetProperty[]){
              {.name = STR_FROM_CSTR("uTexture"),
               .type = MAT_PROP_TEXTURE,
               .texture_path = STR_FROM_CSTR("unity_chan/eyeline_00.png")},
              {.name = STR_FROM_CSTR("uFalloffTexture"),
               .type = MAT_PROP_TEXTURE,
               .texture_path = STR_FROM_CSTR("unity_chan/FO_SKIN1.png")},
              {.name = STR_FROM_CSTR("uRimLightTexture"),
               .type = MAT_PROP_TEXTURE,
               .texture_path = STR_FROM_CSTR("unity_chan/FO_RIM1.png")},
              {.name = STR_FROM_CSTR("uColor"),
               .type = MAT_PROP_VEC3,
               .color = COLOR_RGBA(1.0f, 1.0f, 1.0f, 1.0f)}}))};

  // Initialize cloth material (hair shader with cloth textures)
  g_cloth_material = (MaterialAsset){
      .shader_path = STR_FROM_CSTR("materials/unitychan/unichan_main.frag"),
      .transparent = false,
      .shader_defines = standard_shader_defines,
      .properties = arr_from_c_array_alloc(
          MaterialAssetProperty, allocator,
          ((MaterialAssetProperty[]){
              {.name = STR_FROM_CSTR("uTexture"),
               .type = MAT_PROP_TEXTURE,
               .texture_path = STR_FROM_CSTR("unity_chan/body_01.png")},
              {.name = STR_FROM_CSTR("uFalloffTexture"),
               .type = MAT_PROP_TEXTURE,
               .texture_path = STR_FROM_CSTR("unity_chan/FO_CLOTH1.png")},
              {.name = STR_FROM_CSTR("uRimLightTexture"),
               .type = MAT_PROP_TEXTURE,
               .texture_path = STR_FROM_CSTR("unity_chan/FO_RIM1.png")},
              {.name = STR_FROM_CSTR("uSpecularReflectionTexture"),
               .type = MAT_PROP_TEXTURE,
               .texture_path = STR_FROM_CSTR("unity_chan/body_01_SPEC.png")},
              {.name = STR_FROM_CSTR("uColor"),
               .type = MAT_PROP_VEC3,
               .color = COLOR_RGBA(0.9f, 0.9f, 0.9f, 1.0f)}}))};

  // Initialize cheek material (transparent skin with cheek texture)
  g_cheek_material = (MaterialAsset){
      .shader_path = STR_FROM_CSTR("materials/unitychan/unichan_skin.frag"),
      .transparent = true,
      .shader_defines = standard_shader_defines,
      .properties = arr_from_c_array_alloc(
          MaterialAssetProperty, allocator,
          ((MaterialAssetProperty[]){
              {.name = STR_FROM_CSTR("uTexture"),
               .type = MAT_PROP_TEXTURE,
               .texture_path = STR_FROM_CSTR("unity_chan/cheek_00.png")},
              {.name = STR_FROM_CSTR("uFalloffTexture"),
               .type = MAT_PROP_TEXTURE,
               .texture_path = STR_FROM_CSTR("unity_chan/FO_SKIN1.png")},
              {.name = STR_FROM_CSTR("uRimLightTexture"),
               .type = MAT_PROP_TEXTURE,
               .texture_path = STR_FROM_CSTR("unity_chan/FO_RIM1.png")},
              {.name = STR_FROM_CSTR("uColor"),
               .type = MAT_PROP_VEC3,
               .color = COLOR_RGBA(1.0f, 1.0f, 1.0f, 1.0f)}}))};

  // Initialize eyeline material (same as eyebrow)
  g_eyeline_material = g_eyebrow_material;

  // Initialize eye base material (opaque skin with eyeline texture)
  g_eye_base_material = (MaterialAsset){
      .shader_path = STR_FROM_CSTR("materials/unitychan/unichan_skin.frag"),
      .transparent = false,
      .shader_defines = standard_shader_defines,
      .properties = arr_from_c_array_alloc(
          MaterialAssetProperty, allocator,
          ((MaterialAssetProperty[]){
              {.name = STR_FROM_CSTR("uTexture"),
               .type = MAT_PROP_TEXTURE,
               .texture_path = STR_FROM_CSTR("unity_chan/eyeline_00.png")},
              {.name = STR_FROM_CSTR("uFalloffTexture"),
               .type = MAT_PROP_TEXTURE,
               .texture_path = STR_FROM_CSTR("unity_chan/FO_SKIN1.png")},
              {.name = STR_FROM_CSTR("uRimLightTexture"),
               .type = MAT_PROP_TEXTURE,
               .texture_path = STR_FROM_CSTR("unity_chan/FO_RIM1.png")},
              {.name = STR_FROM_CSTR("uColor"),
               .type = MAT_PROP_VEC3,
               .color = COLOR_RGBA(1.0f, 1.0f, 1.0f, 1.0f)}}))};

  // Initialize face material (opaque skin with face texture)
  g_face_material = (MaterialAsset){
      .shader_path = STR_FROM_CSTR("materials/unitychan/unichan_skin.frag"),
      .transparent = false,
      .shader_defines = standard_shader_defines,
      .properties = arr_from_c_array_alloc(
          MaterialAssetProperty, allocator,
          ((MaterialAssetProperty[]){
              {.name = STR_FROM_CSTR("uTexture"),
               .type = MAT_PROP_TEXTURE,
               .texture_path = STR_FROM_CSTR("unity_chan/face_00.png")},
              {.name = STR_FROM_CSTR("uFalloffTexture"),
               .type = MAT_PROP_TEXTURE,
               .texture_path = STR_FROM_CSTR("unity_chan/FO_SKIN1.png")},
              {.name = STR_FROM_CSTR("uRimLightTexture"),
               .type = MAT_PROP_TEXTURE,
               .texture_path = STR_FROM_CSTR("unity_chan/FO_RIM1.png")},
              {.name = STR_FROM_CSTR("uColor"),
               .type = MAT_PROP_VEC3,
               .color = COLOR_RGBA(1.0f, 1.0f, 1.0f, 1.0f)}}))};

  // Initialize left eye material (transparent skin with left eye texture)
  g_eye_l_material = (MaterialAsset){
      .shader_path = STR_FROM_CSTR("materials/unitychan/unichan_skin.frag"),
      .transparent = true,
      .shader_defines = standard_shader_defines,
      .properties = arr_from_c_array_alloc(
          MaterialAssetProperty, allocator,
          ((MaterialAssetProperty[]){
              {.name = STR_FROM_CSTR("uTexture"),
               .type = MAT_PROP_TEXTURE,
               .texture_path = STR_FROM_CSTR("unity_chan/eye_iris_L_00.png")},
              {.name = STR_FROM_CSTR("uFalloffTexture"),
               .type = MAT_PROP_TEXTURE,
               .texture_path = STR_FROM_CSTR("unity_chan/FO_SKIN1.png")},
              {.name = STR_FROM_CSTR("uRimLightTexture"),
               .type = MAT_PROP_TEXTURE,
               .texture_path = STR_FROM_CSTR("unity_chan/FO_RIM1.png")},
              {.name = STR_FROM_CSTR("uColor"),
               .type = MAT_PROP_VEC3,
               .color = COLOR_RGBA(1.0f, 1.0f, 1.0f, 1.0f)}}))};

  // Initialize right eye material (transparent skin with right eye texture)
  g_eye_r_material = (MaterialAsset){
      .shader_path = STR_FROM_CSTR("materials/unitychan/unichan_skin.frag"),
      .transparent = true,
      .shader_defines = standard_shader_defines,
      .properties = arr_from_c_array_alloc(
          MaterialAssetProperty, allocator,
          ((MaterialAssetProperty[]){
              {.name = STR_FROM_CSTR("uTexture"),
               .type = MAT_PROP_TEXTURE,
               .texture_path = STR_FROM_CSTR("unity_chan/eye_iris_R_00.png")},
              {.name = STR_FROM_CSTR("uFalloffTexture"),
               .type = MAT_PROP_TEXTURE,
               .texture_path = STR_FROM_CSTR("unity_chan/FO_SKIN1.png")},
              {.name = STR_FROM_CSTR("uRimLightTexture"),
               .type = MAT_PROP_TEXTURE,
               .texture_path = STR_FROM_CSTR("unity_chan/FO_RIM1.png")},
              {.name = STR_FROM_CSTR("uColor"),
               .type = MAT_PROP_VEC3,
               .color = COLOR_RGBA(1.0f, 1.0f, 1.0f, 1.0f)}}))};

  // Initialize hair material (hair shader with hair texture)
  g_hair_material = (MaterialAsset){
      .shader_path = STR_FROM_CSTR("materials/unitychan/unichan_main.frag"),
      .transparent = false,
      .shader_defines = standard_shader_defines,
      .properties = arr_from_c_array_alloc(
          MaterialAssetProperty, allocator,
          ((MaterialAssetProperty[]){
              {.name = STR_FROM_CSTR("uTexture"),
               .type = MAT_PROP_TEXTURE,
               .texture_path = STR_FROM_CSTR("unity_chan/hair_01.png")},
              {.name = STR_FROM_CSTR("uFalloffTexture"),
               .type = MAT_PROP_TEXTURE,
               .texture_path = STR_FROM_CSTR("unity_chan/FO_CLOTH1.png")},
              {.name = STR_FROM_CSTR("uRimLightTexture"),
               .type = MAT_PROP_TEXTURE,
               .texture_path = STR_FROM_CSTR("unity_chan/FO_RIM1.png")},
              {.name = STR_FROM_CSTR("uSpecularReflectionTexture"),
               .type = MAT_PROP_TEXTURE,
               .texture_path = STR_FROM_CSTR("unity_chan/hair_01_SPEC.png")},
              {.name = STR_FROM_CSTR("uColor"),
               .type = MAT_PROP_VEC3,
               .color = COLOR_RGBA(1.0f, 1.0f, 1.0f, 1.0f)}}))};

  // Initialize face blendshape material (same as face but with blendshapes)
  g_face_blendshape_material = g_face_material;
  g_face_blendshape_material.shader_defines = blendshape_shader_defines;

  // Initialize skin material (opaque skin with skin texture)
  g_skin_material = (MaterialAsset){
      .shader_path = STR_FROM_CSTR("materials/unitychan/unichan_skin.frag"),
      .transparent = false,
      .shader_defines = standard_shader_defines,
      .properties = arr_from_c_array_alloc(
          MaterialAssetProperty, allocator,
          ((MaterialAssetProperty[]){
              {.name = STR_FROM_CSTR("uTexture"),
               .type = MAT_PROP_TEXTURE,
               .texture_path = STR_FROM_CSTR("unity_chan/skin_01.png")},
              {.name = STR_FROM_CSTR("uFalloffTexture"),
               .type = MAT_PROP_TEXTURE,
               .texture_path = STR_FROM_CSTR("unity_chan/FO_SKIN1.png")},
              {.name = STR_FROM_CSTR("uRimLightTexture"),
               .type = MAT_PROP_TEXTURE,
               .texture_path = STR_FROM_CSTR("unity_chan/FO_RIM1.png")},
              {.name = STR_FROM_CSTR("uColor"),
               .type = MAT_PROP_VEC3,
               .color = COLOR_RGBA(1.0f, 1.0f, 1.0f, 1.0f)}}))};
}

void gym_init(GameMemory *memory) {
  GameContext *ctx = &memory->ctx;

  g_lipsync_state = ALLOC(&ctx->allocator, sizeof(GymState));
  // g_lipsync_state->directional_light[0] = -20;
  g_lipsync_state->directional_light[1] = -20;
  g_lipsync_state->audio_system = audio_init(ctx);

  // Initialize 3D rendering and assets
  g_lipsync_state->asset_system = asset_system_init(&ctx->allocator, 512);
  g_lipsync_state->model_asset_handle = asset_request(
      Model3DData, &g_lipsync_state->asset_system, ctx, "unichan_adult.hmobj");
  g_lipsync_state->test_anim_asset_handle =
      asset_request(AnimationAsset, &g_lipsync_state->asset_system, ctx,
                    "unichan_adult_tpose.hasset");
  g_lipsync_state->wav_file_handle = asset_request(
      WavFile, &g_lipsync_state->asset_system, ctx, "univ0023.wav");

  // Initialize material assets first
  initialize_material_assets(&ctx->allocator);

  // Initialize materials - create slice of Material instances
  g_lipsync_state->materials =
      slice_new_ALLOC(&ctx->allocator, Material, MODEL_MATERIAL_COUNT);

  for (u32 i = 0; i < MODEL_MATERIAL_COUNT; i++) {
    MaterialAsset *asset = g_mesh_to_material[i];
    Material *material =
        material_from_asset(asset, &g_lipsync_state->asset_system, ctx);
    slice_append(g_lipsync_state->materials, *material);
  }

  // Initialize camera
  glm_vec3(cast(vec3){0.0, 1.38, 0.67}, g_lipsync_state->camera.pos);
  g_lipsync_state->camera.fov = 60;
  g_lipsync_state->camera.pitch = -15;

  g_lipsync_state->assets_loaded = false;
  g_lipsync_state->next_play_time = 999999.0f;
}

void gym_update_and_render(GameMemory *memory) {
  GameContext *ctx = &memory->ctx;
  GameTime *time = &memory->time;

  f32 dt = time->dt;
  // Asset loading and model initialization
  asset_system_update(&g_lipsync_state->asset_system, ctx);
  audio_update(&g_lipsync_state->audio_system);

  input_update(&g_lipsync_state->input, &memory->input_events,
               memory->time.now);

  // camera_update(&g_lipsync_state->camera, &g_lipsync_state->input, dt);
  if (!g_lipsync_state->assets_loaded &&
      asset_system_pending_count(&g_lipsync_state->asset_system) == 0) {
    g_lipsync_state->assets_loaded = true;

    WavFile *wav_file = asset_get_data(WavFile, &g_lipsync_state->asset_system,
                                       g_lipsync_state->wav_file_handle);
    if (wav_file) {
      LOG_INFO("WAV file loaded: %d Hz, %d channels, %d samples",
               FMT_INT(wav_file->format.sample_rate),
               FMT_INT(wav_file->format.channels),
               FMT_INT(wav_file->total_samples));

      g_lipsync_state->next_play_time = time->now + 0.5f;
    }

    g_lipsync_state->model_data =
        asset_get_data(Model3DData, &g_lipsync_state->asset_system,
                       g_lipsync_state->model_asset_handle);
    g_lipsync_state->test_anim_asset =
        asset_get_data(AnimationAsset, &g_lipsync_state->asset_system,
                       g_lipsync_state->test_anim_asset_handle);

    g_lipsync_state->anim =
        animation_from_asset(g_lipsync_state->test_anim_asset,
                             g_lipsync_state->model_data, &ctx->allocator);

    // initialize character
    {
      Character *entity = &g_lipsync_state->character;

      quaternion temp_rot;
      quat_from_euler((vec3){0, glm_rad(10), 0}, temp_rot);
      mat_trs((vec3){-0.00, 0, 0}, temp_rot, (vec3){0.01, 0.01, 0.01},
              entity->model_matrix);
      // mat_trs((vec3){0, 0, 0}, temp_rot, (vec3){1,1,1},
      //         g_lipsync_state->model_matrix);

      entity->face_lipsync = lipsync_init(
          &ctx->allocator, g_lipsync_state->audio_system.output_sample_rate,
          &lip_sync_profile);

      entity->skinned_model = skmodel_from_asset(
          ctx, g_lipsync_state->model_data, g_lipsync_state->materials);

      entity->animation_state = (AnimationState){
          .animation = g_lipsync_state->anim,
          .speed = 1.0f,
          .weight = 1.0f,
          .time = 0.0f,
      };

      SkinnedMesh *face_mesh = &entity->skinned_model.meshes.items[14];
      // Initialize blendshape controller for the face mesh (index 14)
      entity->face_blendshapes = blendshape_controller_init(
          &ctx->allocator, &lip_sync_profile, phoneme_blendshape_definitions,
          ARRAY_SIZE(phoneme_blendshape_definitions), face_mesh);

      LOG_INFO("3D Model and blendshapes initialized");
    }
  }

  if (time->now > g_lipsync_state->next_play_time) {
    WavFile *wav_file = asset_get_data(WavFile, &g_lipsync_state->asset_system,
                                       g_lipsync_state->wav_file_handle);
    if (wav_file) {

      // Create and play audio clip
      AudioClip clip = {.wav_file = wav_file, .loop = false};
      audio_play_clip(&g_lipsync_state->audio_system, clip);

      g_lipsync_state->next_play_time = time->now + 3;
    }
  }

  // lipsync
  if (g_lipsync_state->assets_loaded) {
    Character *entity = &g_lipsync_state->character;
    // Feed audio to lipsync system
    AudioState *audio_system = &g_lipsync_state->audio_system;
    LipSyncContext *lipsync = &entity->face_lipsync;
    lipsync_feed_audio(lipsync, ctx, audio_system->sample_buffer,
                       audio_system->sample_buffer_len,
                       audio_system->output_channels);

    // Process and get results
    if (lipsync_process(lipsync, ctx)) {
      LipSyncResult result = lipsync_get_result(lipsync);

      LipsyncBlendshapeController *blendshape_controller =
          &entity->face_blendshapes;

      blendshape_controller_update(blendshape_controller, result, time->dt);

      blendshape_controller_apply(blendshape_controller);
    }
  }

  // Update and render 3D model if assets are loaded
  if (g_lipsync_state->assets_loaded) {
    // Update camera (simple controls could be added here)
    camera_update_uniforms(&g_lipsync_state->camera, memory->canvas.width,
                           memory->canvas.height);

    // Update lighting

    GameInput *input = &g_lipsync_state->input;
    f32 speed = 10.0f;
    if (input->up.is_pressed) {
      g_lipsync_state->directional_light[0] += speed * dt;
    }
    if (input->down.is_pressed) {
      g_lipsync_state->directional_light[0] -= speed * dt;
    }
    if (input->right.is_pressed) {
      g_lipsync_state->directional_light[1] += speed * dt;
    }
    if (input->left.is_pressed) {
      g_lipsync_state->directional_light[1] -= speed * dt;
    }

    g_lipsync_state->directional_lights.count = 1;
    DirectionalLight dir_light = {
        .direction = {0, 0, 1}, .color = {1, 1, 1}, .intensity = 1.0};

    quaternion dir_light_rot;
    quat_from_euler(g_lipsync_state->directional_light, dir_light_rot);
    glm_quat_rotatev(dir_light_rot, dir_light.direction, dir_light.direction);

    g_lipsync_state->directional_lights.lights[0] = dir_light;

    g_lipsync_state->point_lights.count = 1;
    f32 angle1 = time->now * 0.5f;
    f32 radius = 1.5f;
    f32 height = 0.25f;
    f32 intensity = 0.1f;

    g_lipsync_state->point_lights.lights[0] = (PointLight){
        .position = {radius * cosf(angle1), height, 3.0f * sinf(angle1)},
        .color = {1.0, 1.0, 1.0},
        .intensity = intensity,
        .innerRadius = 0.5f,
        .outerRadius = 3.0f};

    renderer_set_lights(&g_lipsync_state->directional_lights,
                        &g_lipsync_state->point_lights);

    // Update animation
    Character *entity = &g_lipsync_state->character;
    animation_update(&entity->animation_state, dt);
    animation_evaluate(&entity->animation_state,
                       entity->skinned_model.joint_matrices);

    // Render the model
    renderer_skm_draw(&ctx->temp_allocator, &entity->skinned_model,
                      entity->model_matrix);
  }
  input_end_frame(&g_lipsync_state->input);
}