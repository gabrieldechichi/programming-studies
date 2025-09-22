#ifndef H_ANIMATION_SYSTEM
#define H_ANIMATION_SYSTEM

#include "animation.h"
#include "lib/array.h"
#include "lib/string.h"
#include "lib/typedefs.h"
#include "renderer/renderer.h"

typedef struct {
  u32 to_index;
  f32 duration;
  f32 elapsed;
  bool32 active;
} AnimationTransition;

typedef struct {
  u32_Array enabled_joints;
} SkeletonMask;

typedef struct {
  AnimationState_Slice animation_states;
  AnimationTransition current_transition;
  u32 current_animation_index;
  SkeletonMask skeleton_mask;
  f32 layer_weight;
  String name;
} AnimationLayer;

slice_define(AnimationLayer);

typedef struct {
  AnimationLayer_Slice layers;
  JointTransform_Array final_pose;
  JointTransform_Array temp_pose;
  JointTransform_Array layer_pose;
  BlendshapeEvalResult_Array blendshape_results;
} AnimatedEntity;

void animated_entity_init(AnimatedEntity *entity, Model3DData *model,
                          Allocator *allocator);

void animated_entity_update(AnimatedEntity *entity, f32 dt);

void animated_entity_play_animation(AnimatedEntity *entity,
                                    Animation *animation,
                                    f32 transition_duration, f32 speed,
                                    bool32 loop);

void animated_entity_evaluate_pose(AnimatedEntity *entity, Model3DData *model);
void animated_entity_apply_pose(AnimatedEntity *entity, Model3DData *model,
                                SkinnedModel *skinned_model);

AnimationState *animated_entity_current_state(AnimatedEntity *entity);

SkeletonMask skeleton_mask_create_all(Allocator *allocator, u32 num_joints);
SkeletonMask skeleton_mask_create_from_joints(Allocator *allocator,
                                              u32 *joint_indices,
                                              u32 num_indices);
SkeletonMask skeleton_mask_create_from_joint_names(Allocator *allocator,
                                                   Model3DData *model,
                                                   String *joint_names,
                                                   u32 num_names);

u32 animated_entity_add_layer(AnimatedEntity *entity, String name,
                              SkeletonMask mask, f32 weight,
                              Allocator *allocator);
void animated_entity_remove_layer(AnimatedEntity *entity, i32 layer_index);

void animated_entity_play_animation_on_layer(AnimatedEntity *entity,
                                             u32 layer_index,
                                             Animation *animation,
                                             f32 transition_duration, f32 speed,
                                             bool32 loop);

void animated_entity_set_layer_weight(AnimatedEntity *entity, u32 layer_index,
                                      f32 weight);

#endif // !H_ANIMATION_SYSTEM
