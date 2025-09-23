#ifndef H_GAMEPLAYLIB
#define H_GAMEPLAYLIB

/* Code that is *technically* reusable, but not really
 * or reusable but only in context of specific game
 * or that I haven't figure out the right abstraction yet
 * leave comment explaining thoughts
 * */

#include "lib/lipsync.h"
#include "lib/string.h"
#include "lib/audio.h"
#include "renderer/renderer.h"

typedef struct {
  String32Bytes name;
  String32Bytes phoneme_name;
  u32 index;
  f32 weight;
  f32 target_weight;
  f32 weight_velocity;
  f32 max_weight;
} LipsyncBlendshape;
slice_define(LipsyncBlendshape);

// Blendshape controller
typedef struct {
  SkinnedMesh *mesh;
  LipsyncBlendshape_Slice blendshapes;
  f32 smoothness;
  bool32 use_phoneme_blend;
  LipSyncProfile *profile;
  f32 min_volume;
  f32 max_volume;
  f32 volume;
  f32 volume_velocity;
} LipsyncBlendshapeController;

typedef struct {
  char phoneme_name[MAX_PHONEME_NAME_LENGTH];
  char blendshape_name[32];
} PhonemeBlendshapeDefinition;

LipsyncBlendshapeController
blendshape_controller_init(Allocator *allocator, LipSyncProfile *profile,
                           PhonemeBlendshapeDefinition *phonemes_to_blendshapes,
                           u32 phonemes_to_blendshapes_len,
                           SkinnedMesh *face_mesh) {
  LipsyncBlendshapeController controller = {0};
  controller.mesh = face_mesh;
  controller.profile = profile;

  controller.blendshapes = slice_new_ALLOC(allocator, LipsyncBlendshape,
                                           phonemes_to_blendshapes_len);
  controller.smoothness = 0.06f;
  controller.use_phoneme_blend = false;
  controller.min_volume = -2.5f;
  controller.max_volume = -1.5f;
  controller.volume = 0.0f;
  controller.volume_velocity = 0.0f;

  // Convert definitions to runtime mappings
  for (u32 i = 0; i < phonemes_to_blendshapes_len; i++) {
    PhonemeBlendshapeDefinition *def = &phonemes_to_blendshapes[i];

    // Find phoneme index
    i32 phoneme_index = -1;
    for (i32 p = 0; p < profile->mfcc_count; p++) {
      if (str_equal(profile->mfccs[p].name, def->phoneme_name)) {
        phoneme_index = p;
        break;
      }
    }

    // Find blendshape index
    u32 blendshape_index = UINT32_MAX;
    for (u32 b = 0; b < face_mesh->blendshape_names.len; b++) {
      if (str_equal(face_mesh->blendshape_names.items[b].value,
                    def->blendshape_name)) {
        blendshape_index = b;
        break;
      }
    }

    // Store runtime mapping (with validation)
    if (phoneme_index >= 0 && blendshape_index != UINT32_MAX) {
      LOG_INFO("%", FMT_STR(def->phoneme_name));
      f32 max_weight = 1.0;
      if (str_equal(def->phoneme_name, "A") ||
          str_equal(def->phoneme_name, "O")) {
        max_weight = 0.75;
      }
      LipsyncBlendshape blendshape = {
          .name = fixedstr32_from_cstr(def->blendshape_name),
          .phoneme_name = fixedstr32_from_cstr(def->phoneme_name),
          .index = blendshape_index,
          .weight = 0.0f,
          .target_weight = 0.0f,
          .weight_velocity = 0.0f,
          .max_weight = max_weight,
      };
      slice_append(controller.blendshapes, blendshape);

      LOG_INFO("Mapped phoneme '%' (index %) to blendshape '%' (index %)",
               FMT_STR(def->phoneme_name), FMT_INT(phoneme_index),
               FMT_STR(def->blendshape_name), FMT_INT(blendshape_index));
    } else {
      LOG_WARN("Failed to map phoneme '%' to blendshape '%'",
               FMT_STR(def->phoneme_name), FMT_STR(def->blendshape_name));
    }
  }

  return controller;
}

f32 smooth_damp(f32 current, f32 target, f32 *current_velocity, f32 smooth_time,
                f32 max_speed, f32 delta_time) {
  smooth_time = fmaxf(0.0001f, smooth_time);
  f32 num = 2.0f / smooth_time;
  f32 num2 = num * delta_time;
  f32 num3 =
      1.0f / (1.0f + num2 + 0.48f * num2 * num2 + 0.235f * num2 * num2 * num2);
  f32 num4 = current - target;
  f32 num5 = target;
  f32 num6 = max_speed * smooth_time;

  num4 = clamp(num4, -num6, num6);
  target = current - num4;

  f32 num7 = (*current_velocity + num * num4) * delta_time;
  *current_velocity = (*current_velocity - num * num7) * num3;
  f32 num8 = target + (num4 + num7) * num3;

  if ((num5 - current > 0.0f) == (num8 > num5)) {
    num8 = num5;
    *current_velocity = (num8 - num5) / delta_time;
  }

  return num8;
}

void blendshape_lipsync_update_volume(LipsyncBlendshapeController *controller,
                                      LipSyncResult lipsync_result, f32 dt) {
  f32 norm_vol = 0.0f;
  if (lipsync_result.volume > 0.0f) {
    norm_vol = log10f(lipsync_result.volume);
    norm_vol = (norm_vol - controller->min_volume) /
               fmaxf(controller->max_volume - controller->min_volume, 1e-4f);
    norm_vol = clamp(norm_vol, 0.0f, 1.0f);
  }
  controller->volume =
      smooth_damp(controller->volume, norm_vol, &controller->volume_velocity,
                  controller->smoothness, INFINITY, dt);
}

void blendshape_lipsync_update_vowels(LipsyncBlendshapeController *controller,
                                      LipSyncResult lipsync_result, f32 dt) {
  f32 sum = 0.0f;

  // Set target weights for each blendshape
  arr_foreach_ptr(controller->blendshapes, blendshape) {
    f32 target_weight = 0.0f;

    if (controller->use_phoneme_blend && lipsync_result.all_scores) {
      // Find the phoneme index for this blendshape
      for (i32 p = 0; p < controller->profile->mfcc_count; p++) {
        if (str_equal(controller->profile->mfccs[p].name,
                      blendshape->phoneme_name.value)) {
          target_weight = lipsync_result.all_scores[p];
          break;
        }
      }
    } else {
      // Single phoneme mode
      if (lipsync_result.best_phoneme_name &&
          str_equal(blendshape->phoneme_name.value,
                    lipsync_result.best_phoneme_name)) {
        target_weight = 1.0f;
      }
    }

    blendshape->weight = smooth_damp(blendshape->weight, target_weight,
                                     &blendshape->weight_velocity,
                                     controller->smoothness, INFINITY, dt);
    sum += blendshape->weight;
  }

  if (sum > 0.0f) {
    arr_foreach_ptr(controller->blendshapes, blendshape) {
      blendshape->weight = blendshape->weight / sum;
    }
  }
}

void blendshape_controller_update(LipsyncBlendshapeController *controller,
                                  LipSyncResult lipsync_result, f32 dt) {
  blendshape_lipsync_update_volume(controller, lipsync_result, dt);
  blendshape_lipsync_update_vowels(controller, lipsync_result, dt);
}

void blendshape_controller_apply(LipsyncBlendshapeController *controller) {
  SkinnedMesh *face_mesh = controller->mesh;
  debug_assert(face_mesh);
  if (!face_mesh) {
    return;
  }

  // // Reset all weights to 0 first (Unity does this)
  // for (i32 i = 0; i < (i32)face_mesh->blendshape_weights.len; i++) {
  //   face_mesh->blendshape_weights.items[i] = 0.0f;
  // }

  // Apply weights from controller (adapted for 0-1 blendshape range):
  // weight += bs.weight * bs.maxWeight * volume
  arr_foreach_ptr(controller->blendshapes, blendshape) {
    if (blendshape->index < face_mesh->blendshape_weights.len) {
      f32 current_weight =
          face_mesh->blendshape_weights.items[blendshape->index];
      f32 final_weight =
          current_weight +
          (blendshape->weight * blendshape->max_weight * controller->volume);

      face_mesh->blendshape_weights.items[blendshape->index] = final_weight;
    }
  }
}

Material *material_from_asset(MaterialAsset *asset, AssetSystem *asset_system,
                              GameContext *ctx) {
  Allocator *allocator = &ctx->allocator;
  Material *material = ALLOC(allocator, Material);
  *material = (Material){0};

  material->asset = asset;

  // Initialize properties slice
  material->properties =
      slice_new_ALLOC(allocator, MaterialProperty, asset->properties.len + 5);

  b32 has_detail_tex = false;
  b32 has_tex = false;
  // Convert MaterialAssetProperty to MaterialProperty (load actual textures)
  arr_foreach_ptr(asset->properties, asset_prop) {
    MaterialProperty prop = {.name = asset_prop->name,
                             .type = asset_prop->type};

    if (str_equal(prop.name.value, "uDetailTexture")) {
      has_detail_tex = true;
    } else if (str_equal(prop.name.value, "uTexture")) {
      has_tex = true;
    }

    switch (asset_prop->type) {
    case MAT_PROP_INVALID:
      break;
    case MAT_PROP_TEXTURE: {
      char *asset_path = asset_prop->name.len ? asset_prop->texture_path.value
                                              : "textures/white_pixel.webp";
      Texture_Handle tex_handle =
          asset_request(Texture, asset_system, ctx, asset_path);
      Texture *tex = asset_get_data_unsafe(Texture, asset_system, tex_handle);
      assert(tex);
      assert(handle_is_valid(tex->gpu_tex_handle));
      prop.value.texture = cast_handle(Texture_Handle, tex->gpu_tex_handle);
    } break;
    case MAT_PROP_VEC3:
      glm_vec3_copy(asset_prop->color.components, prop.value.vec3_val);
      break;
    }

    slice_append(material->properties, prop);
  }

  if (!has_tex) {
    MaterialProperty prop = {
        .name =
            (String){
                .value = "uTexture",
                .len = ARRAY_SIZE("uTexture") - 1,
            },
        .type = MAT_PROP_TEXTURE,
    };

    Texture_Handle tex_handle =
        asset_request(Texture, asset_system, ctx, "textures/white_pixel.webp");
    Texture *tex = asset_get_data_unsafe(Texture, asset_system, tex_handle);
    assert(tex);
    assert(handle_is_valid(tex->gpu_tex_handle));
    prop.value.texture = cast_handle(Texture_Handle, tex->gpu_tex_handle);

    slice_append(material->properties, prop);
  }

  if (!has_detail_tex) {
    MaterialProperty prop = {
        .name =
            (String){
                .value = "uDetailTexture",
                .len = ARRAY_SIZE("uDetailTexture") - 1,
            },
        .type = MAT_PROP_TEXTURE,
    };

    Texture_Handle tex_handle = asset_request(Texture, asset_system, ctx,
                                              "textures/transparent_pixel.webp");
    Texture *tex = asset_get_data_unsafe(Texture, asset_system, tex_handle);
    assert(tex);
    assert(handle_is_valid(tex->gpu_tex_handle));
    prop.value.texture = cast_handle(Texture_Handle, tex->gpu_tex_handle);

    slice_append(material->properties, prop);
  }

  // char *vert_shader_path =
  //     str_equal(asset->shader_path.value, "materials/background_img.frag")
  //         ? "materials/skybox.vert"
  //         : "materials/standard.vert";
  // Load shader with defines from MaterialAsset
  LoadShaderParams shader_params = {
      // .vert_shader_path = vert_shader_path,
      // .shader_name = asset->shader_path.value,
      .shader_name = "toon_shading",
      // .defines = asset->shader_defines.items,
      // .define_count = asset->shader_defines.len,
  };
  Handle shader_handle = load_shader(shader_params);

  material->gpu_material =
      load_material(shader_handle, material->properties.items,
                    material->properties.len, material->asset->transparent);

  return material;
}

SkinnedModel skmodel_from_asset(GameContext *ctx, Model3DData *model_data,
                                Material_Slice temp_materials) {
  SkinnedModel skinned_model = {0};
  skinned_model.meshes =
      arr_new_ALLOC(&ctx->allocator, SkinnedMesh, model_data->num_meshes);
  skinned_model.joint_matrices =
      arr_new_ALLOC(&ctx->allocator, mat4, model_data->len_joints);

  // Initialize each SkinnedMesh
  u32 material_index = 0; // Sequential material index across all submeshes
  for (u32 i = 0; i < model_data->num_meshes; i++) {
    MeshData *mesh_data = &model_data->meshes[i];
    SkinnedMesh *skinned_mesh = &skinned_model.meshes.items[i];

    // Set mesh-level blendshape data
    if (mesh_data->blendshape_names.len > 0) {
      skinned_mesh->blendshape_names = arr_new_ALLOC(
          &ctx->allocator, String32Bytes, mesh_data->blendshape_names.len);
      skinned_mesh->blendshape_names.len = mesh_data->blendshape_names.len;
      memcpy(skinned_mesh->blendshape_names.items,
             mesh_data->blendshape_names.items,
             sizeof(String32Bytes) * mesh_data->blendshape_names.len);

      skinned_mesh->blendshape_weights =
          arr_new_ALLOC(&ctx->allocator, f32, mesh_data->blendshape_names.len);
      skinned_mesh->blendshape_weights.len = mesh_data->blendshape_names.len;
    } else {
      skinned_mesh->blendshape_names = arr_new_zero(String32Bytes);
      skinned_mesh->blendshape_weights = arr_new_zero(f32);
    }

    // Create submeshes array
    skinned_mesh->submeshes = arr_new_ALLOC(&ctx->allocator, SkinnedSubMesh,
                                            mesh_data->submeshes.len);

    // Initialize each submesh
    for (u32 j = 0; j < mesh_data->submeshes.len; j++) {
      SubMeshData *submesh_data = &mesh_data->submeshes.items[j];
      SkinnedSubMesh *skinned_submesh = &skinned_mesh->submeshes.items[j];

      skinned_submesh->mesh_handle =
          renderer_create_submesh(submesh_data, true /*is skinned*/);

      // Get material for this submesh
      if (material_index >= temp_materials.len) {
        LOG_WARN("Invalid material index % for mesh % submesh %, using default",
                 FMT_UINT(material_index), FMT_UINT(i), FMT_UINT(j));
        material_index = 0;
      }

      Material *material = &temp_materials.items[material_index];
      skinned_submesh->material_handle = material->gpu_material;

      material_index++; // Move to next material for next submesh
    }
    skinned_mesh->submeshes.len = mesh_data->submeshes.len;
  }
  skinned_model.meshes.len = model_data->num_meshes;
  return skinned_model;
}
#endif /* ifndef H_GAMEPLAYLIB */
