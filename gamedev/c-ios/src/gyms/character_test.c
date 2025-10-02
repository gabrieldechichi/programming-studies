#include "animation.h"
#include "animation_system.h"
#include "assets.h"
#include "camera.h"
#include "cglm/affine.h"
#include "cglm/util.h"
#include "cglm/vec3.h"
#include "context.h"
#include "game.h"
#include "gameplay_lib.c"
#include "lib/array.h"
#include "lib/audio.h"
#include "lib/lipsync.h"
#include "lib/math.h"
#include "lib/memory.h"
#include "lib/profiler.h"
#include "lib/typedefs.h"
#include "platform/platform.h"
#include "renderer/renderer.h"
#include "stb/stb_image.h"

slice_define(AnimationAsset_Handle);
typedef Animation *AnimationPtr;
slice_define(AnimationPtr);

#define ANIMATIONS_CAP 64
#define MAX_COSTUMES 8

// Phoneme to blendshape mapping for lipsync
internal PhonemeBlendshapeDefinition phoneme_blendshape_definitions[] = {
    {"A", "ah"}, {"I", "ih"}, {"U", "uh"}, {"E", "eh"}, {"O", "oh"},
};

typedef struct {
  mat4 model_matrix;
  SkinnedModel skinned_model;
  AnimatedEntity animated;
  LipsyncBlendshapeController face_blendshapes;
  LipSyncContext face_lipsync;
} Character;

typedef struct {
  // memory
  ArenaAllocator permanent_arena;
  ArenaAllocator temporary_arena;
  GameContext ctx;

  AssetSystem asset_system;
  AudioState audio_system;
  GameInput input;

  // assets
  Model3DData_Handle model_asset_handle;
  AnimationAsset_Handle_Slice anim_asset_handles;
  MaterialAsset_Handle *material_asset_handles;
  u32 material_count;
  LipSyncProfile_Handle lipsync_profile_handle;
  WavFile_Handle wav_file_handle;
  Model3DData *model_data;
  AnimationPtr_Slice animations;
  AnimationPtr_Slice lower_body_animations_loaded;
  AnimationPtr_Slice upper_body_animations_loaded;
  AnimationPtr_Slice face_animations_loaded;
  Material_Slice materials;
  LipSyncProfile *lipsync_profile;
  WavFile *wav_file;
  b32 audio_started;

  u32 face_layer_index;

  // skybox material
  Texture_Handle skybox_texture_handle;
  Handle skybox_material_handle;
  b32 skybox_material_ready;

  // background quad
  Handle quad_shader_handle;
  Handle quad_material_handle;
  Handle quad_mesh_handle;
  b32 quad_ready;

  // costume data - supports multiple costumes
  u32 num_costumes;
  Model3DData_Handle costume_model_handles[MAX_COSTUMES];
  Model3DData *costume_model_datas[MAX_COSTUMES];
  MaterialAsset_Handle *costume_material_handles_array[MAX_COSTUMES];
  u32 costume_material_counts[MAX_COSTUMES];
  Material_Slice costume_materials_array[MAX_COSTUMES];
  SkinnedModel costume_skinned_models[MAX_COSTUMES];
  i32 *costume_to_tolan_joint_maps[MAX_COSTUMES];
  u32 costume_joint_counts[MAX_COSTUMES];
  b32 costume_map_created[MAX_COSTUMES];

  // 3D scene data
  DirectionalLightBlock directional_lights;
  PointLightsBlock point_lights;
  Camera camera;

  Character character;

  i32 neck_joint_idx;
  i32 left_eye_mesh_idx;
  i32 right_eye_mesh_idx;
  i32 left_eye_olive_bs_idx;
  i32 right_eye_olive_bs_idx;
} GymState;

global char *lower_body_animations[] = {
    "tolan/Tolan - Idle 02 - Loop.hasset",
};

global char *upper_body_animations[] = {
    "tolan/Tolan - Idle 03 Loop.hasset",
};

global char *face_animations[] = {};

global char *costume_paths[] = {
    "tolanCostumes/tolan_veeNeckShortSleeve.hasset",
    "tolanCostumes/ShortsSimpleGreyRed.hasset",
    "tolanCostumes/tolan_shoesBubblegum.hasset",
    "tolanCostumes/tolan_scarfSpikey.hasset",
};

global char *texture_preload_paths[] = {
    "tolan/Tolan_tex.png",
    "textures/transparent_pixel.png",
    "backgrounds/tolan_bg_2.png",
    "dogphoto.png",
    "tolanCostumes/Clothes_01.png",
    "tolanCostumes/tolan_cosmeticPalette_GreyRed.png",
    "textures/white_pixel.png",
    "tolanCostumes/tolan_veeNeckShortSleeve_tshirtAlpha_flower.png",
};

global GameContext *g_game_ctx;

extern GameContext *get_global_ctx() { return g_game_ctx; }

void gym_init(GameMemory *memory) {

  PROFILE_BEGIN("game: gym init");
  GymState *gym_state = cast(GymState *) memory->permanent_memory;
  g_game_ctx = &gym_state->ctx;
  GameContext *ctx = &gym_state->ctx;

  gym_state->permanent_arena =
      arena_from_buffer(memory->permanent_memory + sizeof(GymState),
                        memory->pernament_memory_size - sizeof(GymState));
  gym_state->temporary_arena = arena_from_buffer((u8 *)memory->temporary_memory,
                                                 memory->temporary_memory_size);

  gym_state->input = input_init();
  // Create GameContext with allocators
  gym_state->ctx.allocator = make_arena_allocator(&gym_state->permanent_arena);
  gym_state->ctx.temp_allocator =
      make_arena_allocator(&gym_state->temporary_arena);

  gym_state->neck_joint_idx = -1;
  gym_state->left_eye_mesh_idx = -1;
  gym_state->right_eye_mesh_idx = -1;
  gym_state->left_eye_olive_bs_idx = -1;
  gym_state->right_eye_olive_bs_idx = -1;

  gym_state->input = input_init();
  gym_state->audio_system = audio_init(ctx);
  gym_state->asset_system = asset_system_init(&ctx->allocator, 512);
  gym_state->audio_started = false;

  // Preload all textures
  u32 num_textures =
      sizeof(texture_preload_paths) / sizeof(texture_preload_paths[0]);
  for (u32 i = 0; i < num_textures; i++) {
    asset_request(Texture, &gym_state->asset_system, ctx,
                  texture_preload_paths[i]);
  }

  gym_state->model_asset_handle = asset_request(
      Model3DData, &gym_state->asset_system, ctx, "tolan/tolan.hasset");

  // Request lipsync profile
  gym_state->lipsync_profile_handle = asset_request(
      LipSyncProfile, &gym_state->asset_system, ctx, "lipsync_profile.passet");

  // Request a WAV file for lipsync testing (you may need to change this path)
  gym_state->wav_file_handle = asset_request(
      WavFile, &gym_state->asset_system, ctx, "hannahdogaudio.wav");

  // Initialize costume data
  gym_state->num_costumes = sizeof(costume_paths) / sizeof(costume_paths[0]);

  // Request all costume models
  for (u32 i = 0; i < gym_state->num_costumes; i++) {
    gym_state->costume_model_handles[i] = asset_request(
        Model3DData, &gym_state->asset_system, ctx, costume_paths[i]);
    gym_state->costume_model_datas[i] = NULL;
    gym_state->costume_material_handles_array[i] = NULL;
    gym_state->costume_material_counts[i] = 0;
    gym_state->costume_materials_array[i] = (Material_Slice){0};
    gym_state->costume_skinned_models[i] = (SkinnedModel){0};
    gym_state->costume_to_tolan_joint_maps[i] = NULL;
    gym_state->costume_joint_counts[i] = 0;
    gym_state->costume_map_created[i] = false;
  }

  // Request multiple animations
  gym_state->anim_asset_handles =
      slice_new_ALLOC(&ctx->allocator, AnimationAsset_Handle, ANIMATIONS_CAP);

  // Request lower body animations
  u32 num_lower_body =
      sizeof(lower_body_animations) / sizeof(lower_body_animations[0]);
  for (u32 i = 0; i < num_lower_body; i++) {
    slice_append(gym_state->anim_asset_handles,
                 asset_request(AnimationAsset, &gym_state->asset_system, ctx,
                               lower_body_animations[i]));
  }

  // Request upper body animations
  u32 num_upper_body =
      sizeof(upper_body_animations) / sizeof(upper_body_animations[0]);
  for (u32 i = 0; i < num_upper_body; i++) {
    slice_append(gym_state->anim_asset_handles,
                 asset_request(AnimationAsset, &gym_state->asset_system, ctx,
                               upper_body_animations[i]));
  }

  // Request face animations
  u32 num_face_animations =
      sizeof(face_animations) / sizeof(face_animations[0]);
  for (u32 i = 0; i < num_face_animations; i++) {
    slice_append(gym_state->anim_asset_handles,
                 asset_request(AnimationAsset, &gym_state->asset_system, ctx,
                               face_animations[i]));
  }

  // Initialize animations slices
  gym_state->animations =
      slice_new_ALLOC(&ctx->allocator, AnimationPtr, ANIMATIONS_CAP);
  gym_state->lower_body_animations_loaded =
      slice_new_ALLOC(&ctx->allocator, AnimationPtr, num_lower_body);
  gym_state->upper_body_animations_loaded =
      slice_new_ALLOC(&ctx->allocator, AnimationPtr, num_upper_body);
  gym_state->face_animations_loaded =
      slice_new_ALLOC(&ctx->allocator, AnimationPtr, num_face_animations);

  // Request background texture
  gym_state->skybox_texture_handle = asset_request(
      Texture, &gym_state->asset_system, ctx, "dogphoto.png");
  gym_state->quad_ready = false;

  // Load simple_quad shader
  LoadShaderParams shader_params = {.shader_name = "simple_quad"};
  gym_state->quad_shader_handle = load_shader(shader_params);

  // Create quad mesh
  {
    // Quad vertices: position (3 floats) + uv (2 floats)
    float quad_vertices[] = {
        // Position        UV
        -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, // Bottom-left
        1.0f,  -1.0f, 0.0f, 1.0f, 1.0f, // Bottom-right
        1.0f,  1.0f,  0.0f, 1.0f, 0.0f, // Top-right
        -1.0f, 1.0f,  0.0f, 0.0f, 0.0f  // Top-left
    };

    // Indices for 2 triangles (CCW winding)
    u32 quad_indices[] = {
        0, 2, 1, // First triangle (reversed for CCW)
        0, 3, 2  // Second triangle (reversed for CCW)
    };

    SubMeshData quad_mesh_data = {.vertex_buffer = (u8 *)quad_vertices,
                                  .len_vertex_buffer =
                                      sizeof(quad_vertices) / sizeof(float),
                                  .indices = quad_indices,
                                  .len_indices = 6,
                                  .len_vertices = 4,
                                  .len_blendshapes = 0,
                                  .blendshape_deltas = NULL};

    gym_state->quad_mesh_handle =
        renderer_create_submesh(&quad_mesh_data, false);
  }

  gym_state->camera = (Camera){
      .pos = {0.0, 0.7, 9.35},
      .pitch = 0.0f,
      .fov = 14.0f,
  };
  quat_identity(gym_state->camera.rot);

  PROFILE_END();
}

void handle_loading(GymState *gym_state, AssetSystem *asset_system) {
  GameContext *ctx = &gym_state->ctx;

  // Load lipsync profile first since other assets depend on it
  if (!gym_state->lipsync_profile &&
      asset_is_ready(asset_system, gym_state->lipsync_profile_handle)) {
    gym_state->lipsync_profile =
        asset_get_data(LipSyncProfile, &gym_state->asset_system,
                       gym_state->lipsync_profile_handle);
    LOG_INFO("Lipsync profile loaded");
  }

  // Load WAV file for audio playback
  if (!gym_state->wav_file &&
      asset_is_ready(asset_system, gym_state->wav_file_handle)) {
    gym_state->wav_file =
        asset_get_data(WavFile, &gym_state->asset_system,
                       gym_state->wav_file_handle);
    if (gym_state->wav_file) {
      LOG_INFO("WAV file loaded: %d Hz, %d channels, %d samples",
               FMT_INT(gym_state->wav_file->format.sample_rate),
               FMT_INT(gym_state->wav_file->format.channels),
               FMT_INT(gym_state->wav_file->total_samples));
    }
  }

  // Load model data first
  //
  PROFILE_BEGIN("game: loading model");
  if (!gym_state->model_data &&
      asset_is_ready(asset_system, gym_state->model_asset_handle)) {
    gym_state->model_data = asset_get_data(
        Model3DData, &gym_state->asset_system, gym_state->model_asset_handle);

    // Count total submeshes across all meshes
    u32 total_submeshes = 0;
    for (u32 i = 0; i < gym_state->model_data->num_meshes; i++) {
      MeshData *mesh_data = &gym_state->model_data->meshes[i];
      total_submeshes += mesh_data->submeshes.len;
    }

    // Request materials based on submesh material_path
    gym_state->material_count = total_submeshes;
    gym_state->material_asset_handles =
        ALLOC_ARRAY(&ctx->allocator, MaterialAsset_Handle, total_submeshes);

    u32 material_idx = 0;
    for (u32 i = 0; i < gym_state->model_data->num_meshes; i++) {
      MeshData *mesh_data = &gym_state->model_data->meshes[i];

      for (u32 j = 0; j < mesh_data->submeshes.len; j++) {
        SubMeshData *submesh_data = &mesh_data->submeshes.items[j];

        if (submesh_data->material_path.len > 0 &&
            submesh_data->material_path.value != NULL) {
          // Request material asset - path is already absolute
          gym_state->material_asset_handles[material_idx] =
              asset_request(MaterialAsset, &gym_state->asset_system, ctx,
                            submesh_data->material_path.value);
          LOG_INFO("Requesting material % for mesh % submesh %",
                   FMT_STR(submesh_data->material_path.value), FMT_UINT(i),
                   FMT_UINT(j));
        } else {
          // No material path - will use white material
          gym_state->material_asset_handles[material_idx] =
              (MaterialAsset_Handle){0};
          LOG_INFO(
              "No material path for mesh % submesh %, will use white material",
              FMT_UINT(i), FMT_UINT(j));
        }
        material_idx++;
      }
    }

    // init animated entity
    AnimatedEntity *animated_entity = &gym_state->character.animated;
    animated_entity_init(animated_entity, gym_state->model_data,
                         &ctx->allocator);

    // Create skeleton masks for upper and lower body layers
    String lower_body_joints[] = {
        STR_FROM_CSTR("Hips"),           STR_FROM_CSTR("Left leg"),
        STR_FROM_CSTR("Left knee"),      STR_FROM_CSTR("Left ankle"),
        STR_FROM_CSTR("Left toe"),       STR_FROM_CSTR("Right leg"),
        STR_FROM_CSTR("Right knee"),     STR_FROM_CSTR("Right ankle"),
        STR_FROM_CSTR("Right toe"),      STR_FROM_CSTR("DynamicSkirtL"),
        STR_FROM_CSTR("DynamicSkirtL1"), STR_FROM_CSTR("DynamicSkirtR"),
        STR_FROM_CSTR("DynamicSkirtR1")};
    u32 num_lower_joints =
        sizeof(lower_body_joints) / sizeof(lower_body_joints[0]);

    String upper_body_joints[] = {STR_FROM_CSTR("Spine"),
                                  STR_FROM_CSTR("Chest"),
                                  STR_FROM_CSTR("Neck"),
                                  STR_FROM_CSTR("Head"),
                                  STR_FROM_CSTR("LeftEye"),
                                  STR_FROM_CSTR("RightEye"),
                                  STR_FROM_CSTR("Left shoulder"),
                                  STR_FROM_CSTR("Left arm"),
                                  STR_FROM_CSTR("Left elbow"),
                                  STR_FROM_CSTR("Left Hand"),
                                  STR_FROM_CSTR("Right shoulder"),
                                  STR_FROM_CSTR("Right arm"),
                                  STR_FROM_CSTR("Right elbow"),
                                  STR_FROM_CSTR("Right hand"),
                                  STR_FROM_CSTR("IndexFinger1_L"),
                                  STR_FROM_CSTR("IndexFinger2_L"),
                                  STR_FROM_CSTR("IndexFinger3_L"),
                                  STR_FROM_CSTR("MiddleFinger1_L"),
                                  STR_FROM_CSTR("MiddleFinger2_L"),
                                  STR_FROM_CSTR("MiddleFinger3_L"),
                                  STR_FROM_CSTR("RingFinger1_L"),
                                  STR_FROM_CSTR("RingFinger2_L"),
                                  STR_FROM_CSTR("RingFinger3_L"),
                                  STR_FROM_CSTR("Thumb0_L"),
                                  STR_FROM_CSTR("Thumb1_L"),
                                  STR_FROM_CSTR("Thumb2_L"),
                                  STR_FROM_CSTR("LittleFinger1_L"),
                                  STR_FROM_CSTR("LittleFinger2_L"),
                                  STR_FROM_CSTR("LittleFinger3_L"),
                                  STR_FROM_CSTR("LittleFinger1_R"),
                                  STR_FROM_CSTR("LittleFinger2_R"),
                                  STR_FROM_CSTR("LittleFinger3_R"),
                                  STR_FROM_CSTR("MiddleFinger1_R"),
                                  STR_FROM_CSTR("MiddleFinger2_R"),
                                  STR_FROM_CSTR("MiddleFinger3_R"),
                                  STR_FROM_CSTR("Thumb0_R"),
                                  STR_FROM_CSTR("Thumb1_R"),
                                  STR_FROM_CSTR("Thumb2_R"),
                                  STR_FROM_CSTR("IndexFinger1_R"),
                                  STR_FROM_CSTR("IndexFinger2_R"),
                                  STR_FROM_CSTR("IndexFinger3_R"),
                                  STR_FROM_CSTR("RingFinger1_R"),
                                  STR_FROM_CSTR("RingFinger2_R"),
                                  STR_FROM_CSTR("RingFinger3_R"),
                                  STR_FROM_CSTR("DynamicHairROOT"),
                                  STR_FROM_CSTR("HairBone00"),
                                  STR_FROM_CSTR("HairBone01"),
                                  STR_FROM_CSTR("HairBone02"),
                                  STR_FROM_CSTR("HairBone03"),
                                  STR_FROM_CSTR("HairBone04"),
                                  STR_FROM_CSTR("HairBone05"),
                                  STR_FROM_CSTR("HairBone06"),
                                  STR_FROM_CSTR("HairBone07"),
                                  STR_FROM_CSTR("HairBone08"),
                                  STR_FROM_CSTR("HairBone09"),
                                  STR_FROM_CSTR("HairBone10"),
                                  STR_FROM_CSTR("HairBone11")};
    u32 num_upper_joints =
        sizeof(upper_body_joints) / sizeof(upper_body_joints[0]);

    SkeletonMask lower_body_mask = skeleton_mask_create_from_joint_names(
        &ctx->allocator, gym_state->model_data, lower_body_joints,
        num_lower_joints);
    SkeletonMask upper_body_mask = skeleton_mask_create_from_joint_names(
        &ctx->allocator, gym_state->model_data, upper_body_joints,
        num_upper_joints);
    // SkeletonMask lower_body_mask = skeleton_mask_create_from_joint_names(
    //     &ctx->allocator, gym_state->model_data, lower_body_joints,
    //     num_lower_joints);
    // SkeletonMask upper_body_mask = skeleton_mask_create_all(
    //     &ctx->allocator, gym_state->model_data->len_joints);

    // Create animation layers (default layer already exists at index 0)
    // Remove default layer since we want separate upper/lower layers
    animated_entity_remove_layer(animated_entity, 0);

    // Create face layer with no joints (blendshapes only)
    SkeletonMask face_mask =
        skeleton_mask_create_from_joints(&ctx->allocator, NULL, 0);
    gym_state->face_layer_index =
        animated_entity_add_layer(animated_entity, STR_FROM_CSTR("Face"),
                                  face_mask, 1.0f, &ctx->allocator);

    LOG_INFO("VRoid Male Model loaded with % meshes, % "
             "total submeshes",
             FMT_UINT(gym_state->model_data->num_meshes),
             FMT_UINT(total_submeshes));
  }
  PROFILE_END();

  PROFILE_BEGIN("game: create skinned mesh");
  // Wait for all materials to load, then create SkinnedModel
  if (gym_state->model_data &&
      !gym_state->character.skinned_model.meshes.items) {
    bool all_materials_ready = true;

    for (u32 i = 0; i < gym_state->material_count; i++) {
      if (gym_state->material_asset_handles[i].idx != 0) {
        if (!asset_is_ready(asset_system,
                            gym_state->material_asset_handles[i])) {
          all_materials_ready = false;
          break;
        }
      }
    }

    if (all_materials_ready && gym_state->lipsync_profile) {
      // First, deduplicate material assets
      typedef struct {
        MaterialAsset_Handle handle;
        Material *material;
      } UniqueMaterial;

      UniqueMaterial *unique_materials = ALLOC_ARRAY(
          &ctx->temp_allocator, UniqueMaterial, gym_state->material_count);
      u32 unique_count = 0;

      // Create materials array that maps submesh index to material
      gym_state->materials =
          slice_new_ALLOC(&ctx->allocator, Material, gym_state->material_count);

      PROFILE_BEGIN("game: create materials");
      for (u32 i = 0; i < gym_state->material_count; i++) {
        if (gym_state->material_asset_handles[i].idx != 0) {
          // Check if we already created this material
          Material *existing_material = NULL;
          for (u32 j = 0; j < unique_count; j++) {
            if (handle_equals(
                    cast_handle(Handle, unique_materials[j].handle),
                    cast_handle(Handle,
                                gym_state->material_asset_handles[i]))) {
              existing_material = unique_materials[j].material;
              break;
            }
          }

          if (existing_material) {
            // Reuse existing material
            slice_append(gym_state->materials, *existing_material);
            LOG_INFO("Reusing material for submesh %", FMT_UINT(i));
          } else {

            PROFILE_BEGIN("game: create single material");
            // Create new material
            MaterialAsset *material_asset =
                asset_get_data(MaterialAsset, &gym_state->asset_system,
                               gym_state->material_asset_handles[i]);
            assert(material_asset);
            PROFILE_BEGIN("game: material from asset");
            Material *material = material_from_asset(
                material_asset, &gym_state->asset_system, ctx);
            PROFILE_END();
            slice_append(gym_state->materials, *material);

            // Add to unique materials list
            unique_materials[unique_count].handle =
                gym_state->material_asset_handles[i];
            unique_materials[unique_count].material =
                &gym_state->materials.items[gym_state->materials.len - 1];
            unique_count++;

            LOG_INFO("Created unique material % (handle idx=%) for submesh %",
                     FMT_STR(material_asset->name.value),
                     FMT_UINT(gym_state->material_asset_handles[i].idx),
                     FMT_UINT(i));
            PROFILE_END();
          }
        } else {
          // Use default white material for submeshes without materials
          // This would need a default material implementation
          LOG_WARN("No material for submesh %, skipping", FMT_UINT(i));
          // For now, just add a placeholder - this might need proper default
          // material handling
          Material default_material = {0};
          slice_append(gym_state->materials, default_material);
        }
      }
      PROFILE_END();

      LOG_INFO(
          "Material deduplication: % unique materials from % total submeshes",
          FMT_UINT(unique_count), FMT_UINT(gym_state->material_count));

      // Create SkinnedModel with loaded materials
      Character *entity = &gym_state->character;

      quaternion temp_rot;
      quat_from_euler((vec3){glm_rad(0), 0, 0}, temp_rot);
      mat_trs((vec3){0, 0, 0}, temp_rot, (vec3){0.01, 0.01, 0.01},
              entity->model_matrix);

      PROFILE_BEGIN("game: skmodel from asset");
      entity->skinned_model =
          skmodel_from_asset(ctx, gym_state->model_data, gym_state->materials);
      PROFILE_END();

      // Initialize lipsync components
      entity->face_lipsync = lipsync_init(
          &ctx->allocator, gym_state->audio_system.output_sample_rate,
          gym_state->lipsync_profile);

      // Find the face mesh for blendshape control
      const char *face_name = "head_geo";
      i32 face_idx = arr_find_index_pred_raw(
          gym_state->model_data->meshes, gym_state->model_data->num_meshes,
          str_equal(_item.mesh_name.value, face_name));

      if (face_idx >= 0) {
        SkinnedMesh *face_mesh = &entity->skinned_model.meshes.items[face_idx];
        entity->face_blendshapes = blendshape_controller_init(
            &ctx->allocator, gym_state->lipsync_profile,
            phoneme_blendshape_definitions,
            sizeof(phoneme_blendshape_definitions) / sizeof(phoneme_blendshape_definitions[0]),
            face_mesh);
        LOG_INFO("Initialized lipsync for face mesh at index %", FMT_INT(face_idx));
      } else {
        LOG_WARN("Could not find face mesh '%' for lipsync", FMT_STR(face_name));
      }

      LOG_INFO("SkinnedModel created with % materials",
               FMT_UINT(gym_state->materials.len));
    }
  }
  PROFILE_END();

  // Load animations as they become ready
  PROFILE_BEGIN("game: load animations");
  if (gym_state->model_data && gym_state->materials.len > 0 &&
      gym_state->animations.len < gym_state->anim_asset_handles.len) {

    u32 num_lower_body =
        sizeof(lower_body_animations) / sizeof(lower_body_animations[0]);
    u32 num_upper_body =
        sizeof(upper_body_animations) / sizeof(upper_body_animations[0]);
    u32 num_face_animations =
        sizeof(face_animations) / sizeof(face_animations[0]);

    for (u32 i = gym_state->animations.len;
         i < gym_state->anim_asset_handles.len; i++) {
      AnimationAsset_Handle handle = gym_state->anim_asset_handles.items[i];
      if (asset_is_ready(asset_system, handle)) {
        AnimationAsset *anim_asset =
            asset_get_data(AnimationAsset, &gym_state->asset_system, handle);
        Animation *anim = animation_from_asset(
            anim_asset, gym_state->model_data, &ctx->allocator);
        slice_append(gym_state->animations, anim);

        // Also add to appropriate body animation array
        if (i < num_lower_body) {
          // Lower body animation
          slice_append(gym_state->lower_body_animations_loaded, anim);
        } else if (i < num_lower_body + num_upper_body) {
          // Upper body animation
          slice_append(gym_state->upper_body_animations_loaded, anim);
        } else if (i < num_lower_body + num_upper_body + num_face_animations) {
          // Face animation
          slice_append(gym_state->face_animations_loaded, anim);
        }
      } else {
        break; // Stop checking once we hit an unready asset
      }
    }

    gym_state->neck_joint_idx = arr_find_index_pred_raw(
        gym_state->model_data->joint_names, gym_state->model_data->len_joints,
        str_equal(_item.value, "Neck"));
    debug_assert(gym_state->neck_joint_idx >= 0);

    if (gym_state->neck_joint_idx >= 0) {
      LOG_INFO("Found neck joint at index %",
               FMT_UINT(gym_state->neck_joint_idx));
    } else {
      LOG_WARN("Neck joint not found in model");
    }

    gym_state->left_eye_mesh_idx = arr_find_index_pred_raw(
        gym_state->model_data->meshes, gym_state->model_data->num_meshes,
        str_equal(_item.mesh_name.value, "l_eye_geo"));
    debug_assert(gym_state->left_eye_mesh_idx >= 0);

    gym_state->right_eye_mesh_idx = arr_find_index_pred_raw(
        gym_state->model_data->meshes, gym_state->model_data->num_meshes,
        str_equal(_item.mesh_name.value, "r_eye_geo"));
    debug_assert(gym_state->right_eye_mesh_idx >= 0);

    if (gym_state->left_eye_mesh_idx >= 0) {
      MeshData *left_eye_mesh =
          &gym_state->model_data->meshes[gym_state->left_eye_mesh_idx];
      gym_state->left_eye_olive_bs_idx = arr_find_index_pred_raw(
          left_eye_mesh->blendshape_names.items,
          left_eye_mesh->blendshape_names.len, str_equal(_item.value, "olive"));

      debug_assert(gym_state->left_eye_olive_bs_idx >= 0);

      LOG_INFO("Found left eye mesh at index %, Olive blendshape at index %",
               FMT_UINT(gym_state->left_eye_mesh_idx),
               FMT_INT(gym_state->left_eye_olive_bs_idx));
    } else {
      LOG_WARN("l_eye_geo mesh not found in model");
    }

    if (gym_state->right_eye_mesh_idx >= 0) {
      MeshData *right_eye_mesh =
          &gym_state->model_data->meshes[gym_state->right_eye_mesh_idx];
      gym_state->right_eye_olive_bs_idx =
          arr_find_index_pred_raw(right_eye_mesh->blendshape_names.items,
                                  right_eye_mesh->blendshape_names.len,
                                  str_equal(_item.value, "olive"));
      debug_assert(gym_state->right_eye_olive_bs_idx >= 0);

      LOG_INFO("Found right eye mesh at index %, Olive blendshape at index %",
               FMT_UINT(gym_state->right_eye_mesh_idx),
               FMT_INT(gym_state->right_eye_olive_bs_idx));
    } else {
      LOG_WARN("r_eye_geo mesh not found in model");
    }
  }
  PROFILE_END();

  PROFILE_BEGIN("game: load quad material");
  // Setup quad material when texture and shader are ready
  if (!gym_state->quad_ready &&
      asset_is_ready(asset_system, gym_state->skybox_texture_handle) &&
      handle_is_valid(gym_state->quad_shader_handle)) {

    // Create material properties using the texture handle directly
    MaterialProperty properties[] = {
        {.name = STR_FROM_CSTR("uTexture"),
         .type = MAT_PROP_TEXTURE,
         .value.texture = gym_state->skybox_texture_handle}};

    // Load the material
    gym_state->quad_material_handle =
        load_material(gym_state->quad_shader_handle, properties,
                      1,    // property count
                      false // not transparent
        );

    LOG_INFO("Created quad material with shader handle idx=%, gen=%",
             FMT_UINT(gym_state->quad_material_handle.idx),
             FMT_UINT(gym_state->quad_material_handle.gen));

    gym_state->quad_ready = true;
    LOG_INFO("Skybox material set successfully");
  }
  PROFILE_END();

  PROFILE_BEGIN("game: load costumes");
  // Load costume model data for all costumes
  for (u32 costume_idx = 0; costume_idx < gym_state->num_costumes;
       costume_idx++) {
    if (!gym_state->costume_model_datas[costume_idx] &&
        asset_is_ready(asset_system,
                       gym_state->costume_model_handles[costume_idx])) {
      gym_state->costume_model_datas[costume_idx] =
          asset_get_data(Model3DData, &gym_state->asset_system,
                         gym_state->costume_model_handles[costume_idx]);

      // Count total submeshes for this costume
      u32 total_costume_submeshes = 0;
      for (u32 i = 0;
           i < gym_state->costume_model_datas[costume_idx]->num_meshes; i++) {
        MeshData *mesh_data =
            &gym_state->costume_model_datas[costume_idx]->meshes[i];
        total_costume_submeshes += mesh_data->submeshes.len;
      }

      // Request materials for this costume
      gym_state->costume_material_counts[costume_idx] = total_costume_submeshes;
      gym_state->costume_material_handles_array[costume_idx] = ALLOC_ARRAY(
          &ctx->allocator, MaterialAsset_Handle, total_costume_submeshes);

      u32 material_idx = 0;
      for (u32 i = 0;
           i < gym_state->costume_model_datas[costume_idx]->num_meshes; i++) {
        MeshData *mesh_data =
            &gym_state->costume_model_datas[costume_idx]->meshes[i];

        for (u32 j = 0; j < mesh_data->submeshes.len; j++) {
          SubMeshData *submesh_data = &mesh_data->submeshes.items[j];

          if (submesh_data->material_path.len > 0 &&
              submesh_data->material_path.value != NULL) {
            gym_state
                ->costume_material_handles_array[costume_idx][material_idx] =
                asset_request(MaterialAsset, &gym_state->asset_system, ctx,
                              submesh_data->material_path.value);
            LOG_INFO("Requesting costume % material % for mesh % submesh %",
                     FMT_UINT(costume_idx),
                     FMT_STR(submesh_data->material_path.value), FMT_UINT(i),
                     FMT_UINT(j));
          } else {
            gym_state
                ->costume_material_handles_array[costume_idx][material_idx] =
                (MaterialAsset_Handle){0};
            LOG_INFO("No material path for costume % mesh % submesh %",
                     FMT_UINT(costume_idx), FMT_UINT(i), FMT_UINT(j));
          }
          material_idx++;
        }
      }

      LOG_INFO(
          "Costume % model loaded with % meshes, % total submeshes",
          FMT_UINT(costume_idx),
          FMT_UINT(gym_state->costume_model_datas[costume_idx]->num_meshes),
          FMT_UINT(total_costume_submeshes));
    }
  }
  PROFILE_END();

  PROFILE_BEGIN("game: create costumes");
  // Create costume SkinnedModels when materials are ready
  for (u32 costume_idx = 0; costume_idx < gym_state->num_costumes;
       costume_idx++) {
    if (gym_state->costume_model_datas[costume_idx] &&
        !gym_state->costume_skinned_models[costume_idx].meshes.items) {
      bool all_costume_materials_ready = true;

      for (u32 i = 0; i < gym_state->costume_material_counts[costume_idx];
           i++) {
        if (gym_state->costume_material_handles_array[costume_idx][i].idx !=
            0) {
          if (!asset_is_ready(
                  asset_system,
                  gym_state->costume_material_handles_array[costume_idx][i])) {
            all_costume_materials_ready = false;
            break;
          }
        }
      }

      if (all_costume_materials_ready) {
        // First, deduplicate material assets for this costume
        typedef struct {
          MaterialAsset_Handle handle;
          Material *material;
        } UniqueMaterial;

        UniqueMaterial *unique_materials =
            ALLOC_ARRAY(&ctx->temp_allocator, UniqueMaterial,
                        gym_state->costume_material_counts[costume_idx]);
        u32 unique_count = 0;

        // Create materials array for this costume
        gym_state->costume_materials_array[costume_idx] =
            slice_new_ALLOC(&ctx->allocator, Material,
                            gym_state->costume_material_counts[costume_idx]);

        for (u32 i = 0; i < gym_state->costume_material_counts[costume_idx];
             i++) {
          if (gym_state->costume_material_handles_array[costume_idx][i].idx !=
              0) {
            // Check if we already created this material
            Material *existing_material = NULL;
            for (u32 j = 0; j < unique_count; j++) {
              if (handle_equals(
                      cast_handle(Handle, unique_materials[j].handle),
                      cast_handle(
                          Handle,
                          gym_state->costume_material_handles_array[costume_idx]
                                                                   [i]))) {
                existing_material = unique_materials[j].material;
                break;
              }
            }

            if (existing_material) {
              // Reuse existing material
              slice_append(gym_state->costume_materials_array[costume_idx],
                           *existing_material);
              LOG_INFO("Costume % - Reusing material for submesh %",
                       FMT_UINT(costume_idx), FMT_UINT(i));
            } else {
              // Create new material
              MaterialAsset *material_asset = asset_get_data(
                  MaterialAsset, &gym_state->asset_system,
                  gym_state->costume_material_handles_array[costume_idx][i]);
              assert(material_asset);
              Material *material = material_from_asset(
                  material_asset, &gym_state->asset_system, ctx);
              slice_append(gym_state->costume_materials_array[costume_idx],
                           *material);

              // Add to unique materials list
              unique_materials[unique_count].handle =
                  gym_state->costume_material_handles_array[costume_idx][i];
              unique_materials[unique_count].material =
                  &gym_state->costume_materials_array[costume_idx].items
                       [gym_state->costume_materials_array[costume_idx].len -
                        1];
              unique_count++;

              LOG_INFO(
                  "Costume % - Created unique material % (handle idx=%) for "
                  "submesh %",
                  FMT_UINT(costume_idx), FMT_STR(material_asset->name.value),
                  FMT_UINT(
                      gym_state->costume_material_handles_array[costume_idx][i]
                          .idx),
                  FMT_UINT(i));
            }
          } else {
            LOG_WARN("No material for costume % submesh %, using default",
                     FMT_UINT(costume_idx), FMT_UINT(i));
            Material default_material = {0};
            slice_append(gym_state->costume_materials_array[costume_idx],
                         default_material);
          }
        }

        LOG_INFO("Costume % material deduplication: % unique materials from % "
                 "total submeshes",
                 FMT_UINT(costume_idx), FMT_UINT(unique_count),
                 FMT_UINT(gym_state->costume_material_counts[costume_idx]));

        // Create costume SkinnedModel
        gym_state->costume_skinned_models[costume_idx] =
            skmodel_from_asset(ctx, gym_state->costume_model_datas[costume_idx],
                               gym_state->costume_materials_array[costume_idx]);

        LOG_INFO("Costume % SkinnedModel created with % materials",
                 FMT_UINT(costume_idx),
                 FMT_UINT(gym_state->costume_materials_array[costume_idx].len));

        // Create joint mapping between this costume and Tolan
        if (gym_state->model_data &&
            gym_state->costume_model_datas[costume_idx] &&
            !gym_state->costume_map_created[costume_idx]) {
          gym_state->costume_joint_counts[costume_idx] =
              gym_state->costume_model_datas[costume_idx]->len_joints;
          gym_state->costume_to_tolan_joint_maps[costume_idx] =
              ALLOC_ARRAY(&ctx->allocator, i32,
                          gym_state->costume_joint_counts[costume_idx]);

          u32 mapped_count = 0;
          u32 unmapped_count = 0;

          // For each costume joint, find matching Tolan joint by name
          for (u32 joint_idx = 0;
               joint_idx < gym_state->costume_joint_counts[costume_idx];
               joint_idx++) {
            String costume_joint_name =
                gym_state->costume_model_datas[costume_idx]
                    ->joint_names[joint_idx];
            i32 tolan_idx = -1;

            if (joint_idx == 0) {
              costume_joint_name = (String){.value = "geo", .len = 3};
            }

            // Search for matching joint in Tolan model
            for (u32 tolan_joint_idx = 0;
                 tolan_joint_idx < gym_state->model_data->len_joints;
                 tolan_joint_idx++) {
              if (str_equal(
                      gym_state->model_data->joint_names[tolan_joint_idx].value,
                      costume_joint_name.value)) {
                tolan_idx = tolan_joint_idx;
                break;
              }
            }

            gym_state->costume_to_tolan_joint_maps[costume_idx][joint_idx] =
                tolan_idx;

            if (tolan_idx >= 0) {
              mapped_count++;
              LOG_INFO("Costume % - Mapped joint % (%) to Tolan joint %",
                       FMT_UINT(costume_idx), FMT_STR(costume_joint_name.value),
                       FMT_UINT(joint_idx), FMT_UINT(tolan_idx));
            } else {
              unmapped_count++;
              LOG_WARN("Costume % - No match for joint % (%)",
                       FMT_UINT(costume_idx), FMT_STR(costume_joint_name.value),
                       FMT_UINT(joint_idx));
            }
          }

          gym_state->costume_map_created[costume_idx] = true;
          LOG_INFO(
              "Costume % joint mapping created: % mapped, % unmapped (total %)",
              FMT_UINT(costume_idx), FMT_UINT(mapped_count),
              FMT_UINT(unmapped_count),
              FMT_UINT(gym_state->costume_joint_counts[costume_idx]));
        }
      }
    }
  }
  PROFILE_END();
}

void gym_update_and_render(GameMemory *memory) {
  GymState *gym_state = cast(GymState *) memory->permanent_memory;
  GameContext *ctx = &gym_state->ctx;
  GameTime *time = &memory->time;

  f32 dt = time->dt;

  AudioState *audio_system = &gym_state->audio_system;
  AssetSystem *asset_system = &gym_state->asset_system;
  GameInput *input = &gym_state->input;

  PROFILE_BEGIN("game: handle loading");
  handle_loading(gym_state, asset_system);
  PROFILE_END();

  PROFILE_BEGIN("game: asset update");
  asset_system_update(asset_system, ctx);
  PROFILE_END();

  PROFILE_BEGIN("game: input update");
  input_update(input, &memory->input_events, memory->time.now);
  PROFILE_END();
  // camera_update(&gym_state->camera, input, dt);

  // Play audio once loaded
  if (!gym_state->audio_started && gym_state->wav_file) {
    AudioClip clip = {
        .wav_file = gym_state->wav_file,
        .loop = true,  // Loop for continuous testing
        .volume = 1.0
    };
    audio_play_clip(&gym_state->audio_system, clip);
    gym_state->audio_started = true;
    LOG_INFO("Started audio playback for lipsync");
  }

  PROFILE_BEGIN("game: audio update");
  audio_update(audio_system, ctx, dt);
  PROFILE_END();

  Character *entity = &gym_state->character;

  PROFILE_BEGIN("game: update camera");
  camera_update_uniforms(&gym_state->camera, memory->canvas.width,
                         memory->canvas.height);
  renderer_update_camera(&gym_state->camera.uniforms);
  PROFILE_END();

  PROFILE_BEGIN("game: light update");
  local_persist vec3 light_dir = {0.490610, 0.141831, 0.859758};
  // if (input->right.is_pressed) {
  //   light_dir[0] += 2 * dt;
  // }
  // if (input->left.is_pressed) {
  //   light_dir[0] -= 2 * dt;
  // }
  // if (input->space.is_pressed) {
  //
  //   if (input->up.is_pressed) {
  //     light_dir[2] += 2 * dt;
  //   }
  //   if (input->down.is_pressed) {
  //     light_dir[2] -= 2 * dt;
  //   }
  // } else {
  //
  //   if (input->up.is_pressed) {
  //     light_dir[1] += 2 * dt;
  //   }
  //   if (input->down.is_pressed) {
  //     light_dir[1] -= 2 * dt;
  //   }
  // }
  //
  // LOG_INFO("% % %", FMT_VEC3(light_dir));
  glm_normalize(light_dir);

  gym_state->directional_lights.count = 1;
  gym_state->directional_lights.lights[0] = (DirectionalLight){
      .direction = {light_dir[0], light_dir[1], light_dir[2]},
      .color = {1, 1, 1},
      .intensity = 1.0};

  renderer_set_lights(&gym_state->directional_lights);
  PROFILE_END();

  PROFILE_BEGIN("game: layered animations");
  // Handle layered animations
  if ((gym_state->lower_body_animations_loaded.len > 0 ||
       gym_state->upper_body_animations_loaded.len > 0 ||
       gym_state->face_animations_loaded.len > 0) &&
      gym_state->character.animated.layers.len >= 2) {
    AnimatedEntity *animated = &entity->animated;

    // Start lower body animation if none playing
    if (gym_state->lower_body_animations_loaded.len > 0) {
      AnimationLayer *lower_layer = &animated->layers.items[0];
      if (lower_layer->animation_states.len == 0) {
        // Start with "anya/Anya - Run Fwd 1.hasset"
        animated_entity_play_animation_on_layer(
            animated, 0, gym_state->lower_body_animations_loaded.items[0], 0.0f,
            1.0, true);
        LOG_INFO(
            "Started lower body animation: %",
            FMT_STR(
                gym_state->lower_body_animations_loaded.items[0]->name.value));
      }

      // Handle lower body animation cycling
      if (!lower_layer->current_transition.active &&
          lower_layer->animation_states.len > 0 &&
          gym_state->lower_body_animations_loaded.len > 1) {
        AnimationState *current_state =
            &lower_layer->animation_states
                 .items[lower_layer->current_animation_index];

        f32 transition_trigger_time = current_state->animation->length - 0.5f;
        if (current_state->time > transition_trigger_time) {
          // Find current animation in lower body list
          u32 current_index = 0;
          for (u32 i = 0; i < gym_state->lower_body_animations_loaded.len;
               i++) {
            if (gym_state->lower_body_animations_loaded.items[i] ==
                current_state->animation) {
              current_index = i;
              break;
            }
          }

          u32 next_index =
              (current_index + 1) % gym_state->lower_body_animations_loaded.len;
          animated_entity_play_animation_on_layer(
              animated, 0,
              gym_state->lower_body_animations_loaded.items[next_index], 0.3f,
              1.0, true);
          LOG_INFO(
              "Transitioning lower body to: %",
              FMT_STR(gym_state->lower_body_animations_loaded.items[next_index]
                          ->name.value));
        }
      }
    }

    // Start face animation if none playing
    if (gym_state->face_animations_loaded.len > 0) {
      AnimationLayer *face_layer =
          &animated->layers.items[gym_state->face_layer_index];
      if (face_layer->animation_states.len == 0) {
        LOG_INFO(
            "Here playing face animation %",
            FMT_STR(gym_state->face_animations_loaded.items[0]->name.value));
        // Start with first face animation - no looping for face animations
        animated_entity_play_animation_on_layer(
            animated, gym_state->face_layer_index,
            gym_state->face_animations_loaded.items[0], 0.0f, 1.0, false);
        LOG_INFO(
            "Started face animation: %",
            FMT_STR(gym_state->face_animations_loaded.items[0]->name.value));
      }

      local_persist f32 time_since_last_face_change = 4;

      // Handle face animation cycling
      if (!face_layer->current_transition.active &&
          face_layer->animation_states.len > 0 &&
          gym_state->face_animations_loaded.len > 1) {
        AnimationState *current_state =
            &face_layer->animation_states
                 .items[face_layer->current_animation_index];

        // For face animations, wait at least 1 second before switching
        if (time->now > time_since_last_face_change) {
          time_since_last_face_change = time->now + 3.0;
          // Find current animation in face list
          u32 current_index = 0;
          for (u32 i = 0; i < gym_state->face_animations_loaded.len; i++) {
            if (gym_state->face_animations_loaded.items[i] ==
                current_state->animation) {
              current_index = i;
              break;
            }
          }

          u32 next_index =
              (current_index + 1) % gym_state->face_animations_loaded.len;
          // Face animations don't loop
          animated_entity_play_animation_on_layer(
              animated, gym_state->face_layer_index,
              gym_state->face_animations_loaded.items[next_index], 0.3f, 1.0f,
              false);
          LOG_INFO("Transitioning face to: %",
                   FMT_STR(gym_state->face_animations_loaded.items[next_index]
                               ->name.value));
        }
      }
    }

    PROFILE_BEGIN("game: animate entity update");
    animated_entity_update(animated, dt);
    PROFILE_END();
    PROFILE_BEGIN("game: evaluate pose");
    animated_entity_evaluate_pose(animated, gym_state->model_data);
    PROFILE_END();

    PROFILE_BEGIN("game: tolan stuff");
    // tolan stuff
    {
      if (gym_state->left_eye_mesh_idx >= 0 &&
          gym_state->left_eye_olive_bs_idx >= 0 &&
          (u32)gym_state->left_eye_mesh_idx <
              animated->blendshape_results.len) {
        BlendshapeEvalResult *left_eye_result =
            &animated->blendshape_results.items[gym_state->left_eye_mesh_idx];
        if ((u32)gym_state->left_eye_olive_bs_idx <
            left_eye_result->blendshape_weights.len) {
          left_eye_result->blendshape_weights
              .items[gym_state->left_eye_olive_bs_idx] = 1.0f;
        }
      }

      if (gym_state->right_eye_mesh_idx >= 0 &&
          gym_state->right_eye_olive_bs_idx >= 0 &&
          (u32)gym_state->right_eye_mesh_idx <
              animated->blendshape_results.len) {
        BlendshapeEvalResult *left_eye_result =
            &animated->blendshape_results.items[gym_state->right_eye_mesh_idx];
        if ((u32)gym_state->right_eye_olive_bs_idx <
            left_eye_result->blendshape_weights.len) {
          left_eye_result->blendshape_weights
              .items[gym_state->right_eye_olive_bs_idx] = 1.0f;
        }
      }

      if (gym_state->neck_joint_idx >= 0 &&
          (u32)gym_state->neck_joint_idx < animated->final_pose.len) {
        JointTransform *joint =
            &animated->final_pose.items[gym_state->neck_joint_idx];

        joint->translation[1] = 5.5;
      }
    }
    PROFILE_END();

    PROFILE_BEGIN("game: animation apply pose");
    animated_entity_apply_pose(animated, gym_state->model_data,
                               &entity->skinned_model);
    PROFILE_END();

    // Process lipsync if audio is playing
    if (gym_state->audio_started && gym_state->wav_file) {
      // Feed audio to lipsync system
      LipSyncContext *lipsync = &entity->face_lipsync;
      lipsync_feed_audio(lipsync, ctx, audio_system->sample_buffer,
                         audio_system->sample_buffer_len,
                         audio_system->output_channels);

      // Process and get results
      if (lipsync_process(lipsync, ctx)) {
        LipSyncResult result = lipsync_get_result(lipsync);

        LipsyncBlendshapeController *blendshape_controller =
            &entity->face_blendshapes;

        blendshape_controller_update(blendshape_controller, result, dt);
        blendshape_controller_apply(blendshape_controller);
      }
    }
  }
  PROFILE_END();

  // Color clear_color = color_from_hex(0xebebeb);
  Color clear_color = color_from_hex(0x000000);
  renderer_clear(clear_color);

  PROFILE_BEGIN("game: draw bg");
  // Draw background quad if ready
  if (gym_state->quad_ready) {
    // Create model matrix for the quad (position it behind the character)

    mat4 quad_model;
    glm_mat4_identity(quad_model);

    // Render the quad using the non-skinned mesh path
    renderer_draw_mesh(gym_state->quad_mesh_handle,
                       gym_state->quad_material_handle, quad_model);
  }
  PROFILE_END();

  PROFILE_BEGIN("game: draw skinned meshes");
  // todo: draw skinned mesh function
  SkinnedModel *skinned_model = &entity->skinned_model;
  mat4 *model_matrix = &entity->model_matrix;
  for (u32 i = 0; i < skinned_model->meshes.len; i++) {
    SkinnedMesh *mesh = &skinned_model->meshes.items[i];
    // todo: blendshapes
    BlendshapeParams *blendshape_parms =
        ALLOC(&ctx->temp_allocator, BlendshapeParams);
    blendshape_parms->count = mesh->blendshape_weights.len;

    memcpy(blendshape_parms->weights, mesh->blendshape_weights.items,
           sizeof(f32) * mesh->blendshape_weights.len);

    for (u32 k = 0; k < mesh->submeshes.len; k++) {
      SkinnedSubMesh *submesh = &mesh->submeshes.items[k];
      Handle mesh_handle = submesh->mesh_handle;
      Handle material_handle = submesh->material_handle;

      if (handle_is_valid(mesh_handle) && handle_is_valid(material_handle)) {
        renderer_draw_skinned_mesh(mesh_handle, material_handle, *model_matrix,
                                   skinned_model->joint_matrices.items,
                                   skinned_model->joint_matrices.len,
                                   blendshape_parms);
      }
    }
  }
  PROFILE_END();

  PROFILE_BEGIN("game: costumes");
  // Render all costumes that are loaded
  for (u32 costume_idx = 0; costume_idx < gym_state->num_costumes;
       costume_idx++) {
    if (gym_state->costume_skinned_models[costume_idx].meshes.items &&
        entity->skinned_model.joint_matrices.items &&
        gym_state->costume_map_created[costume_idx]) {

      // Copy mapped joint matrices from Tolan to this costume
      PROFILE_BEGIN("game: costume copy joints");
      for (u32 joint_idx = 0;
           joint_idx < gym_state->costume_joint_counts[costume_idx];
           joint_idx++) {
        i32 tolan_idx =
            gym_state->costume_to_tolan_joint_maps[costume_idx][joint_idx];
        mat4 *joint_mat = &gym_state->costume_skinned_models[costume_idx]
                               .joint_matrices.items[joint_idx];

        if (tolan_idx >= 0 &&
            (u32)tolan_idx < entity->skinned_model.joint_matrices.len) {
          glm_mat4_copy(entity->skinned_model.joint_matrices.items[tolan_idx],
                        *joint_mat);
          if (costume_idx == 1) { // pants
            quaternion q;
            quat_from_euler((vec3){glm_rad(90), 0, 0}, q);
            mat4 t;
            mat_tr((vec3){0, -0.061, 0.0}, q, t);
            mat4_mul(entity->skinned_model.joint_matrices.items[tolan_idx], t,
                     *joint_mat);
          } else if (costume_idx == 2) { // shoes
            f32 sign = 1.0;
            // invert right foot
            if (joint_idx >= 48 && joint_idx <= 52) {
              sign = -1.0;
            }
            quaternion q;
            quat_from_euler(
                (vec3){glm_rad(90), glm_rad(-15 * sign), glm_rad(0)}, q);
            mat4 t;
            mat_tr((vec3){0.115 * sign, -0.000, 0.0}, q, t);
            mat4_mul(entity->skinned_model.joint_matrices.items[tolan_idx], t,
                     *joint_mat);
          } else if (costume_idx == 3) { // scarf
            quaternion q;
            quat_from_euler((vec3){glm_rad(45), glm_rad(0), glm_rad(0)}, q);
            mat4 t;
            mat_t((vec3){0, -0.1, 0}, t);
            mat4_mul(*joint_mat, t, *joint_mat);
          }
        } else {
          // No matching joint, use identity matrix
          glm_mat4_identity(gym_state->costume_skinned_models[costume_idx]
                                .joint_matrices.items[joint_idx]);
        }
      }
      PROFILE_END();

      PROFILE_BEGIN("game: draw costumes");
      // Render this costume with same transform as Tolan
      for (u32 i = 0;
           i < gym_state->costume_skinned_models[costume_idx].meshes.len; i++) {
        SkinnedMesh *mesh =
            &gym_state->costume_skinned_models[costume_idx].meshes.items[i];
        BlendshapeParams *blendshape_parms =
            ALLOC(&ctx->temp_allocator, BlendshapeParams);
        blendshape_parms->count = mesh->blendshape_weights.len;

        memcpy(blendshape_parms->weights, mesh->blendshape_weights.items,
               sizeof(f32) * mesh->blendshape_weights.len);

        for (u32 k = 0; k < mesh->submeshes.len; k++) {
          SkinnedSubMesh *submesh = &mesh->submeshes.items[k];
          Handle mesh_handle = submesh->mesh_handle;
          Handle material_handle = submesh->material_handle;

          if (handle_is_valid(mesh_handle) &&
              handle_is_valid(material_handle)) {
            renderer_draw_skinned_mesh(
                mesh_handle, material_handle, *model_matrix,
                gym_state->costume_skinned_models[costume_idx]
                    .joint_matrices.items,
                gym_state->costume_skinned_models[costume_idx]
                    .joint_matrices.len,
                blendshape_parms);
          }
        }
      }
      PROFILE_END();
    }
  }
  PROFILE_END();

  input_end_frame(input);
  ALLOC_RESET(&gym_state->ctx.temp_allocator);
}
