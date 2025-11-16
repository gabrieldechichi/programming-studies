#ifndef H_ANIMATION
#define H_ANIMATION

#include "context.h"
#include "lib/array.h"
#include "lib/common.h"
#include "lib/handle.h"
#include "lib/math.h"
#include "lib/memory.h"
#include "lib/typedefs.h"
#include "renderer/renderer.h"

typedef struct {
  String name;
  vec3 translation;
  quaternion rotation;
  vec3 scale;
} KeyframeAssetJointTransform;
arr_define(KeyframeAssetJointTransform);
slice_define(KeyframeAssetJointTransform);

typedef struct {
  f32 timestamp;
  KeyframeAssetJointTransform_Array joint_transforms;
} KeyframeAsset;
arr_define(KeyframeAsset);
slice_define(KeyframeAsset);

typedef struct {
  f32 timestamp;
  String mesh_name;

  u32 blendshape_len;
  String *blendshape_names;
  f32 *blendshape_values;
} BlendshapeKeyframeAsset;
arr_define(BlendshapeKeyframeAsset);
slice_define(BlendshapeKeyframeAsset);

typedef struct {
  String name;
  KeyframeAsset_Array keyframes;
  BlendshapeKeyframeAsset_Array blendshape_keyframes;
} AnimationAsset;

typedef struct {
  u32 index;
  vec3 translation;
  quaternion rotation;
  vec3 scale;
} KeyframeJointTransform;
arr_define(KeyframeJointTransform);

// Joint transform for blending (no index, just transform data)
typedef struct {
  vec3 translation;
  quaternion rotation;
} JointTransform;
arr_define(JointTransform);

// Blendshape evaluation result for blending
typedef struct {
  u32 mesh_index;
  f32_Array blendshape_weights;
} BlendshapeEvalResult;
arr_define(BlendshapeEvalResult);

// runtime blendshape keyframe (indices instead of names)
typedef struct {
  u32 mesh_index; // Index into Model3DData.meshes
  u32 blendshape_len;
  u32 *blendshape_indices; // Indices into MeshData.blendshape_names
  f32 *blendshape_values;  // Weight values
} BlendshapeKeyframe;
arr_define(BlendshapeKeyframe);

// runtime keyframe, mapped precisely to a skeleton
typedef struct {
  // TODO: maybe keyframe data should be SOA
  f32 timestamp;
  KeyframeJointTransform_Array joint_transforms;
} Keyframe;
arr_define(Keyframe);

// runtime blendshape keyframe container
typedef struct {
  f32 timestamp;
  BlendshapeKeyframe_Array blendshape_transforms;
} RuntimeBlendshapeKeyframe;
arr_define(RuntimeBlendshapeKeyframe);

// runtime animation, tied to a specific model. Keyframes match skeleton 1-1
typedef struct {
  // todo: use handle
  Model3DData *model;
  String name;
  f32 length;
  Keyframe_Array keyframes;                             // skeletal keyframes
  RuntimeBlendshapeKeyframe_Array blendshape_keyframes; // blendshape keyframes
} Animation;

typedef struct {
  b32 loop;
  f32 time;
  f32 speed;
  f32 weight;
  Animation *animation;
} AnimationState;
arr_define(AnimationState);
slice_define(AnimationState);

void animation_update(AnimationState *animation, f32 dt);

void animation_evaluate(AnimationState *animation,
                        _out_ JointTransform_Array joint_transforms);

void animation_evaluate_blendshapes(AnimationState *animation,
                                    Model3DData *model,
                                    _out_ BlendshapeEvalResult_Array results);

bool32 animation_asset_write(const AnimationAsset *animation,
                             Allocator *allocator, _out_ u8_Array *buffer);
AnimationAsset *animation_asset_read(const u8_Array binary_data,
                                     Allocator *allocator);

Animation *animation_from_asset(const AnimationAsset *animation_asset,
                                Model3DData *model, Allocator *allocator);

void joint_transforms_to_matrices(JointTransform_Array joint_transforms,
                                  Model3DData *model,
                                  _out_ mat4_Array joint_matrices);

bool32 renderer_create_animation_texture(GameContext *ctx, Animation *animation,
                                         _out_ Handle *handle);
#endif
