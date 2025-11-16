#include "../animation.h"
#include "../assets.h"
#include "../camera.h"
#include "../game.h"
#include "../input.h"
#include "../lib/array.h"
#include "../lib/handle.h"
#include "../lib/math.h"
#include "../lib/memory.h"
#include "../lib/random.h"
#include "../lib/typedefs.h"
#include "../platform.h"
#include "../renderer.h"
#include "../stats.h"
#include "../vendor/cglm/util.h"
#include "../vendor/cglm/vec3.h"
#include "../vendor/stb/stb_image.h"

#define asset_request_texture(system, file_path)                               \
  asset_request(Texture, system, ctx, file_path)
#define asset_request_model(system, file_path)                                 \
  asset_request(Model3DData, system, ctx, file_path)
#define asset_request_animation(system, file_path)                             \
  asset_request(AnimationAsset, system, ctx, file_path)

typedef struct {
  Handle batch_handle;
  mat4_Slice model_matrices;
  f32_Slice times;
} InstancedSkinnedBatch;
slice_define(InstancedSkinnedBatch);
arr_define(InstancedSkinnedBatch);

typedef struct {
  InstancedSkinnedBatch_Slice batches;
  u32 current_batch_idx;
} InstancedSkinnedBatchGroup;
slice_define(InstancedSkinnedBatchGroup);
HANDLE_ARRAY_DEFINE(InstancedSkinnedBatchGroup);
TYPED_HANDLE_DEFINE(InstancedSkinnedBatchGroup);

typedef struct {
  u32 len;
  f32 *times;
  f32 *speeds;
  f32 *anim_lengths;
  mat4 *model_matrices;
} AnimatedEntitiesSOA;

#define MAX_LOD_LEVELS 8
#define LOD_COUNT 6

typedef struct {
  AssetSystem assets;
  GameInput input;

  DirectionalLightBlock directional_lights;
  PointLightsBlock point_lights;

  Camera camera;

  PerfStats stats;

  u32 temp_anim_count;
  u32 temp_anim_count_cap;

  AnimatedEntitiesSOA animated_entities;
  f32 entity_bounding_radius;

  HandleArray_InstancedSkinnedBatchGroup batch_groups;
  LODLevel_Array lod_config;
} GameState;

// global data
global GameContext *ctx = NULL;
global GameState *g_game_state = NULL;
global Image_Handle tex_asset_handle = INVALID_TYPED_HANDLE(Image);
global Image *tex_data = NULL;
global AnimationAsset *test_anim_asset = NULL;
global AnimationAsset_Handle test_anim_asset_handle =
    INVALID_TYPED_HANDLE(AnimationAsset);
global GameState *game_state;
global Model3DData_Handle lod_asset_handles[LOD_COUNT] = {0};
global Model3DData *lods[LOD_COUNT] = {0};
global Handle_Array texture_handles = {0};
global Handle_Array material_handles = {0};
global Handle_Array mesh_handles = {0};
global Animation *anim = {0};
global Handle anim_texture_handle;

// for mobile
// #define MAX_ANIM_INSTANCES 18000
global u32 MAX_ANIM_INSTANCES = 0;
// #define MAX_ANIM_INSTANCES 16384
global u32 MAX_INSTANCES_PER_BATCH = 16384;
global u32 BATCHES_PER_LOD = 0;
// (((MAX_ANIM_INSTANCES) + (MAX_INSTANCES_PER_BATCH)-1) /
//  (MAX_INSTANCES_PER_BATCH));

void create_batch(u32 lod_idx) {
  InstancedSkinnedBatchGroup *batch_group =
      ha_get_assert(InstancedSkinnedBatchGroup, &game_state->batch_groups,
                    game_state->lod_config.items[lod_idx].renderer_id);
  debug_assert_or_return_void_msg(batch_group->batches.len < BATCHES_PER_LOD,
                                  "Out of capacity for creating batches");

  InstancedSkinnedBatch batch = {0};

  // create gpu batch
  b32 success = renderer_skm_create_batch_compute(
      mesh_handles.items[lod_idx], material_handles.items[0], lods[lod_idx],
      &batch.batch_handle);

  assert(success);

  // cpu side batch
  batch.model_matrices =
      slice_new_ALLOC(&ctx->allocator, mat4, MAX_INSTANCES_PER_BATCH);
  batch.times = slice_new_ALLOC(&ctx->allocator, f32, MAX_INSTANCES_PER_BATCH);

  slice_append(batch_group->batches, batch);
}

void spawn_more_animated_meshes() {
  if (!g_game_state) {
    return;
  }
  GameState *game_state = g_game_state;
  u32 total_count = game_state->temp_anim_count;

  for (u32 i = 0; i < LOD_COUNT; i++) {

    InstancedSkinnedBatchGroup *batch_group =
        ha_get_assert(InstancedSkinnedBatchGroup, &game_state->batch_groups,
                      game_state->lod_config.items[i].renderer_id);
    u32 required_batch_count = (total_count / MAX_INSTANCES_PER_BATCH) + 1;
    i32 new_batches = required_batch_count - batch_group->batches.len;
    for (i32 j = 0; j < new_batches; j++) {
      LOG_INFO("Allocating new batch % %", FMT_UINT(i),
               FMT_UINT(batch_group->batches.len));
      create_batch(i);
    }
  }

  if (total_count > game_state->animated_entities.len) {

    i32 spawn_count = total_count - game_state->animated_entities.len;
    Xorshift32_State prgn = {0};
    xorshift32_seed(&prgn, 1234 * (game_state->animated_entities.len + 1));
    for (i32 j = 0; j < spawn_count; j++) {
      AnimatedEntitiesSOA *animated_entities = &game_state->animated_entities;
      i32 animated_entity_idx = game_state->animated_entities.len;

      mat4_identity(animated_entities->model_matrices[animated_entity_idx]);

      animated_entities->times[animated_entity_idx] = 1.2 * j;
      animated_entities->speeds[animated_entity_idx] = 1.0f;
      animated_entities->anim_lengths[animated_entity_idx] = anim->length;

      animated_entities->len++;
    }
  } else if (total_count < game_state->animated_entities.len) {
    game_state->animated_entities.len = fmaxf(0, total_count);
  }

  u32 grid_size = (i32)sqrtf(total_count);
  f32 spacing = 1.f;

  for (u32 j = 0; j < total_count; j++) {
    AnimatedEntitiesSOA animated_entities = game_state->animated_entities;
    u32 grid_x = j % grid_size;
    u32 grid_z = j / grid_size;

    vec3 temp_pos;
    temp_pos[0] = (grid_x - (f32)grid_size / 2) * spacing;
    temp_pos[1] = 0;
    temp_pos[2] = (grid_z - (f32)grid_size / 2) * spacing - 2;

    quaternion temp_rot;
    quat_from_euler((vec3){glm_rad(90), 0, 0}, temp_rot);

    mat_trs(temp_pos, temp_rot, (vec3){0.01, 0.01, 0.01},
            animated_entities.model_matrices[j]);
  }
}

export void get_perf_stats(f32 *frame_time_ms, f32 *fps, f32 *cpu_memory_mb,
                           u32 *instance_count) {
  if (g_game_state) {
    *frame_time_ms = g_game_state->stats.dt_avg * 1000.0f;
    *fps = 1.0f / g_game_state->stats.dt_avg;
    *cpu_memory_mb =
        (f32)arena_committed_size(&ctx->allocator) / (1024.0f * 1024.0f);
    *instance_count = g_game_state->animated_entities.len;
  } else {
    *frame_time_ms = 0.0f;
    *fps = 0.0f;
    *cpu_memory_mb = 0.0f;
    *instance_count = 0;
  }
}

#define SPAWN_INCREMENT 500
export void spawn_100_more() {
  if (g_game_state) {
    g_game_state->temp_anim_count += SPAWN_INCREMENT;
    if (g_game_state->temp_anim_count > MAX_ANIM_INSTANCES) {
      g_game_state->temp_anim_count = MAX_ANIM_INSTANCES;
    }

    // increase or keep cap
    g_game_state->temp_anim_count_cap =
        fmaxf(g_game_state->temp_anim_count, g_game_state->temp_anim_count_cap);
  }
}

export void remove_100() {
  if (g_game_state) {
    if (g_game_state->temp_anim_count >= SPAWN_INCREMENT) {
      g_game_state->temp_anim_count -= SPAWN_INCREMENT;
    } else {
      g_game_state->temp_anim_count = 0;
    }
  }
}

void gym_init(GameMemory *memory) {
  MAX_INSTANCES_PER_BATCH = (u32)memory->temp_instances_per_batch;
  MAX_ANIM_INSTANCES = 200000;
  BATCHES_PER_LOD = (((MAX_ANIM_INSTANCES) + (MAX_INSTANCES_PER_BATCH)-1) /
                     (MAX_INSTANCES_PER_BATCH));

  ctx = &memory->ctx;
  game_state = ALLOC(&ctx->allocator, GameState);
  g_game_state = game_state;
  assert(game_state);
  glm_vec3(cast(vec3){-0.5, 120, 280}, game_state->camera.pos);
  // glm_vec3(cast(vec3){-0.5, 120 / 2, 280 / 2}, game_state->camera.pos);
  // glm_vec3(cast(vec3){-0.5, 5, 10}, game_state->camera.pos);
  game_state->camera.pitch = -25;

  game_state->input.touches.cap = ARRAY_SIZE(game_state->input.touches.items);

  game_state->batch_groups = ha_init(
      InstancedSkinnedBatchGroup, &ctx->allocator, BATCHES_PER_LOD * LOD_COUNT);

  asset_system_init(&game_state->assets, &ctx->allocator, 512);

  lod_asset_handles[0] =
      asset_request_model(&game_state->assets, "xbot_lod_0.hmobj");
  lod_asset_handles[1] =
      asset_request_model(&game_state->assets, "xbot_lod_1.hmobj");
  lod_asset_handles[2] =
      asset_request_model(&game_state->assets, "xbot_lod_2.hmobj");
  lod_asset_handles[3] =
      asset_request_model(&game_state->assets, "xbot_lod_3.hmobj");
  lod_asset_handles[4] =
      asset_request_model(&game_state->assets, "xbot_lod_4.hmobj");
  lod_asset_handles[5] =
      asset_request_model(&game_state->assets, "xbot_lod_5.hmobj");

  tex_asset_handle =
      asset_request_texture(&game_state->assets, "xbot_tex.webp");
  test_anim_asset_handle =
      asset_request_animation(&game_state->assets, "anim_test.hasset");

  game_state->animated_entities = (AnimatedEntitiesSOA){
      .len = 0,
      .times = ALLOC_ARRAY(&ctx->allocator, f32, MAX_ANIM_INSTANCES),
      .speeds = ALLOC_ARRAY(&ctx->allocator, f32, MAX_ANIM_INSTANCES),
      .anim_lengths = ALLOC_ARRAY(&ctx->allocator, f32, MAX_ANIM_INSTANCES),
      .model_matrices = ALLOC_ARRAY(&ctx->allocator, mat4, MAX_ANIM_INSTANCES)};

  game_state->temp_anim_count = 100000;
  game_state->temp_anim_count_cap = game_state->temp_anim_count;
}

void gym_update_and_render(GameMemory *memory) {
  local_persist b32 did_load = false;
  AssetSystem *assets = &game_state->assets;
  asset_system_update(assets, &memory->ctx);

  // init stuff after load
  if (!did_load && asset_system_pending_count(assets) == 0) {
    did_load = true;
    for (u32 i = 0; i < ARRAY_SIZE(lod_asset_handles); i++) {
      lods[i] = asset_get_data(assets, lod_asset_handles[i]);
    }
    tex_data = asset_get_data(assets, tex_asset_handle);
    test_anim_asset = asset_get_data(assets, test_anim_asset_handle);

    anim = animation_from_asset(test_anim_asset, lods[0], &ctx->allocator);

    // gpu anim texture
    {
      b32 success = renderer_create_animation_texture(
          ctx, anim, _out_ & anim_texture_handle);
      assert(success);
    }

    game_state->entity_bounding_radius = 1.0f;

    f32 lod_factor = platform_is_mobile() ? 0.75f : 1.0f;
    f32 first_lod_factor = platform_is_mobile() ? 0.4f : 1.0f;
    game_state->lod_config =
        arr_new_ALLOC(&ctx->allocator, LODLevel, LOD_COUNT);

    game_state->lod_config.items[0].max_distance_squared =
        SQR(5.0f * first_lod_factor);
    game_state->lod_config.items[1].max_distance_squared =
        SQR(10.0f * lod_factor);
    game_state->lod_config.items[2].max_distance_squared =
        SQR(20.0f * lod_factor);
    game_state->lod_config.items[3].max_distance_squared =
        SQR(30.0f * lod_factor);
    game_state->lod_config.items[4].max_distance_squared =
        SQR(60.0f * lod_factor);
    game_state->lod_config.items[5].max_distance_squared = INFINITY;

    // create tex
    texture_handles = arr_new_ALLOC(&ctx->allocator, Handle, 1);
    b32 success = renderer_create_texture(tex_data, &texture_handles.items[0]);
    assert(success);

    // material
    material_handles = arr_new_ALLOC(&ctx->allocator, Handle, 1);
    success = renderer_skm_create_material(
        texture_handles.items[0], (vec3){1, 1, 1}, &material_handles.items[0]);

    mesh_handles = arr_new_ALLOC(&ctx->allocator, Handle, LOD_COUNT);

    for (u32 i = 0; i < LOD_COUNT; i++) {
      Model3DData *lod = lods[i];
      MeshData *mesh_lod = &lod->meshes[0];

      // create mesh vao
      b32 success = renderer_skm_create_mesh(mesh_lod, &mesh_handles.items[i]);
      assert(success);

      // initialize batch group for this LOD level
      InstancedSkinnedBatchGroup batch_group = {0};

      batch_group.batches = slice_new_ALLOC(
          &ctx->allocator, InstancedSkinnedBatch, BATCHES_PER_LOD);
      batch_group.current_batch_idx = 0;

      game_state->lod_config.items[i].renderer_id =
          cast_handle(Handle, ha_add(InstancedSkinnedBatchGroup,
                                     &game_state->batch_groups, batch_group));

      create_batch(i);
    }
  }

  if (game_state->lod_config.len > 0 && anim != NULL) {
    if (game_state->temp_anim_count != game_state->animated_entities.len) {
      spawn_more_animated_meshes();
    }
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
    // todo: fix light direction
    game_state->directional_lights.count = 1;
    game_state->directional_lights.lights[0] = (DirectionalLight){
        .direction = {-2, 2, -1}, .color = {1, 1, 1}, .intensity = 1.0};

    game_state->point_lights.count = 0;

    renderer_set_lights(&game_state->directional_lights,
                        &game_state->point_lights);
  }

  AnimatedEntitiesSOA animated_entities = game_state->animated_entities;

  // update animation
  for (u32 i = 0; i < animated_entities.len; i++) {
    f32 *time = &animated_entities.times[i];
    f32 speed = animated_entities.speeds[i];
    f32 length = animated_entities.anim_lengths[i];

    *time += dt * speed;

    if (length > 0.0f) {
      // Use fmod to handle multiple wraps and negative values
      *time = fmodf(*time, length);

      // Ensure positive time for negative wrap-around
      if (*time < 0.0f) {
        *time += length;
      }
    }
  }

  // render models with frustum culling and LOD selection
  if (game_state->lod_config.len > 0) {
    // reset all batches in all LOD items
    for (u32 lod_idx = 0; lod_idx < game_state->lod_config.len; lod_idx++) {
      InstancedSkinnedBatchGroup *batch_group =
          ha_get_assert(InstancedSkinnedBatchGroup, &game_state->batch_groups,
                        game_state->lod_config.items[lod_idx].renderer_id);
      batch_group->current_batch_idx = 0;

      for (u32 batch_idx = 0; batch_idx < batch_group->batches.len;
           batch_idx++) {
        InstancedSkinnedBatch *batch = &batch_group->batches.items[batch_idx];
        batch->model_matrices.len = 0;
        batch->times.len = 0;
      }
    }

    // get camera position for distance calculations
    vec3 camera_pos;
    vec3_copy(game_state->camera.pos, camera_pos);

    // frustum culling and LOD selection: distribute entities to appropriate LOD
    // batches
    for (u32 i = 0; i < animated_entities.len; i++) {
      vec3 entity_pos;
      mat4_get_translation(animated_entities.model_matrices[i], entity_pos);

      // frustum culling first
      if (sphere_in_frustum(&game_state->camera.frustum, entity_pos,
                            game_state->entity_bounding_radius)) {

        // calculate squared distance to camera for LOD selection
        vec3 cam_to_entity;
        glm_vec3_sub(entity_pos, camera_pos, cam_to_entity);
        f32 dist_squared = vec3_sqrlen(cam_to_entity);

        // determine LOD level based on distance using LOD config
        u32 lod_level =
            game_state->lod_config.len - 1; // default to lowest quality
        for (u32 lod_idx = 0; lod_idx < game_state->lod_config.len - 1;
             lod_idx++) {
          if (dist_squared <
              game_state->lod_config.items[lod_idx].max_distance_squared) {
            lod_level = lod_idx;
            break;
          }
        }

        // get the batch group for this LOD level
        InstancedSkinnedBatchGroup *batch_group =
            ha_get_assert(InstancedSkinnedBatchGroup, &game_state->batch_groups,
                          game_state->lod_config.items[lod_level].renderer_id);

        // find a batch with available space
        b32 entity_added = false;
        arr_foreach_ptr(batch_group->batches, batch) {
          // check if this batch has space
          if (batch->model_matrices.len < batch->model_matrices.cap) {
            u32 batch_entity_idx = batch->model_matrices.len;

            // copy entity data to this batch
            glm_mat4_copy(animated_entities.model_matrices[i],
                          batch->model_matrices.items[batch_entity_idx]);
            batch->times.items[batch_entity_idx] = animated_entities.times[i];

            batch->model_matrices.len++;
            batch->times.len++;
            entity_added = true;
            break;
          }
        }

        debug_assert_msg(
            entity_added,
            "Failed to add entity for group (LOD %). All groups filled",
            FMT_UINT(lod_level));
      }
    }

    // render all batches in all LOD items
    for (u32 lod_idx = 0; lod_idx < game_state->lod_config.len; lod_idx++) {
      InstancedSkinnedBatchGroup *batch_group =
          ha_get_assert(InstancedSkinnedBatchGroup, &game_state->batch_groups,
                        game_state->lod_config.items[lod_idx].renderer_id);

      arr_foreach_ptr(batch_group->batches, batch) {
        if (batch->times.len > 0) {
          renderer_skm_draw_batch_compute(
              batch->batch_handle, anim_texture_handle, anim->keyframes.len,
              batch->times.len, batch->model_matrices.items,
              batch->times.items);
        }
      }
    }
  }

  // end frame
  {
    input_end_frame(&game_state->input);

    perf_stats_update(&game_state->stats, dt);
  }
}
