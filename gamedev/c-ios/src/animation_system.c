#include "animation_system.h"
#include "lib/array.h"
#include "lib/fmt.h"
#include "lib/math.h"
#include "lib/typedefs.h"

#define ANIMATIONS_CAP 64
#define LAYERS_CAP 8

void animated_entity_init(AnimatedEntity *entity, Model3DData *model,
                          Allocator *allocator) {
  u32 num_joints = model->len_joints;
  entity->final_pose = arr_new_ALLOC(allocator, JointTransform, num_joints);
  entity->temp_pose = arr_new_ALLOC(allocator, JointTransform, num_joints);
  entity->layer_pose = arr_new_ALLOC(allocator, JointTransform, num_joints);

  u32 num_meshes = model->num_meshes;
  entity->blendshape_results =
      arr_new_ALLOC(allocator, BlendshapeEvalResult, num_meshes);

  for (u32 mesh_idx = 0; mesh_idx < num_meshes; mesh_idx++) {
    BlendshapeEvalResult *result = &entity->blendshape_results.items[mesh_idx];
    result->mesh_index = mesh_idx;
    u32 num_blendshapes = model->meshes[mesh_idx].blendshape_names.len;
    result->blendshape_weights = arr_new_ALLOC(allocator, f32, num_blendshapes);
  }

  entity->layers = slice_new_ALLOC(allocator, AnimationLayer, LAYERS_CAP);

  // Create default layer with all joints enabled
  SkeletonMask default_mask = skeleton_mask_create_all(allocator, num_joints);
  animated_entity_add_layer(entity, STR_FROM_CSTR("Default"), default_mask,
                            1.0f, allocator);
}

void animated_entity_play_animation(AnimatedEntity *entity,
                                    Animation *animation,
                                    f32 transition_duration, f32 speed,
                                    bool32 loop) {
  // Play on default layer (index 0)
  animated_entity_play_animation_on_layer(entity, 0, animation,
                                          transition_duration, speed, loop);
}

void animated_entity_update(AnimatedEntity *entity, f32 dt) {
  if (entity->layers.len == 0) {
    return;
  }

  // Update each layer
  for (u32 layer_idx = 0; layer_idx < entity->layers.len; layer_idx++) {
    AnimationLayer *layer = &entity->layers.items[layer_idx];

    if (layer->animation_states.len == 0) {
      continue;
    }

    // handle transition for this layer
    if (layer->current_transition.active) {
      layer->current_transition.elapsed += dt;

      debug_assert(layer->current_transition.to_index <
                   layer->animation_states.len);

      u32 target_state_idx = layer->current_transition.to_index;

      f32 target_state_weight =
          layer->current_transition.duration > 0
              ? clamp01(layer->current_transition.elapsed /
                        layer->current_transition.duration)
              : 1.0f;

      // normalize weights
      {
        f32 sum_weights = 0.0f;
        for (u32 i = 0; i < layer->animation_states.len; i++) {
          if (i != target_state_idx) {
            sum_weights += layer->animation_states.items[i].weight;
          }
        }

        f32 remaining_weight = 1.0f - target_state_weight;
        f32 inverse_sum_weights =
            sum_weights > 0.0f ? remaining_weight / sum_weights : 0.0f;
        for (u32 i = 0; i < layer->animation_states.len; i++) {
          if (i != target_state_idx) {
            layer->animation_states.items[i].weight *= inverse_sum_weights;
          } else {
            layer->animation_states.items[i].weight = target_state_weight;
          }
        }
      }

      b32 transition_ended = layer->current_transition.elapsed >=
                             layer->current_transition.duration;
      if (transition_ended) {
        layer->current_transition.elapsed = layer->current_transition.duration;
        layer->current_animation_index = layer->current_transition.to_index;
        layer->current_transition.active = false;
      }
    }

    // cleanup zero-weight states
    if (!layer->current_transition.active) {
      for (i32 i = (i32)layer->animation_states.len - 1; i >= 0; i--) {
        AnimationState *state = &layer->animation_states.items[i];
        bool32 is_current = ((u32)i == layer->current_animation_index);

        if (approximately(state->weight, 0.0f) && !is_current) {
          slice_remove_swap(layer->animation_states, i);

          // Update indices since slice_remove_swap can invalidate them
          if (layer->current_animation_index == layer->animation_states.len) {
            layer->current_animation_index = i;
          }
        }
      }
    }

    // update animations in this layer
    for (u32 i = 0; i < layer->animation_states.len; i++) {
      animation_update(&layer->animation_states.items[i], dt);
    }
  }
}

static void blend_layer_to_final(JointTransform_Array layer_pose,
                                 JointTransform_Array final_pose,
                                 SkeletonMask *mask, f32 weight) {
  for (u32 i = 0; i < mask->enabled_joints.len; i++) {
    u32 joint_idx = mask->enabled_joints.items[i];

    if (joint_idx >= final_pose.len) {
      continue;
    }

    if (weight >= 1.0f) {
      final_pose.items[joint_idx] = layer_pose.items[joint_idx];
    } else if (weight > 0.0f) {
      glm_vec3_lerp(final_pose.items[joint_idx].translation,
                    layer_pose.items[joint_idx].translation, weight,
                    final_pose.items[joint_idx].translation);

      glm_quat_nlerp(final_pose.items[joint_idx].rotation,
                     layer_pose.items[joint_idx].rotation, weight,
                     final_pose.items[joint_idx].rotation);
    }
  }
}

static void evaluate_layer_animations(AnimationLayer *layer,
                                      JointTransform_Array layer_pose,
                                      JointTransform_Array temp_pose) {
  if (layer->animation_states.len == 0) {
    return;
  }

  u32 num_joints = layer_pose.len;
  bool32 did_evaluate_any = false;

  for (u32 anim_idx = 0; anim_idx < layer->animation_states.len; anim_idx++) {
    AnimationState *anim_state = &layer->animation_states.items[anim_idx];

    if (anim_state->weight <= 0.0f) {
      continue;
    }

    // skip animations with no joint data (blendshape only)
    if (anim_state->animation->keyframes.len <= 0) {
      continue;
    }

    if (!did_evaluate_any) {
      animation_evaluate(anim_state, layer_pose);
      did_evaluate_any = true;
    } else {
      animation_evaluate(anim_state, temp_pose);
      for (u32 joint_idx = 0; joint_idx < num_joints; joint_idx++) {
        glm_vec3_add(layer_pose.items[joint_idx].translation,
                     temp_pose.items[joint_idx].translation,
                     layer_pose.items[joint_idx].translation);

        quat_add_shortest_path(layer_pose.items[joint_idx].rotation,
                               temp_pose.items[joint_idx].rotation,
                               layer_pose.items[joint_idx].rotation);
      }
    }
  }
}

void animated_entity_evaluate_pose(AnimatedEntity *entity, Model3DData *model) {
  if (entity->layers.len == 0) {
    return;
  }

  u32 num_joints = model->len_joints;

  // Initialize final pose to identity/bind pose
  for (u32 i = 0; i < num_joints; i++) {
    entity->final_pose.items[i] =
        (JointTransform){.translation = {0.0f, 0.0f, 0.0f},
                         .rotation = {0.0f, 0.0f, 0.0f, 1.0f}};
  }

  // Evaluate each layer and blend into final pose
  for (u32 layer_idx = 0; layer_idx < entity->layers.len; layer_idx++) {
    AnimationLayer *layer = &entity->layers.items[layer_idx];

    if (layer->layer_weight <= 0.0f || layer->animation_states.len == 0) {
      continue;
    }

    // Clear layer pose
    for (u32 i = 0; i < num_joints; i++) {
      entity->layer_pose.items[i] =
          (JointTransform){.translation = {0.0f, 0.0f, 0.0f},
                           .rotation = {0.0f, 0.0f, 0.0f, 1.0f}};
    }

    // Evaluate this layer's animations
    evaluate_layer_animations(layer, entity->layer_pose, entity->temp_pose);

    // Blend layer into final pose
    blend_layer_to_final(entity->layer_pose, entity->final_pose,
                         &layer->skeleton_mask, layer->layer_weight);
  }

  // evaluate blendshapes (same as before, but iterate through layers)
  {
    BlendshapeEvalResult_Array blendshape_results = entity->blendshape_results;
    arr_foreach_ptr(blendshape_results, b) {
      memset(b->blendshape_weights.items, 0,
             sizeof(f32) * b->blendshape_weights.len);
    }

    for (u32 layer_idx = 0; layer_idx < entity->layers.len; layer_idx++) {
      AnimationLayer *layer = &entity->layers.items[layer_idx];

      for (u32 anim_idx = 0; anim_idx < layer->animation_states.len;
           anim_idx++) {
        AnimationState *anim_state = &layer->animation_states.items[anim_idx];
        animation_evaluate_blendshapes(anim_state, model, blendshape_results);
      }
    }

    entity->blendshape_results = blendshape_results;
  }
}

void animated_entity_apply_pose(AnimatedEntity *entity, Model3DData *model,
                                SkinnedModel *skinned_model) {
  if (entity->layers.len == 0) {
    return;
  }

  joint_transforms_to_matrices(entity->final_pose, model,
                               skinned_model->joint_matrices);

  // apply blendshapes
  u32 num_meshes = model->num_meshes;
  for (u32 mesh_idx = 0;
       mesh_idx < num_meshes && mesh_idx < skinned_model->meshes.len;
       mesh_idx++) {
    BlendshapeEvalResult *result = &entity->blendshape_results.items[mesh_idx];
    SkinnedMesh *mesh = &skinned_model->meshes.items[mesh_idx];

    for (u32 bs_idx = 0; bs_idx < result->blendshape_weights.len &&
                         bs_idx < mesh->blendshape_weights.len;
         bs_idx++) {
      mesh->blendshape_weights.items[bs_idx] =
          clamp(result->blendshape_weights.items[bs_idx], 0.0f, 1.0f);
    }
  }
}

AnimationState *animated_entity_current_state(AnimatedEntity *entity) {
  if (entity->layers.len == 0) {
    return NULL;
  }
  AnimationLayer *default_layer = &entity->layers.items[0];
  if (default_layer->current_animation_index < 0 ||
      default_layer->current_animation_index >=
          default_layer->animation_states.len) {
    return NULL;
  }
  return &default_layer->animation_states
              .items[default_layer->current_animation_index];
}

SkeletonMask skeleton_mask_create_all(Allocator *allocator, u32 num_joints) {
  SkeletonMask mask = {0};
  mask.enabled_joints = arr_new_ALLOC(allocator, u32, num_joints);
  for (u32 i = 0; i < num_joints; i++) {
    mask.enabled_joints.items[i] = i;
  }
  return mask;
}

SkeletonMask skeleton_mask_create_from_joints(Allocator *allocator,
                                              u32 *joint_indices,
                                              u32 num_indices) {
  SkeletonMask mask = {0};
  mask.enabled_joints = arr_new_ALLOC(allocator, u32, num_indices);
  for (u32 i = 0; i < num_indices; i++) {
    mask.enabled_joints.items[i] = joint_indices[i];
  }
  return mask;
}

u32 animated_entity_add_layer(AnimatedEntity *entity, String name,
                              SkeletonMask mask, f32 weight,
                              Allocator *allocator) {
  AnimationLayer layer = {0};
  layer.animation_states =
      slice_new_ALLOC(allocator, AnimationState, ANIMATIONS_CAP);
  layer.current_transition = (AnimationTransition){0};
  layer.current_animation_index = 0;
  layer.skeleton_mask = mask;
  layer.layer_weight = weight;
  layer.name = name;

  slice_append(entity->layers, layer);
  return entity->layers.len - 1;
}

void animated_entity_remove_layer(AnimatedEntity *entity, i32 layer_index) {
  if (layer_index < (i32)entity->layers.len && layer_index > 0) {
    slice_remove_swap(entity->layers, layer_index);
  }
}

void animated_entity_play_animation_on_layer(AnimatedEntity *entity,
                                             u32 layer_index,
                                             Animation *animation,
                                             f32 transition_duration, f32 speed,
                                             bool32 loop) {
    UNUSED(speed);
  if (layer_index >= entity->layers.len || !animation) {
    return;
  }

  AnimationLayer *layer = &entity->layers.items[layer_index];

  AnimationState *current_state = NULL;
  if (layer->current_animation_index < layer->animation_states.len) {
    current_state =
        &layer->animation_states.items[layer->current_animation_index];
  }

  LOG_INFO("Layer %: Transitioning from % to %. Transition active % (%)",
           FMT_STR(layer->name.value),
           FMT_STR(current_state ? current_state->animation->name.value : ""),
           FMT_STR(animation->name.value),
           FMT_UINT(layer->current_transition.active),
           FMT_FLOAT(layer->current_transition.active
                         ? (layer->current_transition.elapsed /
                            layer->current_transition.duration)
                         : 0.0));

  AnimationState new_state = {
      .animation = animation,
      .speed = 1.0,
      .weight = 0.0f,
      .time = 0.0f,
      .loop = loop,
  };
  slice_append(layer->animation_states, new_state);
  u32 target_index = layer->animation_states.len - 1;

  if (layer->animation_states.len == 1) {
    layer->animation_states.items[target_index].weight = 1.0f;
    layer->current_animation_index = target_index;
    layer->current_transition.active = false;
  } else {
    layer->current_transition =
        (AnimationTransition){.to_index = target_index,
                              .duration = transition_duration,
                              .elapsed = 0.0f,
                              .active = true};
  }
}

void animated_entity_set_layer_weight(AnimatedEntity *entity, u32 layer_index,
                                      f32 weight) {
  if (layer_index < entity->layers.len) {
    entity->layers.items[layer_index].layer_weight = weight;
  }
}

SkeletonMask skeleton_mask_create_from_joint_names(Allocator *allocator,
                                                   Model3DData *model,
                                                   String *joint_names,
                                                   u32 num_names) {
  SkeletonMask mask = {0};

  // First pass: count how many joints we found
  u32 found_joints = 0;
  for (u32 name_idx = 0; name_idx < num_names; name_idx++) {
    for (u32 joint_idx = 0; joint_idx < model->len_joints; joint_idx++) {
      if (model->joint_names[joint_idx].len == joint_names[name_idx].len &&
          str_equal(model->joint_names[joint_idx].value,
                    joint_names[name_idx].value)) {
        found_joints++;
        break;
      }
    }
  }

  if (found_joints == 0) {
    mask.enabled_joints = arr_new_ALLOC(allocator, u32, 0);
    return mask;
  }

  // Second pass: store the joint indices
  mask.enabled_joints = arr_new_ALLOC(allocator, u32, found_joints);
  u32 current_idx = 0;

  for (u32 name_idx = 0; name_idx < num_names; name_idx++) {
    for (u32 joint_idx = 0; joint_idx < model->len_joints; joint_idx++) {
      if (model->joint_names[joint_idx].len == joint_names[name_idx].len &&
          str_equal(model->joint_names[joint_idx].value,
                    joint_names[name_idx].value)) {
        mask.enabled_joints.items[current_idx] = joint_idx;
        current_idx++;
        break;
      }
    }
  }

  return mask;
}
