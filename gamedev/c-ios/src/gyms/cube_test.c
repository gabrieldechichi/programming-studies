#include "assets.h"
#include "camera.h"
#include "cglm/util.h"
#include "cglm/vec3.h"
#include "context.h"
#include "game.h"
#include "gameplay_lib.c"
#include "lib/array.h"
#include "lib/math.h"
#include "lib/memory.h"
#include "lib/typedefs.h"
#include "platform/platform.h"
#include "renderer/renderer.h"
#include <stdio.h>
#include <string.h>

typedef struct {
  mat4 model_matrix;
  SkinnedModel skinned_model;
} CubeEntity;

typedef struct {
  // memory
  ArenaAllocator permanent_arena;
  ArenaAllocator temporary_arena;
  GameContext ctx;

  AssetSystem asset_system;
  GameInput input;

  // assets
  Model3DData_Handle model_asset_handle;
  MaterialAsset_Handle *material_asset_handles;
  u32 material_count;
  Model3DData *model_data;
  Material_Slice materials;

  // 3D scene data
  DirectionalLightBlock directional_lights;
  Camera camera;

  CubeEntity cube;
} GymState;

global GameContext *g_game_ctx;

extern GameContext *get_global_ctx() { return g_game_ctx; }

void gym_init(GameMemory *memory) {
  GymState *gym_state = (GymState *)memory->permanent_memory;
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

  gym_state->asset_system = asset_system_init(&ctx->allocator, 512);

  // Request the cube model
  // gym_state->model_asset_handle = asset_request(
  //     Model3DData, &gym_state->asset_system, ctx, "cube/cube.hasset");
  gym_state->model_asset_handle = asset_request(
      Model3DData, &gym_state->asset_system, ctx, "assets/generic_female/generic_female.hasset");

  // Initialize camera
  gym_state->camera = (Camera){
      .pos = {0.0, 0.0, 5.0},
      .pitch = 0.0f,
      .fov = 45.0f,
  };
  quat_identity(gym_state->camera.rot);

  printf("[CubeTest] Initialized, requesting cube model\n");
}

void handle_loading(GymState *gym_state, AssetSystem *asset_system) {
  GameContext *ctx = &gym_state->ctx;

  // Load model data first
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
          // Request material asset
          gym_state->material_asset_handles[material_idx] =
              asset_request(MaterialAsset, &gym_state->asset_system, ctx,
                            submesh_data->material_path.value);
          LOG_INFO("Requesting material % for mesh % submesh %",
                   FMT_STR(submesh_data->material_path.value), FMT_UINT(i),
                   FMT_UINT(j));
        } else {
          // No material path - will use default material
          gym_state->material_asset_handles[material_idx] =
              (MaterialAsset_Handle){0};
          LOG_INFO("No material path for mesh % submesh %, will use default",
                   FMT_UINT(i), FMT_UINT(j));
        }
        material_idx++;
      }
    }

    LOG_INFO("Cube model loaded with % meshes, % total submeshes",
             FMT_UINT(gym_state->model_data->num_meshes),
             FMT_UINT(total_submeshes));

    // Log vertex format info
    if (gym_state->model_data->num_meshes > 0 &&
        gym_state->model_data->meshes[0].submeshes.len > 0) {
      SubMeshData *first_submesh =
          &gym_state->model_data->meshes[0].submeshes.items[0];
      LOG_INFO("First submesh: % vertices, % indices, vertex buffer size: %",
               FMT_UINT(first_submesh->len_vertices),
               FMT_UINT(first_submesh->len_indices),
               FMT_UINT(first_submesh->len_vertex_buffer));
    }
  }

  // Wait for all materials to load, then create SkinnedModel
  if (gym_state->model_data && !gym_state->cube.skinned_model.meshes.items) {
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

    if (all_materials_ready) {
      // Create materials array
      gym_state->materials =
          slice_new_ALLOC(&ctx->allocator, Material, gym_state->material_count);

      for (u32 i = 0; i < gym_state->material_count; i++) {
        if (gym_state->material_asset_handles[i].idx != 0) {
          // Load material from asset
          MaterialAsset *material_asset =
              asset_get_data(MaterialAsset, &gym_state->asset_system,
                             gym_state->material_asset_handles[i]);
          assert(material_asset);
          Material *material = material_from_asset(
              material_asset, &gym_state->asset_system, ctx);
          slice_append(gym_state->materials, *material);
          LOG_INFO("Loaded material % for submesh %",
                   FMT_STR(material_asset->name.value), FMT_UINT(i));
        } else {
          // Use default material
          LOG_WARN("No material for submesh %, using default", FMT_UINT(i));

          // Create a simple default material with white color
          LoadShaderParams shader_params = {.shader_name = "toon_shading"};
          Handle shader_handle = load_shader(shader_params);

          MaterialProperty props[] = {{.name = {.value = "uColor", .len = 6},
                                       .type = MAT_PROP_VEC3,
                                       .value.vec3_val = {1.0f, 1.0f, 1.0f}}};

          Handle material_handle =
              load_material(shader_handle, props, 1, false);

          Material default_material = {.gpu_material = material_handle};
          slice_append(gym_state->materials, default_material);
        }
      }

      // Create SkinnedModel with loaded materials
      CubeEntity *entity = &gym_state->cube;

      // Set up model matrix - identity for now
      glm_mat4_identity(entity->model_matrix);

      entity->skinned_model =
          skmodel_from_asset(ctx, gym_state->model_data, gym_state->materials);

      LOG_INFO("SkinnedModel created with % materials",
               FMT_UINT(gym_state->materials.len));
    }
  }
}

void gym_update_and_render(GameMemory *memory) {
  GymState *gym_state = (GymState *)memory->permanent_memory;
  GameContext *ctx = &gym_state->ctx;
  GameTime *time = &memory->time;
  f32 dt = time->dt;

  AssetSystem *asset_system = &gym_state->asset_system;
  GameInput *input = &gym_state->input;

  // Load assets
  handle_loading(gym_state, asset_system);
  asset_system_update(asset_system, ctx);

  input_update(input, &memory->input_events, memory->time.now);

  CubeEntity *entity = &gym_state->cube;

  // Update camera
  camera_update_uniforms(&gym_state->camera, memory->canvas.width,
                         memory->canvas.height);
  renderer_update_camera(&gym_state->camera.uniforms);

  // Set up lighting
  vec3 light_dir = {0.5f, -1.0f, -0.5f};
  glm_normalize(light_dir);

  gym_state->directional_lights.count = 1;
  gym_state->directional_lights.lights[0] = (DirectionalLight){
      .direction = {light_dir[0], light_dir[1], light_dir[2]},
      .color = {1, 1, 1},
      .intensity = 1.0};

  renderer_set_lights(&gym_state->directional_lights);

  // Clear to gray
  Color clear_color = color_from_hex(0xff0000);
  renderer_clear(clear_color);

  // Rotate the cube slowly
  static f32 rotation = 0.0f;
  rotation += dt * 0.5f;

  // Create rotation matrix
  mat4 rot_matrix;
  quaternion rot;
  quat_from_euler(VEC3(0, glm_rad(45), glm_rad(45)), rot);
  quat_identity(rot);
  mat_trs(VEC3(0, 0, 0), rot, VEC3(1,1,1), rot_matrix);

  // Draw the cube if loaded
  if (gym_state->cube.skinned_model.meshes.items) {
    SkinnedModel *skinned_model = &entity->skinned_model;

    // If the model has joints, use them; otherwise create identity transforms
    u32 num_joints = skinned_model->joint_matrices.len;
    if (num_joints == 0) {
      // No joints, create identity transforms
      num_joints = 256; // Default size
      mat4 *joint_transforms =
          ALLOC_ARRAY(&ctx->temp_allocator, mat4, num_joints);
      for (u32 i = 0; i < num_joints; i++) {
        glm_mat4_identity(joint_transforms[i]);
      }

      // Draw all submeshes
      for (u32 i = 0; i < skinned_model->meshes.len; i++) {
        SkinnedMesh *mesh = &skinned_model->meshes.items[i];

        for (u32 k = 0; k < mesh->submeshes.len; k++) {
          SkinnedSubMesh *submesh = &mesh->submeshes.items[k];
          Handle mesh_handle = submesh->mesh_handle;
          Handle material_handle = submesh->material_handle;

          if (handle_is_valid(mesh_handle) &&
              handle_is_valid(material_handle)) {
            renderer_draw_skinned_mesh(mesh_handle, material_handle, rot_matrix,
                                       joint_transforms, num_joints, NULL);

            static int frame_count = 0;
            if (frame_count++ % 60 == 0) {
              LOG_INFO("Drew submesh % of mesh %", FMT_UINT(k), FMT_UINT(i));
            }
          }
        }
      }
    } else {
      // Use existing joint transforms from the model
      for (u32 i = 0; i < skinned_model->meshes.len; i++) {
        SkinnedMesh *mesh = &skinned_model->meshes.items[i];

        for (u32 k = 0; k < mesh->submeshes.len; k++) {
          SkinnedSubMesh *submesh = &mesh->submeshes.items[k];
          Handle mesh_handle = submesh->mesh_handle;
          Handle material_handle = submesh->material_handle;

          if (handle_is_valid(mesh_handle) &&
              handle_is_valid(material_handle)) {
            renderer_draw_skinned_mesh(mesh_handle, material_handle, rot_matrix,
                                       skinned_model->joint_matrices.items,
                                       skinned_model->joint_matrices.len, NULL);
          }
        }
      }
    }
  }

  input_end_frame(input);
  ALLOC_RESET(&gym_state->ctx.temp_allocator);
}