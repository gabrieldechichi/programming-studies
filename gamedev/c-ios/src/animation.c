#include "animation.h"
#include "lib/array.h"
#include "lib/assert.h"
#include "lib/common.h"
#include "lib/fmt.h"
#include "lib/math.h"
#include "lib/serialization.h"
#include "lib/typedefs.h"
#include "platform/platform.h"
#include "renderer/renderer.h"
#include "vendor/cglm/types.h"
#include "vendor/cglm/vec3.h"

void apply_joint_transform_recursive(Joint_Array joints,
                                     mat4_Array joint_matrices, u32 joint_idx,
                                     mat4 parent_transform) {
  mat4 *joint_transform = arr_get_ptr(joint_matrices, joint_idx);
  mat4_mul(parent_transform, *joint_transform, *joint_transform);
  Joint *joint = arr_get_ptr(joints, joint_idx);
  arr_foreach(joint->children, child_idx) {
    apply_joint_transform_recursive(joints, joint_matrices, child_idx,
                                    *joint_transform);
  }

  mat4_mul(*joint_transform, joint->inverse_bind_matrix, *joint_transform);
}

b32 find_start_end_keyframe(AnimationState *animation,
                            _out_ i32 *start_keyframe_idx,
                            _out_ i32 *end_keyframe_idx, _out_ f32 *t) {
  debug_assert(animation->animation->keyframes.len > 0);
  if (animation->animation->keyframes.len <= 0) {
    return false;
  }

  u32 len = animation->animation->keyframes.len;
  f32 time = animation->time;

  // Handle wrap case first
  if (animation->animation->keyframes.items[0].timestamp >= time) {
    *start_keyframe_idx = max(0, len - 1);
    *end_keyframe_idx = 0;
    *t = lerp_inverse(
        animation->animation->keyframes.items[*start_keyframe_idx].timestamp,
        animation->animation->keyframes.items[*end_keyframe_idx].timestamp,
        animation->time);
    return true;
  }

  // Binary search for the keyframe
  i32 left = 0;
  i32 right = len - 1;

  while (left < right) {
    i32 mid = left + (right - left) / 2;
    if (animation->animation->keyframes.items[mid].timestamp <= time) {
      left = mid + 1;
    } else {
      right = mid;
    }
  }

  // left is now the first keyframe with timestamp > time
  *start_keyframe_idx = max(0, left - 1);
  *end_keyframe_idx = left;
  *t = lerp_inverse(
      animation->animation->keyframes.items[*start_keyframe_idx].timestamp,
      animation->animation->keyframes.items[*end_keyframe_idx].timestamp,
      animation->time);

  return true;
}

b32 find_blendshape_start_end_keyframe(AnimationState *animation,
                                       _out_ i32 *start_keyframe_idx,
                                       _out_ i32 *end_keyframe_idx,
                                       _out_ f32 *t) {
  debug_assert(animation->animation->blendshape_keyframes.len > 0);
  if (animation->animation->blendshape_keyframes.len <= 0) {
    return false;
  }

  u32 len = animation->animation->blendshape_keyframes.len;
  f32 time = animation->time;

  // Handle wrap case first
  if (animation->animation->blendshape_keyframes.items[0].timestamp >= time) {
    *start_keyframe_idx = max(0, len - 1);
    *end_keyframe_idx = 0;
    *t = lerp_inverse(
        animation->animation->blendshape_keyframes.items[*start_keyframe_idx]
            .timestamp,
        animation->animation->blendshape_keyframes.items[*end_keyframe_idx]
            .timestamp,
        animation->time);
    return true;
  }

  // Binary search for the keyframe
  i32 left = 0;
  i32 right = len - 1;

  while (left < right) {
    i32 mid = left + (right - left) / 2;
    if (animation->animation->blendshape_keyframes.items[mid].timestamp <=
        time) {
      left = mid + 1;
    } else {
      right = mid;
    }
  }

  // left is now the first keyframe with timestamp > time
  *start_keyframe_idx = max(left - 1, 0);
  *end_keyframe_idx = left;
  *t = lerp_inverse(
      animation->animation->blendshape_keyframes.items[*start_keyframe_idx]
          .timestamp,
      animation->animation->blendshape_keyframes.items[*end_keyframe_idx]
          .timestamp,
      animation->time);

  return true;
}

void animation_update(AnimationState *animation, f32 dt) {
  assert(animation);
  if (!animation || !animation->animation) {
    return;
  }

  if (animation->weight == 0) {
    return;
  }

  f32 length = animation->animation->length;

  animation->time += dt * animation->speed;

  if (animation->loop && length > 0) {
    // Use fmod to handle multiple wraps and negative values
    animation->time = fmodf(animation->time, length);

    // Ensure positive time for negative wrap-around
    if (animation->time < 0.0f) {
      animation->time += length;
    }
  } else {
    animation->time = clamp(animation->time, 0, length);
  }
}

void animation_evaluate(AnimationState *animation,
                        _out_ JointTransform_Array joint_transforms) {
  assert(animation);
  if (!animation || !animation->animation) {
    return;
  }

  i32 start_keyframe_idx, end_keyframe_idx;
  f32 percent;
  if (!find_start_end_keyframe(animation, &start_keyframe_idx,
                               &end_keyframe_idx, &percent)) {
    return;
  }

  Keyframe *start_keyframe =
      &animation->animation->keyframes.items[start_keyframe_idx];
  Keyframe *end_keyframe =
      &animation->animation->keyframes.items[end_keyframe_idx];

  // we require every keyframe to define every joint
  assert_msg(start_keyframe->joint_transforms.len ==
                 end_keyframe->joint_transforms.len,
             "Expected keyframes to have matching transforms. Start % End %",
             FMT_UINT(start_keyframe->joint_transforms.len),
             FMT_UINT(end_keyframe->joint_transforms.len));

  // ensure output array can hold all joints
  assert(joint_transforms.len >= start_keyframe->joint_transforms.len);

  // evaluate all joints for keyframe and store weighted local transforms
  for (u32 i = 0; i < start_keyframe->joint_transforms.len; i++) {
    KeyframeJointTransform *start_joint_transform =
        &start_keyframe->joint_transforms.items[i];
    KeyframeJointTransform *end_joint_transform =
        &end_keyframe->joint_transforms.items[i];

    // again, every keyframe defines every joint in the same order
    assert(start_joint_transform->index == end_joint_transform->index);

    u32 joint_index = start_joint_transform->index;
    if (joint_index >= joint_transforms.len) {
      continue;
    }

    JointTransform *joint_transform = &joint_transforms.items[joint_index];

    vec3 interpolated_translation;
    quaternion interpolated_rotation;
    vec3_lerp(start_joint_transform->translation,
              end_joint_transform->translation, percent,
              interpolated_translation);
    quat_nlerp(start_joint_transform->rotation, end_joint_transform->rotation,
               percent, interpolated_rotation);

    // apply animation weight to the interpolated transforms
    glm_vec3_scale(interpolated_translation, animation->weight,
                   joint_transform->translation);
    quat_scale(interpolated_rotation, animation->weight,
               joint_transform->rotation);

    // todo: you know what you did
    if (i == 0) {
      // glm_vec3_zero(joint_transform->translation);
    }
  }
}

void animation_evaluate_blendshapes(AnimationState *animation,
                                    Model3DData *model,
                                    _out_ BlendshapeEvalResult_Array results) {
  assert(animation);
  assert(model);
  if (!animation || !animation->animation || !model) {
    return;
  }

  // Early exit if no blendshape keyframes
  if (animation->animation->blendshape_keyframes.len == 0) {
    return;
  }

  i32 start_keyframe_idx, end_keyframe_idx;
  f32 percent;
  if (!find_blendshape_start_end_keyframe(animation, &start_keyframe_idx,
                                          &end_keyframe_idx, &percent)) {
    return;
  }

  RuntimeBlendshapeKeyframe *start_keyframe =
      &animation->animation->blendshape_keyframes.items[start_keyframe_idx];
  RuntimeBlendshapeKeyframe *end_keyframe =
      &animation->animation->blendshape_keyframes.items[end_keyframe_idx];

  // we require every keyframe to define the same blendshape transforms
  assert_msg(start_keyframe->blendshape_transforms.len ==
                 end_keyframe->blendshape_transforms.len,
             "Expected blendshape keyframes to have matching transforms. Start "
             "% End %",
             FMT_UINT(start_keyframe->blendshape_transforms.len),
             FMT_UINT(end_keyframe->blendshape_transforms.len));

  // evaluate all blendshape transforms for keyframe
  for (u32 i = 0; i < start_keyframe->blendshape_transforms.len; i++) {
    BlendshapeKeyframe *start_bs_keyframe =
        &start_keyframe->blendshape_transforms.items[i];
    BlendshapeKeyframe *end_bs_keyframe =
        &end_keyframe->blendshape_transforms.items[i];

    // every keyframe defines the same meshes in the same order
    assert(start_bs_keyframe->mesh_index == end_bs_keyframe->mesh_index);
    assert(start_bs_keyframe->blendshape_len ==
           end_bs_keyframe->blendshape_len);

    u32 mesh_index = start_bs_keyframe->mesh_index;
    if (mesh_index >= model->num_meshes) {
      continue;
    }

    // find or create result for this mesh
    BlendshapeEvalResult *mesh_result = NULL;
    for (u32 j = 0; j < results.len; j++) {
      if (results.items[j].mesh_index == mesh_index) {
        mesh_result = &results.items[j];
        break;
      }
    }

    if (!mesh_result) {
      continue; // mesh result should be pre-allocated
    }

    // interpolate blendshape weights
    for (u32 bs_idx = 0; bs_idx < start_bs_keyframe->blendshape_len; bs_idx++) {
      u32 blendshape_index = start_bs_keyframe->blendshape_indices[bs_idx];

      // ensure the blendshape index is valid for this mesh
      if (blendshape_index >= mesh_result->blendshape_weights.len) {
        continue;
      }

      f32 start_weight = start_bs_keyframe->blendshape_values[bs_idx];
      f32 end_weight = end_bs_keyframe->blendshape_values[bs_idx];

      // interpolate and apply animation weight
      f32 interpolated_weight = lerpc(start_weight, end_weight, percent);
      f32 weighted_value = interpolated_weight * animation->weight;

      // accumulate (additive blending)
      mesh_result->blendshape_weights.items[blendshape_index] += weighted_value;
    }
  }
}

bool32 animation_asset_write(const AnimationAsset *animation,
                             Allocator *allocator, _out_ u8_Array *buffer) {
  debug_assert(animation);
  debug_assert(allocator);
  if (!animation || !allocator) {
    return false;
  }

  // calculate total_size
  size_t total_size = 0;
  total_size += sizeof(u32);         // name length
  total_size += animation->name.len; // name data
  total_size += sizeof(u32);         // keyframes.len
  arr_foreach_ptr(animation->keyframes, keyframe) {
    total_size += sizeof(f32); // timestamp
    total_size += sizeof(u32); // joint_transforms.len
    arr_foreach_ptr(keyframe->joint_transforms, 
                joint_transform) {
      total_size += sizeof(u32);              // joint name length
      total_size += joint_transform->name.len; // joint name data
      total_size += sizeof(vec3) + sizeof(quaternion) +
                    sizeof(vec3); // translation, rotation, scale
    }
  }

  // blendshape keyframes
  total_size += sizeof(u32); // blendshape_keyframes.len
  arr_foreach_ptr(animation->blendshape_keyframes, 
              bs_keyframe) {
    total_size += sizeof(f32);               // timestamp
    total_size += sizeof(u32);               // mesh_name length
    total_size += bs_keyframe->mesh_name.len; // mesh_name data
    total_size += sizeof(u32);               // blendshape_len
    for (u32 i = 0; i < bs_keyframe->blendshape_len; i++) {
      total_size += sizeof(u32); // blendshape name length
      total_size += bs_keyframe->blendshape_names[i].len; // blendshape name data
    }
    total_size += bs_keyframe->blendshape_len * sizeof(f32); // blendshape values
  }

  BinaryWriter writer = {.cur_offset = 0,
                         .len = total_size,
                         .bytes = ALLOC_ARRAY(allocator, u8, total_size)};

  write_u32(&writer, animation->name.len);
  write_u8(&writer, (u8 *)animation->name.value, animation->name.len);
  write_u32(&writer, animation->keyframes.len);

  arr_foreach_ptr(animation->keyframes, keyframe) {
    write_f32(&writer, keyframe->timestamp);
    write_u32(&writer, keyframe->joint_transforms.len);

    arr_foreach_ptr(keyframe->joint_transforms, 
                joint_transform) {
      write_u32(&writer, joint_transform->name.len);
      write_u8(&writer, (u8 *)joint_transform->name.value,
               joint_transform->name.len);
      write_f32_array(&writer, (f32 *)&joint_transform->translation, 3);
      write_f32_array(&writer, (f32 *)&joint_transform->rotation, 4);
      write_f32_array(&writer, (f32 *)&joint_transform->scale, 3);
    }
  }

  // write blendshape keyframes
  write_u32(&writer, animation->blendshape_keyframes.len);
  arr_foreach_ptr(animation->blendshape_keyframes, 
              bs_keyframe) {
    write_f32(&writer, bs_keyframe->timestamp);
    write_u32(&writer, bs_keyframe->mesh_name.len);
    write_u8(&writer, (u8 *)bs_keyframe->mesh_name.value,
             bs_keyframe->mesh_name.len);
    write_u32(&writer, bs_keyframe->blendshape_len);

    for (u32 i = 0; i < bs_keyframe->blendshape_len; i++) {
      write_u32(&writer, bs_keyframe->blendshape_names[i].len);
      write_u8(&writer, (u8 *)bs_keyframe->blendshape_names[i].value,
               bs_keyframe->blendshape_names[i].len);
    }

    write_f32_array(&writer, bs_keyframe->blendshape_values,
                    bs_keyframe->blendshape_len);
  }

  assert(writer.cur_offset == writer.len);
  buffer->len = writer.len;
  buffer->items = writer.bytes;
  return true;
}

AnimationAsset *animation_asset_read(const u8_Array binary_data,
                                     Allocator *allocator) {
  debug_assert(allocator);
  debug_assert(binary_data.items);
  debug_assert(binary_data.len > 0);
  if (!allocator || !binary_data.len || binary_data.len == 0) {
    return NULL;
  }

  BinaryReader reader = {
      .bytes = binary_data.items, .len = binary_data.len, .cur_offset = 0};
  AnimationAsset *animation = ALLOC(allocator, AnimationAsset);

  u32 name_len = 0;
  read_u32(&reader, &name_len);
  animation->name.len = name_len;
  animation->name.value = ALLOC_ARRAY(allocator, char, name_len + 1);
  read_u8_array(&reader, (u8 *)animation->name.value, name_len);
  animation->name.value[name_len] = '\0';

  u32 keyframes_len = 0;
  read_u32(&reader, &keyframes_len);

  if (keyframes_len > 0) {
    animation->keyframes =
        arr_new_ALLOC(allocator, KeyframeAsset, keyframes_len);
    arr_foreach_ptr(animation->keyframes, keyframe) {
      read_f32(&reader, &keyframe->timestamp);

      u32 joint_transforms_len = 0;
      read_u32(&reader, &joint_transforms_len);

      if (joint_transforms_len > 0) {
        keyframe->joint_transforms = arr_new_ALLOC(
            allocator, KeyframeAssetJointTransform, joint_transforms_len);

        arr_foreach_ptr(keyframe->joint_transforms, 
                        joint_transform) {
          u32 joint_name_len = 0;
          read_u32(&reader, &joint_name_len);
          joint_transform->name.len = joint_name_len;
          joint_transform->name.value =
              ALLOC_ARRAY(allocator, char, joint_name_len + 1);
          read_u8_array(&reader, (u8 *)joint_transform->name.value,
                        joint_name_len);
          joint_transform->name.value[joint_name_len] = '\0';
          read_f32_array(&reader, (f32 *)&joint_transform->translation, 3);
          read_f32_array(&reader, (f32 *)&joint_transform->rotation, 4);
          read_f32_array(&reader, (f32 *)&joint_transform->scale, 3);
        }
      }
    }
  }

  // read blendshape keyframes
  u32 blendshape_keyframes_len = 0;
  read_u32(&reader, &blendshape_keyframes_len);

  if (blendshape_keyframes_len > 0) {
    animation->blendshape_keyframes = arr_new_ALLOC(
        allocator, BlendshapeKeyframeAsset, blendshape_keyframes_len);
    arr_foreach_ptr(animation->blendshape_keyframes, 
                    bs_keyframe) {
      read_f32(&reader, &bs_keyframe->timestamp);

      u32 mesh_name_len = 0;
      read_u32(&reader, &mesh_name_len);
      bs_keyframe->mesh_name.len = mesh_name_len;
      bs_keyframe->mesh_name.value =
          ALLOC_ARRAY(allocator, char, mesh_name_len + 1);
      read_u8_array(&reader, (u8 *)bs_keyframe->mesh_name.value, mesh_name_len);
      bs_keyframe->mesh_name.value[mesh_name_len] = '\0';

      read_u32(&reader, &bs_keyframe->blendshape_len);

      if (bs_keyframe->blendshape_len > 0) {
        bs_keyframe->blendshape_names =
            ALLOC_ARRAY(allocator, String, bs_keyframe->blendshape_len);
        bs_keyframe->blendshape_values =
            ALLOC_ARRAY(allocator, f32, bs_keyframe->blendshape_len);

        for (u32 i = 0; i < bs_keyframe->blendshape_len; i++) {
          u32 blendshape_name_len = 0;
          read_u32(&reader, &blendshape_name_len);
          bs_keyframe->blendshape_names[i].len = blendshape_name_len;
          bs_keyframe->blendshape_names[i].value =
              ALLOC_ARRAY(allocator, char, blendshape_name_len + 1);
          read_u8_array(&reader, (u8 *)bs_keyframe->blendshape_names[i].value,
                        blendshape_name_len);
          bs_keyframe->blendshape_names[i].value[blendshape_name_len] = '\0';
        }

        read_f32_array(&reader, bs_keyframe->blendshape_values,
                       bs_keyframe->blendshape_len);
      } else {
        bs_keyframe->blendshape_names = NULL;
        bs_keyframe->blendshape_values = NULL;
      }
    }
  } else {
    animation->blendshape_keyframes = arr_new_zero(BlendshapeKeyframeAsset);
  }

  assert(reader.cur_offset == reader.len);
  return animation;
}

Animation *animation_from_asset(const AnimationAsset *animation_asset,
                                Model3DData *model, Allocator *allocator) {
  assert_msg(animation_asset, "animation_asset is null");
  assert_msg(model, "model is null");
  assert_msg(allocator, "allocator is null");

  if (!animation_asset || !model || !allocator) {
    return NULL;
  }

  Animation *animation = ALLOC(allocator, Animation);
  animation->model = model;
  animation->name = str_from_cstr_alloc(animation_asset->name.value,
                                        animation_asset->name.len, allocator);

  // Convert keyframes
  animation->keyframes =
      arr_new_ALLOC(allocator, Keyframe, animation_asset->keyframes.len);

  for (u32 keyframe_idx = 0; keyframe_idx < animation_asset->keyframes.len;
       keyframe_idx++) {
    KeyframeAsset *asset_keyframe =
        &animation_asset->keyframes.items[keyframe_idx];
    Keyframe *runtime_keyframe = &animation->keyframes.items[keyframe_idx];

    runtime_keyframe->timestamp = asset_keyframe->timestamp;
    runtime_keyframe->joint_transforms =
        arr_new_ALLOC(allocator, KeyframeJointTransform,
                      asset_keyframe->joint_transforms.len);

    u32 valid_transforms = 0;
    for (u32 transform_idx = 0;
         transform_idx < asset_keyframe->joint_transforms.len;
         transform_idx++) {
      KeyframeAssetJointTransform *asset_transform =
          &asset_keyframe->joint_transforms.items[transform_idx];

      // Find joint index by name
      int32 joint_index = arr_find_index_pred_raw(
          model->joint_names, model->len_joints,
          str_equal(_item.value, asset_transform->name.value));

      if (joint_index >= 0) {
        KeyframeJointTransform *runtime_transform =
            &runtime_keyframe->joint_transforms.items[valid_transforms];
        runtime_transform->index = joint_index;
        vec3_copy(asset_transform->translation, runtime_transform->translation);
        quat_copy(asset_transform->rotation, runtime_transform->rotation);
        vec3_copy(asset_transform->scale, runtime_transform->scale);
        valid_transforms++;
      } else {
        LOG_WARN("Joint '%' (%) not found in model, skipping",
                 FMT_STR(asset_transform->name.value),
                 FMT_UINT(asset_transform->name.len));
      }
    }

    // Resize array to actual valid transforms count
    runtime_keyframe->joint_transforms.len = valid_transforms;
  }

  // Convert blendshape keyframes
  animation->blendshape_keyframes =
      arr_new_ALLOC(allocator, RuntimeBlendshapeKeyframe,
                    animation_asset->blendshape_keyframes.len);

  for (u32 bs_keyframe_idx = 0;
       bs_keyframe_idx < animation_asset->blendshape_keyframes.len;
       bs_keyframe_idx++) {
    BlendshapeKeyframeAsset *asset_bs_keyframe =
        &animation_asset->blendshape_keyframes.items[bs_keyframe_idx];
    RuntimeBlendshapeKeyframe *runtime_bs_keyframe =
        &animation->blendshape_keyframes.items[bs_keyframe_idx];

    runtime_bs_keyframe->timestamp = asset_bs_keyframe->timestamp;

    // Find mesh index by name
    int32 mesh_index = -1;
    for (u32 mesh_idx = 0; mesh_idx < model->num_meshes; mesh_idx++) {
      if (str_equal(model->meshes[mesh_idx].mesh_name.value,
                    asset_bs_keyframe->mesh_name.value)) {
        mesh_index = mesh_idx;
        break;
      }
    }

    if (mesh_index >= 0) {
      // Create single blendshape keyframe for this mesh
      runtime_bs_keyframe->blendshape_transforms =
          arr_new_ALLOC(allocator, BlendshapeKeyframe, 1);

      BlendshapeKeyframe *bs_keyframe =
          &runtime_bs_keyframe->blendshape_transforms.items[0];
      bs_keyframe->mesh_index = mesh_index;
      bs_keyframe->blendshape_len = asset_bs_keyframe->blendshape_len;

      if (asset_bs_keyframe->blendshape_len > 0) {
        bs_keyframe->blendshape_indices =
            ALLOC_ARRAY(allocator, u32, asset_bs_keyframe->blendshape_len);
        bs_keyframe->blendshape_values =
            ALLOC_ARRAY(allocator, f32, asset_bs_keyframe->blendshape_len);

        MeshData *target_mesh = &model->meshes[mesh_index];
        u32 valid_blendshapes = 0;

        // Map blendshape names to indices
        for (u32 bs_idx = 0; bs_idx < asset_bs_keyframe->blendshape_len;
             bs_idx++) {
          String *asset_bs_name = &asset_bs_keyframe->blendshape_names[bs_idx];

          // Find blendshape index by name in target mesh
          int32 blendshape_index = -1;
          for (u32 mesh_bs_idx = 0;
               mesh_bs_idx < target_mesh->blendshape_names.len; mesh_bs_idx++) {
            if (str_equal(
                    target_mesh->blendshape_names.items[mesh_bs_idx].value,
                    asset_bs_name->value)) {
              blendshape_index = mesh_bs_idx;
              break;
            }
          }

          if (blendshape_index >= 0) {
            bs_keyframe->blendshape_indices[valid_blendshapes] =
                blendshape_index;
            bs_keyframe->blendshape_values[valid_blendshapes] =
                asset_bs_keyframe->blendshape_values[bs_idx];
            valid_blendshapes++;
          } else {
            LOG_WARN("Blendshape '%' not found in mesh '%', skipping",
                     FMT_STR(asset_bs_name->value),
                     FMT_STR(asset_bs_keyframe->mesh_name.value));
          }
        }

        // Resize arrays to actual valid blendshapes count
        bs_keyframe->blendshape_len = valid_blendshapes;
      } else {
        bs_keyframe->blendshape_indices = NULL;
        bs_keyframe->blendshape_values = NULL;
      }
    } else {
      LOG_WARN("Mesh '%' not found in model, skipping blendshape keyframe",
               FMT_STR(asset_bs_keyframe->mesh_name.value));
      // Create empty blendshape transforms array
      runtime_bs_keyframe->blendshape_transforms =
          arr_new_zero(BlendshapeKeyframe);
    }
  }

  // Calculate animation length from both keyframe types
  f32 max_skeletal_time = 0.0f;
  f32 max_blendshape_time = 0.0f;

  i32 last_skeletal_idx = animation_asset->keyframes.len - 1;
  if (last_skeletal_idx >= 0) {
    max_skeletal_time =
        animation_asset->keyframes.items[last_skeletal_idx].timestamp;
  }

  i32 last_blendshape_idx = animation_asset->blendshape_keyframes.len - 1;
  if (last_blendshape_idx >= 0) {
    max_blendshape_time =
        animation_asset->blendshape_keyframes.items[last_blendshape_idx]
            .timestamp;
  }

  animation->length = fmaxf(max_skeletal_time, max_blendshape_time);

  return animation;
}

void joint_transforms_to_matrices(JointTransform_Array joint_transforms,
                                  Model3DData *model,
                                  _out_ mat4_Array joint_matrices) {
  assert(model);
  assert(joint_matrices.len >= model->len_joints);

  // convert joint transforms to local matrices
  for (u32 i = 0; i < model->len_joints && i < joint_transforms.len; i++) {
    JointTransform *transform = &joint_transforms.items[i];
    mat4 *joint_mat = &joint_matrices.items[i];

    mat_tr(transform->translation, transform->rotation, *joint_mat);
  }

  // apply hierarchy
  Joint_Array model_joints =
      arr_from_c_array(Joint, model->joints, model->len_joints);
  apply_joint_transform_recursive(model_joints, joint_matrices, 0,
                                  MAT4_IDENTITY);
}

#if defined(__wasm__)
extern bool32 _renderer_create_animation_texture(f32 *animation_data,
                                                 u32 animation_data_len,
                                                 u32 num_keyframes,
                                                 _out_ Handle *handle);

bool32 renderer_create_animation_texture(GameContext *ctx, Animation *animation,
                                         _out_ Handle *handle) {
  debug_assert(ctx);
  debug_assert(animation);
  debug_assert(handle);
  if (!ctx || !animation || !handle) {
    return false;
  }

  // notes on animation texture format:
  // each row is a complete keyframe pose, starting with timestamp, then
  // j0_translation, j0_rotation, j1_translation, j2_rotation, ...
  // jn_translation, jn_rotation 1st Texel: timestamp + translation (xyz) 2nd
  // we add padding to the translation, so coordinates are alreays yzw
  // Texel: rotation (xyzw) 3rd texel: padding + translation (xyz) 4th texel:
  // rotation (xyzw) 5th texel: padding + translation (xyz) 6th texel: rotation
  // (xyzw)
  // ... and so on
  u32 num_keyframes = animation->keyframes.len;
  u32 buffer_size = num_keyframes * MAX_JOINTS * 2 *
                    4; // 2 texels per joint, 4 floats per texel

  f32 *buffer = ALLOC_ARRAY(&ctx->temp_allocator, f32, buffer_size);

  u32 buffer_idx = 0;
  for (u32 keyframe_idx = 0; keyframe_idx < num_keyframes; keyframe_idx++) {
    Keyframe *keyframe = &animation->keyframes.items[keyframe_idx];
    u32 transform_idx = 0;
    u32 joints_len = glm_min(keyframe->joint_transforms.len, MAX_JOINTS);
    for (; transform_idx < joints_len; transform_idx++) {
      KeyframeJointTransform *transform =
          &keyframe->joint_transforms.items[transform_idx];

      // note: we assume every keyframe has all joints, in order
      debug_assert_msg(
          transform_idx == transform->index,
          "keyframe % transform data out of order. Expected % got %",
          FMT_UINT(keyframe_idx), FMT_UINT(transform_idx),
          FMT_UINT(transform->index));

      buffer[buffer_idx++] = keyframe->timestamp;
      buffer[buffer_idx++] = transform->translation[0];
      buffer[buffer_idx++] = transform->translation[1];
      buffer[buffer_idx++] = transform->translation[2];

      buffer[buffer_idx++] = transform->rotation[0];
      buffer[buffer_idx++] = transform->rotation[1];
      buffer[buffer_idx++] = transform->rotation[2];
      buffer[buffer_idx++] = transform->rotation[3];
    }
    buffer_idx += 8 * (MAX_JOINTS - transform_idx);
  }

  return _renderer_create_animation_texture(buffer, buffer_size, num_keyframes,
                                            handle);
}
#endif
