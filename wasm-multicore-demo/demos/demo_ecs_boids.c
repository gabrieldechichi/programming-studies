#include "context.h"
#include "lib/thread_context.h"
#include "lib/typedefs.h"
#include "os/os.h"
#include "lib/math.h"
#include "lib/hash.h"
#include "lib/random.h"
#include "renderer.h"
#include "camera.h"
#include "flycam.h"
#include "input.h"
#include "app.h"
#include "mesh.h"
#include "assets.h"
#include "shaders/fish_vs.h"
#include "shaders/fish_instanced_vs.h"
#include "shaders/fish_fs.h"

// #define NUM_BOIDS 100000
#define NUM_BOIDS 25000
#define NUM_TARGETS 2
#define NUM_OBSTACLES 1

#define GRID_SIZE (8192)
#define MAX_PER_BUCKET 256
#define CELL_SIZE 8.0f

#define BOID_SEPARATION_WEIGHT 1.0f
#define BOID_ALIGNMENT_WEIGHT 1.0f
#define BOID_TARGET_WEIGHT 2.0f
#define BOID_OBSTACLE_AVERSION_DISTANCE 30.0f
#define BOID_MOVE_SPEED 25.0f
// #define BOID_MOVE_SPEED 0.5f

typedef struct {
  f32 x, y, z;
} Position;
typedef struct {
  f32 x, y, z;
} Heading;
typedef struct {
  u32 index;
} BoidIndex;
typedef struct {
  u8 dummy;
} BoidTag;

typedef struct {
  f32 sample_rate;
  i32 frame_count;
  const vec3 *positions;
  const versor *rotations;
} SampledAnimationClip;

typedef struct {
  const SampledAnimationClip *clip;
  f32 current_time;
  vec3 *dest_position;
} AnimationPlayer;

typedef struct {
  mat4 value;
} LocalToWorld;

typedef struct {
  f32 x, y, z;
} Scale;

typedef struct {
  GpuMesh_Handle mesh;
  Material_Handle material;
} MeshRenderer;

typedef struct {
  f32 px, py, pz;
  f32 hx, hy, hz;
} BoidBucketEntry;

typedef struct {
  u32 count;
  f32 sum_align_x, sum_align_y, sum_align_z;
  f32 sum_sep_x, sum_sep_y, sum_sep_z;
  i32 nearest_target_idx;
  i32 nearest_obstacle_idx;
  f32 nearest_obstacle_dist;
  BoidBucketEntry entries[MAX_PER_BUCKET];
} BoidBucket;

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

#include "./Shark_animation.c"
#include "./Target01_animation.c"
#include "./Target02_animation.c"

typedef struct {
  EcsWorld world;
  AssetSystem assets;

  InputSystem input;
  Camera camera;
  FlyCameraCtrl fly_cam;

  GpuMesh_Handle fish_mesh;
  Material_Handle fish_material;

  GpuTexture fish_albedo_tex;
  GpuTexture fish_tint_tex;
  GpuTexture fish_metallic_gloss_tex;
  GpuTexture shark_albedo_tex;
  GpuTexture shark_metallic_gloss_tex;

  vec3 target_positions[NUM_TARGETS];
  vec3 obstacle_positions[NUM_OBSTACLES];
  EcsEntity target_entities[NUM_TARGETS];
  EcsEntity obstacle_entities[NUM_OBSTACLES];
  BoidBucket buckets[GRID_SIZE];
  mat4 instance_data[NUM_BOIDS];
  InstanceBuffer_Handle instance_buffer;

  f32 total_time;
} GameState;

global GameState state = {0};

ECS_COMPONENT_DECLARE(MeshRenderer);
ECS_COMPONENT_DECLARE(Scale);
ECS_COMPONENT_DECLARE(LocalToWorld);

void on_fish_loaded(Handle asset, void *data, void *user_data) {
  UNUSED(asset);
  UNUSED(user_data);

  AppContext *app_ctx = app_ctx_current();
  Allocator alloc = make_arena_allocator(&app_ctx->arena);

  ModelBlobAsset *model = (ModelBlobAsset *)data;
  assert(model->mesh_count == 1);
  MeshBlobAsset *mesh_asset = blob_array_get(MeshBlobAsset, model, model->meshes);

  MeshDesc mesh_desc = mesh_asset_to_mesh(mesh_asset, &alloc);
  state.fish_mesh = renderer_upload_mesh(&mesh_desc);

  state.fish_material = renderer_create_material(&(MaterialDesc){
      .shader_desc =
          (GpuShaderDesc){
              .vs_code = (const char *)fish_instanced_vs,
              .fs_code = (const char *)fish_fs,
              .uniform_blocks =
                  FIXED_ARRAY_DEFINE(GpuUniformBlockDesc,
                                     {.stage = GPU_STAGE_VERTEX_FRAGMENT,
                                      .size = sizeof(GlobalUniforms),
                                      .binding = 0},
                                     {.stage = GPU_STAGE_VERTEX_FRAGMENT,
                                      .size = sizeof(MaterialUniforms),
                                      .binding = 1}, ),
              .storage_buffers = FIXED_ARRAY_DEFINE(GpuStorageBufferDesc,
                                                    {.stage = GPU_STAGE_VERTEX,
                                                     .binding = 0,
                                                     .readonly = true}, ),
              .texture_bindings = FIXED_ARRAY_DEFINE(
                  GpuTextureBindingDesc, GPU_TEXTURE_BINDING_FRAG(1, 0),
                  GPU_TEXTURE_BINDING_FRAG(3, 2),
                  GPU_TEXTURE_BINDING_FRAG(5, 4), ),
          },
      .vertex_layout = STATIC_MESH_VERTEX_LAYOUT,
      .primitive = GPU_PRIMITIVE_TRIANGLES,
      .depth_test = true,
      .depth_write = true,
      .properties = FIXED_ARRAY_DEFINE(
          MaterialPropertyDesc,
          {.name = "albedo", .type = MAT_PROP_TEXTURE, .binding = 0},
          {.name = "tint", .type = MAT_PROP_TEXTURE, .binding = 1},
          {.name = "metallic_gloss", .type = MAT_PROP_TEXTURE, .binding = 2},
          {.name = "tint_color",
           .type = MAT_PROP_VEC4,
           .binding = 1,
           .offset = offsetof(MaterialUniforms, tint_color)},
          {.name = "tint_offset",
           .type = MAT_PROP_FLOAT,
           .binding = 1,
           .offset = offsetof(MaterialUniforms, tint_offset)},
          {.name = "metallic",
           .type = MAT_PROP_FLOAT,
           .binding = 1,
           .offset = offsetof(MaterialUniforms, metallic)},
          {.name = "smoothness",
           .type = MAT_PROP_FLOAT,
           .binding = 1,
           .offset = offsetof(MaterialUniforms, smoothness)},
          {.name = "wave_frequency",
           .type = MAT_PROP_FLOAT,
           .binding = 1,
           .offset = offsetof(MaterialUniforms, wave_frequency)},
          {.name = "wave_speed",
           .type = MAT_PROP_FLOAT,
           .binding = 1,
           .offset = offsetof(MaterialUniforms, wave_speed)},
          {.name = "wave_distance",
           .type = MAT_PROP_FLOAT,
           .binding = 1,
           .offset = offsetof(MaterialUniforms, wave_distance)},
          {.name = "wave_offset",
           .type = MAT_PROP_FLOAT,
           .binding = 1,
           .offset = offsetof(MaterialUniforms, wave_offset)}, ),
  });

  material_set_texture(state.fish_material, "albedo", state.fish_albedo_tex);
  material_set_texture(state.fish_material, "tint", state.fish_tint_tex);
  material_set_texture(state.fish_material, "metallic_gloss",
                       state.fish_metallic_gloss_tex);
  material_set_vec4(state.fish_material, "tint_color",
                    (vec4){1.0f, 1.0f, 1.0f, 1.0f});
  material_set_float(state.fish_material, "tint_offset", 0.0f);
  material_set_float(state.fish_material, "metallic", 0.636f);
  material_set_float(state.fish_material, "smoothness", 0.848f);
  material_set_float(state.fish_material, "wave_frequency", 0.03f);
  material_set_float(state.fish_material, "wave_speed", 10.0f);
  material_set_float(state.fish_material, "wave_distance", 5.0f);
  material_set_float(state.fish_material, "wave_offset", 0.0f);

  Material_Handle fish_noninst_material = renderer_create_material(&(
      MaterialDesc){
      .shader_desc =
          (GpuShaderDesc){
              .vs_code = (const char *)fish_vs,
              .fs_code = (const char *)fish_fs,
              .uniform_blocks =
                  FIXED_ARRAY_DEFINE(GpuUniformBlockDesc,
                                     {.stage = GPU_STAGE_VERTEX_FRAGMENT,
                                      .size = sizeof(GlobalUniforms),
                                      .binding = 0},
                                     {.stage = GPU_STAGE_VERTEX_FRAGMENT,
                                      .size = sizeof(MaterialUniforms),
                                      .binding = 1}, ),
              .texture_bindings = FIXED_ARRAY_DEFINE(
                  GpuTextureBindingDesc, GPU_TEXTURE_BINDING_FRAG(1, 0),
                  GPU_TEXTURE_BINDING_FRAG(3, 2),
                  GPU_TEXTURE_BINDING_FRAG(5, 4), ),
          },
      .vertex_layout = STATIC_MESH_VERTEX_LAYOUT,
      .primitive = GPU_PRIMITIVE_TRIANGLES,
      .depth_test = true,
      .depth_write = true,
      .properties = FIXED_ARRAY_DEFINE(
          MaterialPropertyDesc,
          {.name = "albedo", .type = MAT_PROP_TEXTURE, .binding = 0},
          {.name = "tint", .type = MAT_PROP_TEXTURE, .binding = 1},
          {.name = "metallic_gloss", .type = MAT_PROP_TEXTURE, .binding = 2},
          {.name = "tint_color",
           .type = MAT_PROP_VEC4,
           .binding = 1,
           .offset = offsetof(MaterialUniforms, tint_color)},
          {.name = "tint_offset",
           .type = MAT_PROP_FLOAT,
           .binding = 1,
           .offset = offsetof(MaterialUniforms, tint_offset)},
          {.name = "metallic",
           .type = MAT_PROP_FLOAT,
           .binding = 1,
           .offset = offsetof(MaterialUniforms, metallic)},
          {.name = "smoothness",
           .type = MAT_PROP_FLOAT,
           .binding = 1,
           .offset = offsetof(MaterialUniforms, smoothness)},
          {.name = "wave_frequency",
           .type = MAT_PROP_FLOAT,
           .binding = 1,
           .offset = offsetof(MaterialUniforms, wave_frequency)},
          {.name = "wave_speed",
           .type = MAT_PROP_FLOAT,
           .binding = 1,
           .offset = offsetof(MaterialUniforms, wave_speed)},
          {.name = "wave_distance",
           .type = MAT_PROP_FLOAT,
           .binding = 1,
           .offset = offsetof(MaterialUniforms, wave_distance)},
          {.name = "wave_offset",
           .type = MAT_PROP_FLOAT,
           .binding = 1,
           .offset = offsetof(MaterialUniforms, wave_offset)}, ),
  });

  material_set_texture(fish_noninst_material, "albedo", state.fish_albedo_tex);
  material_set_texture(fish_noninst_material, "tint", state.fish_tint_tex);
  material_set_texture(fish_noninst_material, "metallic_gloss",
                       state.fish_metallic_gloss_tex);
  material_set_vec4(fish_noninst_material, "tint_color",
                    (vec4){1.0f, 1.0f, 1.0f, 1.0f});
  material_set_float(fish_noninst_material, "tint_offset", 0.0f);
  material_set_float(fish_noninst_material, "metallic", 0.636f);
  material_set_float(fish_noninst_material, "smoothness", 0.848f);
  material_set_float(fish_noninst_material, "wave_frequency", 0.03f);
  material_set_float(fish_noninst_material, "wave_speed", 10.0f);
  material_set_float(fish_noninst_material, "wave_distance", 5.0f);
  material_set_float(fish_noninst_material, "wave_offset", 0.0f);

  for (i32 i = 0; i < NUM_TARGETS; i++) {
    MeshRenderer mr = {
        .mesh = state.fish_mesh,
        .material = fish_noninst_material,
    };
    Scale s = {.x = 0.01f, .y = 0.01f, .z = 0.01f};
    LocalToWorld ltw = {0};
    ecs_set_ptr(&state.world, state.target_entities[i], ecs_id(MeshRenderer),
                &mr);
    ecs_set_ptr(&state.world, state.target_entities[i], ecs_id(Scale), &s);
    ecs_set_ptr(&state.world, state.target_entities[i], ecs_id(LocalToWorld),
                &ltw);
  }

  LOG_INFO("Fish model loaded");
}

void on_shark_loaded(Handle asset, void *data, void *user_data) {
  UNUSED(asset);
  UNUSED(user_data);

  AppContext *app_ctx = app_ctx_current();
  Allocator alloc = make_arena_allocator(&app_ctx->arena);

  ModelBlobAsset *model = (ModelBlobAsset *)data;
  assert(model->mesh_count == 1);
  MeshBlobAsset *mesh_asset = blob_array_get(MeshBlobAsset, model, model->meshes);

  MeshDesc mesh_desc = mesh_asset_to_mesh(mesh_asset, &alloc);
  GpuMesh_Handle shark_mesh = renderer_upload_mesh(&mesh_desc);

  Material_Handle shark_material = renderer_create_material(&(MaterialDesc){
      .shader_desc =
          (GpuShaderDesc){
              .vs_code = (const char *)fish_vs,
              .fs_code = (const char *)fish_fs,
              .uniform_blocks =
                  FIXED_ARRAY_DEFINE(GpuUniformBlockDesc,
                                     {.stage = GPU_STAGE_VERTEX_FRAGMENT,
                                      .size = sizeof(GlobalUniforms),
                                      .binding = 0},
                                     {.stage = GPU_STAGE_VERTEX_FRAGMENT,
                                      .size = sizeof(MaterialUniforms),
                                      .binding = 1}, ),
              .texture_bindings = FIXED_ARRAY_DEFINE(
                  GpuTextureBindingDesc, GPU_TEXTURE_BINDING_FRAG(1, 0),
                  GPU_TEXTURE_BINDING_FRAG(3, 2),
                  GPU_TEXTURE_BINDING_FRAG(5, 4), ),
          },
      .vertex_layout = STATIC_MESH_VERTEX_LAYOUT,
      .primitive = GPU_PRIMITIVE_TRIANGLES,
      .depth_test = true,
      .depth_write = true,
      .properties = FIXED_ARRAY_DEFINE(
          MaterialPropertyDesc,
          {.name = "albedo", .type = MAT_PROP_TEXTURE, .binding = 0},
          {.name = "tint", .type = MAT_PROP_TEXTURE, .binding = 1},
          {.name = "metallic_gloss", .type = MAT_PROP_TEXTURE, .binding = 2},
          {.name = "tint_color",
           .type = MAT_PROP_VEC4,
           .binding = 1,
           .offset = offsetof(MaterialUniforms, tint_color)},
          {.name = "tint_offset",
           .type = MAT_PROP_FLOAT,
           .binding = 1,
           .offset = offsetof(MaterialUniforms, tint_offset)},
          {.name = "metallic",
           .type = MAT_PROP_FLOAT,
           .binding = 1,
           .offset = offsetof(MaterialUniforms, metallic)},
          {.name = "smoothness",
           .type = MAT_PROP_FLOAT,
           .binding = 1,
           .offset = offsetof(MaterialUniforms, smoothness)},
          {.name = "wave_frequency",
           .type = MAT_PROP_FLOAT,
           .binding = 1,
           .offset = offsetof(MaterialUniforms, wave_frequency)},
          {.name = "wave_speed",
           .type = MAT_PROP_FLOAT,
           .binding = 1,
           .offset = offsetof(MaterialUniforms, wave_speed)},
          {.name = "wave_distance",
           .type = MAT_PROP_FLOAT,
           .binding = 1,
           .offset = offsetof(MaterialUniforms, wave_distance)},
          {.name = "wave_offset",
           .type = MAT_PROP_FLOAT,
           .binding = 1,
           .offset = offsetof(MaterialUniforms, wave_offset)}, ),
  });

  material_set_texture(shark_material, "albedo", state.shark_albedo_tex);
  material_set_texture(shark_material, "tint", state.fish_tint_tex);
  material_set_texture(shark_material, "metallic_gloss",
                       state.shark_metallic_gloss_tex);
  material_set_vec4(shark_material, "tint_color",
                    (vec4){1.0f, 1.0f, 1.0f, 1.0f});
  material_set_float(shark_material, "tint_offset", 0.0f);
  material_set_float(shark_material, "metallic", 0.063f);
  material_set_float(shark_material, "smoothness", 1.0f);
  material_set_float(shark_material, "wave_frequency", 0.75f);
  material_set_float(shark_material, "wave_speed", 10.0f);
  material_set_float(shark_material, "wave_distance", 5.0f);
  material_set_float(shark_material, "wave_offset", 0.0f);

  for (i32 i = 0; i < NUM_OBSTACLES; i++) {
    MeshRenderer mr = {
        .mesh = shark_mesh,
        .material = shark_material,
    };
    Scale s = {.x = 0.01f, .y = 0.01f, .z = 0.01f};
    LocalToWorld ltw = {0};
    ecs_set_ptr(&state.world, state.obstacle_entities[i], ecs_id(MeshRenderer),
                &mr);
    ecs_set_ptr(&state.world, state.obstacle_entities[i], ecs_id(Scale), &s);
    ecs_set_ptr(&state.world, state.obstacle_entities[i], ecs_id(LocalToWorld),
                &ltw);
  }

  LOG_INFO("Shark model loaded");
}

void sample_animation_position(const SampledAnimationClip *clip, f32 time,
                               vec3 out_pos) {
  f32 duration = clip->sample_rate * (f32)(clip->frame_count - 1);
  while (time >= duration)
    time -= duration;
  if (time < 0.0f)
    time = 0.0f;

  f32 frame_f = time / clip->sample_rate;
  i32 frame0 = (i32)frame_f;
  i32 frame1 = frame0 + 1;
  if (frame1 >= clip->frame_count)
    frame1 = clip->frame_count - 1;
  f32 t = frame_f - (f32)frame0;

  const vec3 *p0 = &clip->positions[frame0];
  const vec3 *p1 = &clip->positions[frame1];
  out_pos[0] = (*p0)[0] + t * ((*p1)[0] - (*p0)[0]);
  out_pos[1] = (*p0)[1] + t * ((*p1)[1] - (*p0)[1]);
  out_pos[2] = (*p0)[2] + t * ((*p1)[2] - (*p0)[2]);
}

void sample_animation_rotation(const SampledAnimationClip *clip, f32 time,
                               versor out_rot) {
  f32 duration = clip->sample_rate * (f32)(clip->frame_count - 1);
  while (time >= duration)
    time -= duration;
  if (time < 0.0f)
    time = 0.0f;

  f32 frame_f = time / clip->sample_rate;
  i32 frame0 = (i32)frame_f;
  i32 frame1 = frame0 + 1;
  if (frame1 >= clip->frame_count)
    frame1 = clip->frame_count - 1;
  f32 t = frame_f - (f32)frame0;

  const versor *q0 = &clip->rotations[frame0];
  const versor *q1 = &clip->rotations[frame1];
  glm_quat_slerp((f32 *)q0, (f32 *)q1, t, out_rot);
}

void PlayAnimationsSystem(EcsIter *it) {
  Position *positions = ecs_field(it, Position, 0);
  AnimationPlayer *players = ecs_field(it, AnimationPlayer, 1);

  f32 dt = it->delta_time;
  if (dt > 0.05f)
    dt = 0.05f;

  for (i32 i = 0; i < it->count; i++) {
    AnimationPlayer *player = &players[i];
    player->current_time += dt;

    f32 duration =
        player->clip->sample_rate * (f32)(player->clip->frame_count - 1);
    while (player->current_time >= duration) {
      player->current_time -= duration;
    }

    vec3 sampled_pos;
    sample_animation_position(player->clip, player->current_time, sampled_pos);

    positions[i].x = sampled_pos[0];
    positions[i].y = sampled_pos[1];
    positions[i].z = sampled_pos[2];

    if (player->dest_position) {
      (*player->dest_position)[0] = sampled_pos[0];
      (*player->dest_position)[1] = sampled_pos[1];
      (*player->dest_position)[2] = sampled_pos[2];
    }
  }
}

void BuildAnimatedTransformSystem(EcsIter *it) {
  Position *positions = ecs_field(it, Position, 0);
  AnimationPlayer *players = ecs_field(it, AnimationPlayer, 1);
  Scale *scales = ecs_field(it, Scale, 2);
  LocalToWorld *transforms = ecs_field(it, LocalToWorld, 3);

  for (i32 i = 0; i < it->count; i++) {
    Position *pos = &positions[i];
    AnimationPlayer *player = &players[i];
    Scale *scale = &scales[i];

    versor anim_rot;
    sample_animation_rotation(player->clip, player->current_time, anim_rot);

    quaternion fish_orient;
    quat_from_euler(VEC3(RAD(90), RAD(180), 0), fish_orient);

    versor rot;
    glm_quat_mul(anim_rot, fish_orient, rot);

    mat4 model;
    glm_mat4_identity(model);
    glm_translate(model, (vec3){pos->x, pos->y, pos->z});
    mat4 rot_mat;
    glm_quat_mat4(rot, rot_mat);
    glm_mat4_mul(model, rot_mat, model);
    glm_scale(model, (vec3){scale->x, scale->y, scale->z});

    glm_mat4_copy(model, transforms[i].value);
  }
}

void RenderMeshSystem(EcsIter *it) {
  LocalToWorld *transforms = ecs_field(it, LocalToWorld, 0);
  MeshRenderer *renderers = ecs_field(it, MeshRenderer, 1);

  for (i32 i = 0; i < it->count; i++) {
    renderer_draw_mesh(renderers[i].mesh, renderers[i].material,
                       transforms[i].value);
  }
}

void InsertBoidsSystem(EcsIter *it) {
  Position *positions = ecs_field(it, Position, 0);
  Heading *headings = ecs_field(it, Heading, 1);
  BoidIndex *indices = ecs_field(it, BoidIndex, 2);

  for (i32 i = 0; i < it->count; i++) {
    f32 px = positions[i].x;
    f32 py = positions[i].y;
    f32 pz = positions[i].z;

    u32 hash = spatial_hash_3f(px, py, pz, CELL_SIZE) % GRID_SIZE;

    u32 slot = ins_atomic_u32_inc_eval(&state.buckets[hash].count) - 1;

    if (slot < MAX_PER_BUCKET) {
      BoidBucketEntry *entry = &state.buckets[hash].entries[slot];
      entry->px = px;
      entry->py = py;
      entry->pz = pz;
      entry->hx = headings[i].x;
      entry->hy = headings[i].y;
      entry->hz = headings[i].z;
    }
  }
}

void MergeCellsSystem(EcsIter *it) {
  for (i32 i = 0; i < it->count; i++) {
    i32 bucket_idx = it->offset + i;
    BoidBucket *bucket = &state.buckets[bucket_idx];
    if (bucket->count == 0)
      continue;

    u32 count = bucket->count;
    if (count > MAX_PER_BUCKET)
      count = MAX_PER_BUCKET;

    f32 sum_ax = 0, sum_ay = 0, sum_az = 0;
    f32 sum_sx = 0, sum_sy = 0, sum_sz = 0;
    for (u32 j = 0; j < count; j++) {
      BoidBucketEntry *entry = &bucket->entries[j];
      sum_ax += entry->hx;
      sum_ay += entry->hy;
      sum_az += entry->hz;
      sum_sx += entry->px;
      sum_sy += entry->py;
      sum_sz += entry->pz;
    }
    bucket->sum_align_x = sum_ax;
    bucket->sum_align_y = sum_ay;
    bucket->sum_align_z = sum_az;
    bucket->sum_sep_x = sum_sx;
    bucket->sum_sep_y = sum_sy;
    bucket->sum_sep_z = sum_sz;

    f32 first_px = bucket->entries[0].px;
    f32 first_py = bucket->entries[0].py;
    f32 first_pz = bucket->entries[0].pz;

    f32 nearest_target_dist_sq = 1e18f;
    i32 nearest_target_idx = 0;
    for (i32 t = 0; t < NUM_TARGETS; t++) {
      f32 dx = state.target_positions[t][0] - first_px;
      f32 dy = state.target_positions[t][1] - first_py;
      f32 dz = state.target_positions[t][2] - first_pz;
      f32 dist_sq = dx * dx + dy * dy + dz * dz;
      if (dist_sq < nearest_target_dist_sq) {
        nearest_target_dist_sq = dist_sq;
        nearest_target_idx = t;
      }
    }
    bucket->nearest_target_idx = nearest_target_idx;

    f32 nearest_obs_dist_sq = 1e18f;
    i32 nearest_obs_idx = 0;
    for (i32 o = 0; o < NUM_OBSTACLES; o++) {
      f32 dx = state.obstacle_positions[o][0] - first_px;
      f32 dy = state.obstacle_positions[o][1] - first_py;
      f32 dz = state.obstacle_positions[o][2] - first_pz;
      f32 dist_sq = dx * dx + dy * dy + dz * dz;
      if (dist_sq < nearest_obs_dist_sq) {
        nearest_obs_dist_sq = dist_sq;
        nearest_obs_idx = o;
      }
    }
    bucket->nearest_obstacle_idx = nearest_obs_idx;
    bucket->nearest_obstacle_dist = sqrtf(nearest_obs_dist_sq);
  }
}

void SteerBoidsSystem(EcsIter *it) {
  Position *positions = ecs_field(it, Position, 0);
  Heading *headings = ecs_field(it, Heading, 1);

  f32 dt = it->delta_time;
  if (dt > 0.05f)
    dt = 0.05f;

  for (i32 i = 0; i < it->count; i++) {
    f32 px = positions[i].x;
    f32 py = positions[i].y;
    f32 pz = positions[i].z;
    f32 forward_x = headings[i].x;
    f32 forward_y = headings[i].y;
    f32 forward_z = headings[i].z;

    u32 hash = spatial_hash_3f(px, py, pz, CELL_SIZE) % GRID_SIZE;
    BoidBucket *bucket = &state.buckets[hash];
    u32 count = bucket->count;
    if (count > MAX_PER_BUCKET)
      count = MAX_PER_BUCKET;

    f32 alignment_x, alignment_y, alignment_z;
    f32 separation_x, separation_y, separation_z;
    i32 neighbor_count;

    if (count == 0) {
      neighbor_count = 1;
      alignment_x = forward_x;
      alignment_y = forward_y;
      alignment_z = forward_z;
      separation_x = px;
      separation_y = py;
      separation_z = pz;
    } else {
      neighbor_count = (i32)count;
      alignment_x = bucket->sum_align_x;
      alignment_y = bucket->sum_align_y;
      alignment_z = bucket->sum_align_z;
      separation_x = bucket->sum_sep_x;
      separation_y = bucket->sum_sep_y;
      separation_z = bucket->sum_sep_z;
    }

    f32 inv_count = 1.0f / (f32)neighbor_count;

    i32 nearest_obstacle_idx = bucket->nearest_obstacle_idx;
    f32 nearest_obstacle_x = state.obstacle_positions[nearest_obstacle_idx][0];
    f32 nearest_obstacle_y = state.obstacle_positions[nearest_obstacle_idx][1];
    f32 nearest_obstacle_z = state.obstacle_positions[nearest_obstacle_idx][2];
    f32 nearest_obstacle_dist = bucket->nearest_obstacle_dist;

    i32 nearest_target_idx = bucket->nearest_target_idx;
    f32 nearest_target_x = state.target_positions[nearest_target_idx][0];
    f32 nearest_target_y = state.target_positions[nearest_target_idx][1];
    f32 nearest_target_z = state.target_positions[nearest_target_idx][2];

    f32 avg_align_x = alignment_x * inv_count;
    f32 avg_align_y = alignment_y * inv_count;
    f32 avg_align_z = alignment_z * inv_count;
    f32 align_dx = avg_align_x - forward_x;
    f32 align_dy = avg_align_y - forward_y;
    f32 align_dz = avg_align_z - forward_z;
    f32 align_len =
        sqrtf(align_dx * align_dx + align_dy * align_dy + align_dz * align_dz);
    f32 align_result_x = 0, align_result_y = 0, align_result_z = 0;
    if (align_len > 0.0001f) {
      f32 inv = BOID_ALIGNMENT_WEIGHT / align_len;
      align_result_x = align_dx * inv;
      align_result_y = align_dy * inv;
      align_result_z = align_dz * inv;
    }

    f32 sep_dx = px * (f32)neighbor_count - separation_x;
    f32 sep_dy = py * (f32)neighbor_count - separation_y;
    f32 sep_dz = pz * (f32)neighbor_count - separation_z;
    f32 sep_len = sqrtf(sep_dx * sep_dx + sep_dy * sep_dy + sep_dz * sep_dz);
    f32 sep_result_x = 0, sep_result_y = 0, sep_result_z = 0;
    if (sep_len > 0.0001f) {
      f32 inv = BOID_SEPARATION_WEIGHT / sep_len;
      sep_result_x = sep_dx * inv;
      sep_result_y = sep_dy * inv;
      sep_result_z = sep_dz * inv;
    }

    f32 target_dx = nearest_target_x - px;
    f32 target_dy = nearest_target_y - py;
    f32 target_dz = nearest_target_z - pz;
    f32 target_len = sqrtf(target_dx * target_dx + target_dy * target_dy +
                           target_dz * target_dz);
    f32 target_result_x = 0, target_result_y = 0, target_result_z = 0;
    if (target_len > 0.0001f) {
      f32 inv = BOID_TARGET_WEIGHT / target_len;
      target_result_x = target_dx * inv;
      target_result_y = target_dy * inv;
      target_result_z = target_dz * inv;
    }

    f32 obs_steer_x = px - nearest_obstacle_x;
    f32 obs_steer_y = py - nearest_obstacle_y;
    f32 obs_steer_z = pz - nearest_obstacle_z;
    f32 obs_steer_len =
        sqrtf(obs_steer_x * obs_steer_x + obs_steer_y * obs_steer_y +
              obs_steer_z * obs_steer_z);
    f32 avoid_heading_x = 0, avoid_heading_y = 0, avoid_heading_z = 0;
    if (obs_steer_len > 0.0001f) {
      f32 inv = 1.0f / obs_steer_len;
      f32 norm_x = obs_steer_x * inv;
      f32 norm_y = obs_steer_y * inv;
      f32 norm_z = obs_steer_z * inv;
      avoid_heading_x =
          (nearest_obstacle_x + norm_x * BOID_OBSTACLE_AVERSION_DISTANCE) - px;
      avoid_heading_y =
          (nearest_obstacle_y + norm_y * BOID_OBSTACLE_AVERSION_DISTANCE) - py;
      avoid_heading_z =
          (nearest_obstacle_z + norm_z * BOID_OBSTACLE_AVERSION_DISTANCE) - pz;
    }

    f32 normal_x = align_result_x + sep_result_x + target_result_x;
    f32 normal_y = align_result_y + sep_result_y + target_result_y;
    f32 normal_z = align_result_z + sep_result_z + target_result_z;
    f32 normal_len =
        sqrtf(normal_x * normal_x + normal_y * normal_y + normal_z * normal_z);
    if (normal_len > 0.0001f) {
      f32 inv = 1.0f / normal_len;
      normal_x *= inv;
      normal_y *= inv;
      normal_z *= inv;
    } else {
      normal_x = forward_x;
      normal_y = forward_y;
      normal_z = forward_z;
    }

    f32 target_forward_x, target_forward_y, target_forward_z;
    f32 obstacle_dist_from_radius =
        nearest_obstacle_dist - BOID_OBSTACLE_AVERSION_DISTANCE;
    if (obstacle_dist_from_radius < 0) {
      target_forward_x = avoid_heading_x;
      target_forward_y = avoid_heading_y;
      target_forward_z = avoid_heading_z;
    } else {
      target_forward_x = normal_x;
      target_forward_y = normal_y;
      target_forward_z = normal_z;
    }

    f32 new_hx = forward_x + dt * (target_forward_x - forward_x);
    f32 new_hy = forward_y + dt * (target_forward_y - forward_y);
    f32 new_hz = forward_z + dt * (target_forward_z - forward_z);
    f32 new_len = sqrtf(new_hx * new_hx + new_hy * new_hy + new_hz * new_hz);
    if (new_len > 0.0001f) {
      f32 inv = 1.0f / new_len;
      new_hx *= inv;
      new_hy *= inv;
      new_hz *= inv;
    }

    f32 move_dist = BOID_MOVE_SPEED * dt;
    positions[i].x = px + new_hx * move_dist;
    positions[i].y = py + new_hy * move_dist;
    positions[i].z = pz + new_hz * move_dist;
    headings[i].x = new_hx;
    headings[i].y = new_hy;
    headings[i].z = new_hz;
  }
}

void BuildMatricesSystem(EcsIter *it) {
  Position *positions = ecs_field(it, Position, 0);
  Heading *headings = ecs_field(it, Heading, 1);
  BoidIndex *indices = ecs_field(it, BoidIndex, 2);

  for (i32 i = 0; i < it->count; i++) {
    u32 idx = indices[i].index;
    mat4 *model = &state.instance_data[idx];

    vec3 pos = {positions[i].x, positions[i].y, positions[i].z};
    vec3 dir = {headings[i].x, headings[i].y, headings[i].z};

    quaternion heading_rot;
    quat_look_at_dir(dir, heading_rot);

    quaternion fish_orient;
    quat_from_euler(VEC3(RAD(90), RAD(180), 0), fish_orient);

    quaternion rot;
    glm_quat_mul(heading_rot, fish_orient, rot);

    vec3 scale = {0.01f, 0.01f, 0.01f};
    mat_trs(pos, rot, scale, *model);
  }
}

void app_init(AppMemory *memory) {
  UNUSED(memory);

  if (!is_main_thread()) {
    return;
  }

  AppContext *app_ctx = app_ctx_current();

  ecs_world_init(&state.world, &app_ctx->arena);
  ecs_store_init(&state.world);

  ECS_COMPONENT(&state.world, Position);
  ECS_COMPONENT(&state.world, Heading);
  ECS_COMPONENT(&state.world, BoidIndex);
  ECS_COMPONENT(&state.world, BoidTag);
  ECS_COMPONENT(&state.world, AnimationPlayer);
  ECS_COMPONENT_DEFINE(&state.world, LocalToWorld);
  ECS_COMPONENT_DEFINE(&state.world, Scale);
  ECS_COMPONENT_DEFINE(&state.world, MeshRenderer);

  state.input = input_init();
  state.camera = camera_init(VEC3(0, 11.6, 0.4), VEC3(-0.4f, 0, 0), 45.0f);
  state.fly_cam.camera_pos[0] = 0.0f;
  state.fly_cam.camera_pos[1] = 11.6f;
  state.fly_cam.camera_pos[2] = 40.4f;
  state.fly_cam.move_speed = 400.0f;
  renderer_init(&app_ctx->arena, app_ctx->num_threads,
                (u32)memory->canvas_width, (u32)memory->canvas_height, 4);

  ThreadContext *tctx = tctx_current();
  asset_system_init(&state.assets, 64);

  state.fish_albedo_tex = gpu_make_texture("public/fishAlbedo2.png");
  state.fish_tint_tex = gpu_make_texture("public/tints.png");
  state.fish_metallic_gloss_tex = gpu_make_texture("public/fishMetallicGloss.png");
  state.shark_albedo_tex = gpu_make_texture("public/SharkAlbedo.png");
  state.shark_metallic_gloss_tex = gpu_make_texture("public/SharkMetallicGloss.png");

  asset_load_blob(&state.assets, "public/fish.hasset", on_fish_loaded, NULL);
  asset_load_blob(&state.assets, "public/shark.hasset", on_shark_loaded, NULL);

  f32 spawn_radius = 15.0f;
  f32 spawn_center_x = 20.0f;
  f32 spawn_center_y = 5.0f;
  f32 spawn_center_z = -120.0f;
  for (i32 i = 0; i < NUM_BOIDS; i++) {
    EcsEntity e = ecs_entity_new(&state.world);

    UnityRandom rng = unity_random_new((u32)(i + 1) * 0x9F6ABC1u);
    f32 rx = unity_random_next_f32(&rng) - 0.5f;
    f32 ry = unity_random_next_f32(&rng) - 0.5f;
    f32 rz = unity_random_next_f32(&rng) - 0.5f;

    f32 len = sqrtf(rx * rx + ry * ry + rz * rz);
    f32 hx, hy, hz;
    if (len > 0.0001f) {
      f32 inv = 1.0f / len;
      hx = rx * inv;
      hy = ry * inv;
      hz = rz * inv;
    } else {
      hx = 0.0f;
      hy = 1.0f;
      hz = 0.0f;
    }

    f32 px = spawn_center_x + hx * spawn_radius;
    f32 py = spawn_center_y + hy * spawn_radius;
    f32 pz = spawn_center_z + hz * spawn_radius;

    ecs_set(&state.world, e, Position, {.x = px, .y = py, .z = pz});
    ecs_set(&state.world, e, Heading, {.x = hx, .y = hy, .z = hz});
    ecs_set(&state.world, e, BoidIndex, {.index = (u32)i});
    ecs_add(&state.world, e, ecs_id(BoidTag));
  }

  const SampledAnimationClip *target_clips[NUM_TARGETS] = {&Target01_animation,
                                                           &Target02_animation};
  for (i32 i = 0; i < NUM_TARGETS; i++) {
    EcsEntity e = ecs_entity_new(&state.world);
    state.target_entities[i] = e;

    vec3 initial_pos;
    sample_animation_position(target_clips[i], 0.0f, initial_pos);

    ecs_set(&state.world, e, Position,
            {.x = initial_pos[0], .y = initial_pos[1], .z = initial_pos[2]});
    ecs_set(&state.world, e, AnimationPlayer,
            {
                .clip = target_clips[i],
                .current_time = 0.0f,
                .dest_position = &state.target_positions[i],
            });

    state.target_positions[i][0] = initial_pos[0];
    state.target_positions[i][1] = initial_pos[1];
    state.target_positions[i][2] = initial_pos[2];
  }

  for (i32 i = 0; i < NUM_OBSTACLES; i++) {
    EcsEntity e = ecs_entity_new(&state.world);
    state.obstacle_entities[i] = e;

    vec3 initial_pos;
    sample_animation_position(&Shark_animation, 0.0f, initial_pos);

    ecs_set(&state.world, e, Position,
            {.x = initial_pos[0], .y = initial_pos[1], .z = initial_pos[2]});
    ecs_set(&state.world, e, AnimationPlayer,
            {
                .clip = &Shark_animation,
                .current_time = 0.0f,
                .dest_position = &state.obstacle_positions[i],
            });

    state.obstacle_positions[i][0] = initial_pos[0];
    state.obstacle_positions[i][1] = initial_pos[1];
    state.obstacle_positions[i][2] = initial_pos[2];
  }

  EcsTerm play_animations_terms[] = {
      ecs_term_inout(ecs_id(Position)),
      ecs_term_inout(ecs_id(AnimationPlayer)),
  };
  ecs_system_init(&state.world, &(EcsSystemDesc){
                                    .terms = play_animations_terms,
                                    .term_count = 2,
                                    .callback = PlayAnimationsSystem,
                                    .name = "PlayAnimationsSystem",
                                });

  EcsTerm build_animated_transform_terms[] = {
      ecs_term_in(ecs_id(Position)),
      ecs_term_in(ecs_id(AnimationPlayer)),
      ecs_term_in(ecs_id(Scale)),
      ecs_term_inout(ecs_id(LocalToWorld)),
  };
  ecs_system_init(&state.world, &(EcsSystemDesc){
                                    .terms = build_animated_transform_terms,
                                    .term_count = 4,
                                    .callback = BuildAnimatedTransformSystem,
                                    .name = "BuildAnimatedTransformSystem",
                                });

  EcsTerm render_mesh_terms[] = {
      ecs_term_in(ecs_id(LocalToWorld)),
      ecs_term_in(ecs_id(MeshRenderer)),
  };
  ecs_system_init(&state.world, &(EcsSystemDesc){
                                    .terms = render_mesh_terms,
                                    .term_count = 2,
                                    .callback = RenderMeshSystem,
                                    .name = "RenderMeshSystem",
                                });

  EcsTerm insert_boids_terms[] = {
      ecs_term_in(ecs_id(Position)),
      ecs_term_in(ecs_id(Heading)),
      ecs_term_in(ecs_id(BoidIndex)),
      ecs_term_none(ecs_id(BoidTag)),
  };
  EcsSystem *insert_boids_sys =
      ecs_system_init(&state.world, &(EcsSystemDesc){
                                        .terms = insert_boids_terms,
                                        .term_count = 4,
                                        .callback = InsertBoidsSystem,
                                        .name = "InsertBoidsSystem",
                                        .sync_mode = ECS_SYNC_BARRIER,
                                    });

  EcsSystem *merge_cells_sys =
      ecs_system_init(&state.world, &(EcsSystemDesc){
                                        .iter_mode = ECS_ITER_RANGE,
                                        .iter_count = GRID_SIZE,
                                        .callback = MergeCellsSystem,
                                        .name = "MergeCellsSystem",
                                        .sync_mode = ECS_SYNC_BARRIER,
                                    });
  ecs_system_depends_on(merge_cells_sys, insert_boids_sys);

  EcsTerm steer_boids_terms[] = {
      ecs_term_inout(ecs_id(Position)),
      ecs_term_inout(ecs_id(Heading)),
      ecs_term_none(ecs_id(BoidTag)),
  };
  EcsSystem *steer_boids_sys =
      ecs_system_init(&state.world, &(EcsSystemDesc){
                                        .terms = steer_boids_terms,
                                        .term_count = 3,
                                        .callback = SteerBoidsSystem,
                                        .name = "SteerBoidsSystem",
                                    });
  ecs_system_depends_on(steer_boids_sys, merge_cells_sys);

  EcsTerm build_matrices_terms[] = {
      ecs_term_in(ecs_id(Position)),
      ecs_term_in(ecs_id(Heading)),
      ecs_term_in(ecs_id(BoidIndex)),
      ecs_term_none(ecs_id(BoidTag)),
  };
  ecs_system_init(&state.world, &(EcsSystemDesc){
                                    .terms = build_matrices_terms,
                                    .term_count = 4,
                                    .callback = BuildMatricesSystem,
                                    .name = "BuildMatricesSystem",
                                });

  state.instance_buffer = renderer_create_instance_buffer(&(InstanceBufferDesc){
      .stride = sizeof(mat4),
      .max_instances = NUM_BOIDS,
  });

  LOG_INFO("Boids demo initialized: % boids", FMT_UINT(NUM_BOIDS));
}

void app_update_and_render(AppMemory *memory) {
  state.total_time = memory->total_time;

  Range_u64 range = lane_range(GRID_SIZE);
  for (u64 i = range.min; i < range.max; i++) {
    state.buckets[i].count = 0;
  }

  asset_system_update(&state.assets);

  if (is_main_thread()) {
    input_update(&state.input, &memory->input_events, memory->total_time);
    flycam_update(&state.fly_cam, &state.camera, &state.input, memory->dt);
    camera_update(&state.camera, memory->canvas_width, memory->canvas_height);

    renderer_begin_frame(
        state.camera.view, state.camera.proj,
        (GpuColor){2.0f / 255.0f, 94.0f / 255.0f, 131.0f / 255.0f, 1.0f},
        memory->total_time);
  }

  ecs_progress(&state.world, memory->dt);

  if (is_main_thread()) {
    renderer_update_instance_buffer(state.instance_buffer, state.instance_data,
                                    NUM_BOIDS);

    renderer_draw_mesh_instanced(state.fish_mesh, state.fish_material,
                                 state.instance_buffer);

    renderer_end_frame();
    input_end_frame(&state.input);
  }
}
