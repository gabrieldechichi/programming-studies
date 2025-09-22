#include "../animation.h"
#include "../camera.h"
#include "../game.h"
#include "../input.h"
#include "../lib/array.h"
#include "../lib/fmt.h"
#include "../lib/handle.h"
#include "../lib/math.h"
#include "../lib/memory.h"
#include "../lib/random.h"
#include "../lib/typedefs.h"
#include "../platform.h"
#include "../renderer.h"
#include "../vendor/cglm/util.h"
#include "../vendor/cglm/vec3.h"
#include "../vendor/stb/stb_image.h"

typedef struct {
  Handle texture;
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

typedef struct {
  f32 dt_buffer[20];
  u32 dt_idx;
  f32 dt_avg;
} PerfStats;

#define MAX_ANIM_INSTANCES 65536

typedef struct {
  GameInput input;

  DirectionalLightBlock directional_lights;
  PointLightsBlock point_lights;

  Camera camera;

  PerfStats stats;

  u32 temp_anim_count;
  u32 temp_anim_count_cap;

  StaticModel_Slice static_models;
  AnimatedModel_Slice animated_models;
} GameState;

// global data
global GameContext *ctx = NULL;
global GameState *g_game_state = NULL;
global PlatformReadFileOp load_sphere_op = -1;
global PlatformReadFileOp load_tex_op = -1;
global PlatformReadFileOp load_tex_op_2 = -1;
global PlatformReadFileOp load_anim_op = -1;
global Image *tex_data = NULL;
global Image *tex_data_2 = NULL;
global AnimationAsset *test_anim_asset = NULL;
global GameState *game_state;
global Model3DData *sphere_mesh = NULL;
global Handle_Array texture_handles = {0};
global Handle_Array material_handles = {0};
global Handle_Array mesh_handles = {0};
global Animation *anim = {0};

void spawn_more_animated_meshes() {
  if (!g_game_state) {
    return;
  }
  GameState *game_state = g_game_state;
  u32 total_count = game_state->temp_anim_count;

  if (total_count > game_state->animated_models.len) {

    i32 spawn_count = total_count - game_state->animated_models.len;
    i32 pool_count = game_state->temp_anim_count_cap - total_count;
    Xorshift32_State prgn = {0};
    xorshift32_seed(&prgn, 1234 * (game_state->animated_models.len + 1));
    for (i32 j = 0; j < spawn_count; j++) {
      AnimatedModel animated_model = {0};

      if (pool_count > 0) {
        pool_count--;
        i32 existing_anim_model_idx = game_state->animated_models.len;
        animated_model =
            game_state->animated_models.items[existing_anim_model_idx];
        LOG_INFO("Reusing animated model instance %",
                 FMT_UINT(existing_anim_model_idx));
      } else {
        animated_model.skinned_model.material_handles = material_handles;

        animated_model.skinned_model.joint_matrices =
            arr_new_ALLOC(&ctx->allocator, mat4, sphere_mesh->len_joints);

        animated_model.skinned_model.mesh_handles = mesh_handles;

        animated_model.animation_state.animation = anim;
        animated_model.animation_state.speed = 1.2f;
        animated_model.animation_state.weight = 1.0f;
        animated_model.animation_state.time = 1.2;
        // animated_model.animation_state.time =
        //     xorshift32_next_f32_range(&prgn, 0, anim->length);
      }

      slice_append(game_state->animated_models, animated_model);
    }
  } else if (total_count < game_state->animated_models.len) {
    // todo: leaking memory here (joint_matrices and mesh_handles alloc). Use
    // handle array?
    game_state->animated_models.len = fmaxf(0, total_count);
  }

  u32 grid_size = (i32)sqrtf(total_count);
  f32 spacing = 1.5f;

  for (u32 j = 0; j < total_count; j++) {
    AnimatedModel *animated_model = &game_state->animated_models.items[j];
    u32 grid_x = j % grid_size;
    u32 grid_z = j / grid_size;
    animated_model->temp_pos[0] = (grid_x - (f32)grid_size / 2) * spacing;
    animated_model->temp_pos[2] = (grid_z - (f32)grid_size / 2) * spacing - 2;

    quat_from_euler((vec3){glm_rad(0), glm_rad(0), 0},
                    animated_model->temp_rot);
  }
}

export void get_perf_stats(f32 *frame_time_ms, f32 *fps, f32 *cpu_memory_mb,
                           u32 *instance_count) {
  if (g_game_state) {
    *frame_time_ms = g_game_state->stats.dt_avg * 1000.0f;
    *fps = 1.0f / g_game_state->stats.dt_avg;
    *cpu_memory_mb =
        (f32)arena_committed_size(&ctx->allocator) / (1024.0f * 1024.0f);
    *instance_count = g_game_state->animated_models.len;
  } else {
    *frame_time_ms = 0.0f;
    *fps = 0.0f;
    *cpu_memory_mb = 0.0f;
    *instance_count = 0;
  }
}

export void spawn_100_more() {
  if (g_game_state) {
    g_game_state->temp_anim_count += 100;
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
    if (g_game_state->temp_anim_count >= 100) {
      g_game_state->temp_anim_count -= 100;
    } else {
      g_game_state->temp_anim_count = 0;
    }
  }
}

void gym_init(GameMemory *memory) {
  ctx = &memory->ctx;
  game_state = ALLOC(&ctx->allocator, GameState);
  g_game_state = game_state;
  assert(game_state);
  glm_vec3(cast(vec3){-0.5, 5, 11}, game_state->camera.pos);
  game_state->camera.pitch = -20;

  game_state->input.touches.cap = ARRAY_SIZE(game_state->input.touches.items);

  game_state->static_models =
      slice_new_ALLOC(&ctx->allocator, StaticModel, 0);
  game_state->animated_models =
      slice_new_ALLOC(&ctx->allocator, AnimatedModel, MAX_ANIM_INSTANCES);

  game_state->temp_anim_count = 100;
  game_state->temp_anim_count_cap = game_state->temp_anim_count;
}

void gym_update_and_render(GameMemory *memory) {
  // temp async load file code
  {
    if (load_sphere_op < 0) {
      load_sphere_op = platform_start_read_file("animation_test.hmobj");
    }
    if (load_tex_op < 0) {
      load_tex_op = platform_start_read_file("Gorilla_BaseMap.png");
    }
    if (load_tex_op_2 < 0) {
      load_tex_op_2 = platform_start_read_file("Short_Hair.png");
    }
    if (load_anim_op < 0) {
      load_anim_op = platform_start_read_file("anim_test.hasset");
    }

    if (platform_check_read_file(load_sphere_op) == FREADSTATE_COMPLETED) {
      PlatformFileData data = {};
      if (platform_get_file_data(load_sphere_op, &data, &ctx->temp_allocator)) {
        sphere_mesh = load_model(data.buffer, data.buffer_len, &ctx->allocator);
      }
    }

    if (platform_check_read_file(load_tex_op) == FREADSTATE_COMPLETED) {
      PlatformFileData data = {};
      if (platform_get_file_data(load_tex_op, &data, &ctx->temp_allocator)) {
        // Decode the PNG texture using lodepng_decode32
        int x, y, n;
        u8 *decoded_data =
            stbi_load_from_memory(data.buffer, data.buffer_len, &x, &y, &n, 4);

        if (decoded_data) {
          // Allocate and populate the tex_data structure
          tex_data = ALLOC(&ctx->allocator, Image);
          tex_data->width = x;
          tex_data->height = y;
          tex_data->byte_len = data.buffer_len;
          tex_data->data = (uint8 *)decoded_data;
        }
      }
    }
    if (platform_check_read_file(load_tex_op_2) == FREADSTATE_COMPLETED) {
      PlatformFileData data = {};
      if (platform_get_file_data(load_tex_op_2, &data, &ctx->temp_allocator)) {
        // Decode the PNG texture using lodepng_decode32
        int x, y, n;
        u8 *decoded_data =
            stbi_load_from_memory(data.buffer, data.buffer_len, &x, &y, &n, 4);

        if (decoded_data) {
          // Allocate and populate the tex_data structure
          tex_data_2 = ALLOC(&ctx->allocator, Image);
          tex_data_2->width = x;
          tex_data_2->height = y;
          tex_data_2->byte_len = data.buffer_len;
          tex_data_2->data = (uint8 *)decoded_data;
        }
      }
    }

    if (platform_check_read_file(load_anim_op) == FREADSTATE_COMPLETED) {
      PlatformFileData data = {};
      if (platform_get_file_data(load_anim_op, &data, &ctx->temp_allocator)) {
        test_anim_asset = animation_asset_read(
            (u8_Array){.items = data.buffer, .len = data.buffer_len},
            &ctx->allocator);
      }
    }

    if (sphere_mesh && tex_data && tex_data_2 && test_anim_asset) {

      anim =
          animation_from_asset(test_anim_asset, sphere_mesh, &ctx->allocator);

      mesh_handles =
          arr_new_ALLOC(&ctx->allocator, Handle, sphere_mesh->num_meshes);
      texture_handles =
          arr_new_ALLOC(&ctx->allocator, Handle, sphere_mesh->num_meshes);
      material_handles =
          arr_new_ALLOC(&ctx->allocator, Handle, sphere_mesh->num_meshes);

      for (u32 i = 0; i < sphere_mesh->num_meshes; i++) {
        MeshData *mesh = &sphere_mesh->meshes[i];
        bool32 success =
            renderer_create_skmesh_renderer(mesh, &mesh_handles.items[i]);
        assert(success);
        success &= renderer_create_texture(i == 0 ? tex_data : tex_data_2,
                                           &texture_handles.items[i]);
        assert(success);

        vec3 mat_color_1 = {1.0, 1.0, 1.0f};
        vec3 mat_color_2 = {0.3f, 0.3f, 0.3f};
        success &= renderer_create_skmaterial(texture_handles.items[i], i == 0 ? mat_color_1 : mat_color_2, &material_handles.items[i]);
        assert(success);
      }
      tex_data = NULL;
      tex_data_2 = NULL;
    }

    if (sphere_mesh != NULL && mesh_handles.len > 0 &&
        material_handles.len != 0) {
      if (game_state->temp_anim_count != game_state->animated_models.len) {
        spawn_more_animated_meshes();
      }
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
    game_state->directional_lights.count = 1;
    game_state->directional_lights.lights[0] = (DirectionalLight){
        .direction = {2, 1, 1}, .color = {1, 1, 1}, .intensity = 1};

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

  // animation
  arr_foreach_ptr(game_state->animated_models, animated_model) {
    animation_update(&animated_model->animation_state, dt);
    animation_evaluate(&animated_model->animation_state,
                       animated_model->skinned_model.joint_matrices);
  }

  // render models
  {
    arr_foreach_ptr(game_state->animated_models, 
                    animated_model) {
      mat_trs(animated_model->temp_pos, animated_model->temp_rot,
              (vec3){1, 1, 1}, animated_model->temp_model_matrix);

      renderer_draw_skmeshes(
          animated_model->skinned_model.mesh_handles.items,
          animated_model->skinned_model.material_handles.items,
          animated_model->skinned_model.mesh_handles.len,
          animated_model->temp_model_matrix,
          animated_model->skinned_model.joint_matrices.items,
          animated_model->skinned_model.joint_matrices.len);
    }

    arr_foreach_ptr(game_state->static_models, model) {

      mat4 m;
      mat4_identity(m);
      mat_t((vec3){2, 0, 0}, m);
      renderer_draw_meshes(model->meshes.items, model->meshes.len,
                           &model->texture, m);
    }
  }

  // end frame
  {
    input_end_frame(&game_state->input);

    PerfStats *stats = &game_state->stats;

    stats->dt_buffer[stats->dt_idx] = dt;
    stats->dt_idx = (stats->dt_idx + 1) % ARRAY_SIZE(stats->dt_buffer);

    stats->dt_avg =
        arr_sum_raw(stats->dt_buffer, ARRAY_SIZE(stats->dt_buffer), f32);
    stats->dt_avg /= ARRAY_SIZE(stats->dt_buffer);
  }
}
