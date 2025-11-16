#include "animation.h"
#include "animation_system.h"
#include "assets.h"
#include "camera.h"
#include "clay/clay.h"
#include "context.h"
#include "game.h"
#include "lib/array.h"
#include "lib/audio.h"
#include "lib/handle.h"
#include "lib/lipsync_algs.h"
#include "lib/math.h"
#include "lib/memory.h"
#include "lib/string.h"
#include "lib/typedefs.h"
#include "platform/platform.h"
#include "renderer/renderer.h"
#include <string.h>

typedef struct {
  // memory
  ArenaAllocator permanent_arena;
  ArenaAllocator temporary_arena;
  GameContext ctx;

  // systems
  AssetSystem asset_system;
  Camera camera;
  GameInput input;

  Model3DData_Handle anya_model_handle;
  Handle *model_mesh_handles; // Array of mesh handles from model
  MaterialAsset_Handle
      *submesh_material_asset_handles; // Array of material asset handles
  Handle
      *submesh_material_handles; // Array of material handles for each submesh
  u32 num_model_meshes;

// Animation system
#define NUM_ANIMATIONS 5
  AnimationAsset_Handle animation_handles[NUM_ANIMATIONS];
  Animation *animations[NUM_ANIMATIONS];
  AnimatedEntity animated_entity;
  SkinnedModel skinned_model;
  b32 animation_initialized;
  u32 current_animation_index;
  b32 animations_loaded[NUM_ANIMATIONS];
  b32 all_animations_converted;
} GymState;

global GameContext *g_ctx;

extern GameContext *get_global_ctx() { return g_ctx; }

void gym_init(GameMemory *memory) {
  GymState *gym_state = cast(GymState *) memory->permanent_memory;
  g_ctx = &gym_state->ctx;

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

  // Initialize asset system
  gym_state->asset_system = asset_system_init(&gym_state->ctx.allocator, 1024);

  // Initialize camera
  gym_state->camera = (Camera){
      .pos = {0.0, 1.0, 2.0},
      .pitch = 0.0f,
      .yaw = 0.0f,
      .fov = 60.0f,
      .arm = 3.0f,
      .orbit_center = {0.5, 1.0, 0},
  };
  quat_identity(gym_state->camera.rot);

  gym_state->anya_model_handle =
      asset_request(Model3DData, &gym_state->asset_system, &gym_state->ctx,
                    "assets/generic_female/generic_female.hasset");

  // Load all animations
  const char *animation_paths[NUM_ANIMATIONS] = {
      "assets/generic_female/Generic Female - Idle.hasset",
      "assets/generic_female/Generic Female - Look Around.hasset",
      "assets/generic_female/Generic Female - Walking.hasset",
      "assets/generic_female/Generic Female - Angry.hasset",
      "assets/generic_female/Generic Female - Hip Hop Dancing.hasset"};

  for (u32 i = 0; i < NUM_ANIMATIONS; i++) {
    gym_state->animation_handles[i] =
        asset_request(AnimationAsset, &gym_state->asset_system, &gym_state->ctx,
                      animation_paths[i]);
    gym_state->animations[i] = NULL;
    gym_state->animations_loaded[i] = false;
    LOG_INFO("Requested animation [%] load: %, handle: idx=%, gen=%",
             FMT_UINT(i), FMT_STR(animation_paths[i]),
             FMT_UINT(gym_state->animation_handles[i].idx),
             FMT_UINT(gym_state->animation_handles[i].gen));
  }

  // Initialize model mesh handles
  gym_state->model_mesh_handles = NULL;
  gym_state->submesh_material_asset_handles = NULL;
  gym_state->submesh_material_handles = NULL;
  gym_state->num_model_meshes = 0;

  // Initialize animation system
  gym_state->animation_initialized = false;
  gym_state->current_animation_index = 1; // Start with Look Around
  gym_state->all_animations_converted = false;
}

// Global variable to track pressed button state
global int pressed_button_index = -1;

// Forward declaration
void switch_animation(u32 animation_index);

// Global pointer to gym state for animation switching
static GymState *g_gym_state = NULL;

void switch_animation(u32 animation_index) {
  if (!g_gym_state || !g_gym_state->animation_initialized ||
      animation_index >= NUM_ANIMATIONS) {
    return;
  }

  if (animation_index != g_gym_state->current_animation_index) {
    Animation *new_animation = g_gym_state->animations[animation_index];
    if (new_animation) {
      animated_entity_play_animation(&g_gym_state->animated_entity,
                                     new_animation, 0.35f, 1.0f,
                                     true); // 0.35s transition
      g_gym_state->current_animation_index = animation_index;

      const char *anim_names[] = {"Idle", "Look Around", "Walking", "Angry",
                                  "Hip Hop Dance"};
      LOG_INFO("Switched to animation: %",
               FMT_STR(anim_names[animation_index]));
    }
  }
}

// Animation button click handler
void HandleAnimationButtonClick(Clay_ElementId elementId,
                                Clay_PointerData pointerData,
                                intptr_t userData) {
  (void)elementId; // Suppress unused parameter warning
  int buttonIndex = (int)userData;

  // Animation names for logging
  static const char *animationNames[] = {"Idle", "Look Around", "Walking",
                                         "Angry", "Hip Hop Dance"};

  if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
    pressed_button_index = buttonIndex;
    LOG_INFO("CLAY_POINTER_DATA_PRESSED_THIS_FRAME: %",
             FMT_STR(animationNames[buttonIndex]));

    // Switch to the selected animation
    switch_animation((u32)buttonIndex);
  } else if (pointerData.state == CLAY_POINTER_DATA_RELEASED_THIS_FRAME) {
    pressed_button_index = -1;
  }
}

#define clay_color_mul(c, v) ((Clay_Color){c.r * v, c.g * v, c.b * v, c.a});

// Create Clay UI
internal Clay_RenderCommandArray create_ui() {
  Clay_BeginLayout();

  const Clay_Color COLOR_PANEL_BG = hex_to_rgba_255(0xFFFFFF);  // White
  const Clay_Color COLOR_BUTTON_BG = hex_to_rgba_255(0xF5F5F5); // Light gray
  const Clay_Color COLOR_BUTTON_HOVER =
      hex_to_rgba_255(0xD9D9D9); // Medium gray
  const Clay_Color COLOR_BUTTON_PRESSED = hex_to_rgba_255(0x5c5c5c);

  const Clay_Color COLOR_BORDER = clay_color_mul(
      (Clay_Color)hex_to_rgba_255(0xD9D9D9), 0.9); // Medium gray border
  const Clay_Color COLOR_TEXT =
      clay_color_mul((Clay_Color)hex_to_rgba_255(0x5c5c5c), 1.0);

  // Animation list
  static const char *animationNames[] = {"Idle", "Look Around", "Walking",
                                         "Angry", "Hip Hop Dance"};

  // Main container - fills the entire screen, horizontal layout
  CLAY({.id = CLAY_ID("MainContainer"),
        .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                   .layoutDirection = CLAY_LEFT_TO_RIGHT}}) {

    // Left content area - grows to fill remaining space
    CLAY({.id = CLAY_ID("ContentArea"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}}}) {
      // This is where the main 3D content renders
    }

    // Right panel - fixed 200px width
    CLAY(
        {
            .id = CLAY_ID("AnimationPanel"),
            .layout = {.sizing = {CLAY_SIZING_FIXED(200), CLAY_SIZING_GROW(0)},
                       .layoutDirection = CLAY_TOP_TO_BOTTOM,
                       .padding = CLAY_PADDING_ALL(10),
                       .childGap = 5},
            .backgroundColor = COLOR_PANEL_BG,
            .border = {.width = {.left = 2}, .color = COLOR_BORDER},
        },

    ) {

      // Panel title
      CLAY({
          .id = CLAY_ID("PanelTitle"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(30)},
                     .padding = CLAY_PADDING_ALL(2),
                     .childAlignment = {.x = CLAY_ALIGN_X_LEFT,
                                        .y = CLAY_ALIGN_Y_CENTER}},
      }) {
        CLAY_TEXT(CLAY_STRING("Animations"),
                  CLAY_TEXT_CONFIG(
                      {.fontId = 0, .fontSize = 16, .textColor = COLOR_TEXT}));
      }

      // Animation buttons
      for (int i = 0; i < NUM_ANIMATIONS; i++) {
        CLAY({
            .id = CLAY_IDI("AnimButton", i),
            .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(35)},
                       .padding = CLAY_PADDING_ALL(8),
                       .childAlignment = {CLAY_ALIGN_X_CENTER,
                                          CLAY_ALIGN_Y_CENTER}},
            .backgroundColor =
                (Clay_Hovered() && pressed_button_index == i)
                    ? COLOR_BUTTON_PRESSED
                    : (Clay_Hovered() ? COLOR_BUTTON_HOVER : COLOR_BUTTON_BG),
            .cornerRadius = CLAY_CORNER_RADIUS(4),
        }) {

          Clay_OnHover(HandleAnimationButtonClick, (intptr_t)i);
          Clay_String animText = {.chars = animationNames[i],
                                  .length = strlen(animationNames[i]),
                                  .isStaticallyAllocated = true};
          CLAY_TEXT(animText, CLAY_TEXT_CONFIG({.fontId = 0,
                                                .fontSize = 14,
                                                .textColor = COLOR_TEXT}));
        }
      }
    }
  }

  return Clay_EndLayout();
}

// Setup blendshape params (testing with 1 blendshape)
BlendshapeParams blendshape_params = {
    .count = 1,                  // 1 blendshape active
    .weights = {{0.0f, 0, 0, 0}} // First weight = 1.0, others = 0
};

void gym_update_and_render(GameMemory *memory) {
  GymState *gym_state = cast(GymState *) memory->permanent_memory;
  // todo: remove this
  g_gym_state = gym_state; // Set global pointer for animation switching
  GameContext *ctx = &gym_state->ctx;
  Camera *camera = &gym_state->camera;
  AssetSystem *asset_system = &gym_state->asset_system;
  GameInput *input = &gym_state->input;

  asset_system_update(asset_system, ctx);

  camera_update_uniforms(camera, (f32)memory->canvas.width,
                         (f32)memory->canvas.height);

  Color clear_color = color_from_hex(0xebebeb);
  renderer_clear(clear_color);

  local_persist b32 model_loaded = false;
  if (!model_loaded &&
      asset_is_ready(asset_system, gym_state->anya_model_handle)) {
    Model3DData *model =
        asset_get_data(Model3DData, asset_system, gym_state->anya_model_handle);
    if (model) {
      LOG_INFO("Anya model loaded successfully!");

      // Count total submeshes
      u32 total_submeshes = 0;
      for (u32 i = 0; i < model->num_meshes; i++) {
        total_submeshes += model->meshes[i].submeshes.len;
      }

      // Allocate mesh and material handles arrays
      gym_state->model_mesh_handles =
          ALLOC_ARRAY(&ctx->allocator, Handle, total_submeshes);
      gym_state->submesh_material_asset_handles =
          ALLOC_ARRAY(&ctx->allocator, MaterialAsset_Handle, total_submeshes);
      gym_state->submesh_material_handles =
          ALLOC_ARRAY(&ctx->allocator, Handle, total_submeshes);
      gym_state->num_model_meshes = total_submeshes;

      // Create renderer mesh handles for all submeshes
      u32 handle_idx = 0;
      for (u32 mesh_idx = 0; mesh_idx < model->num_meshes; mesh_idx++) {
        MeshData *mesh = &model->meshes[mesh_idx];
        LOG_INFO("Processing mesh [%]: % (% submeshes)", FMT_UINT(mesh_idx),
                 FMT_STR(mesh->mesh_name.value), FMT_UINT(mesh->submeshes.len));

        for (u32 sub_idx = 0; sub_idx < mesh->submeshes.len; sub_idx++) {
          SubMeshData *submesh = &mesh->submeshes.items[sub_idx];
          Handle mesh_handle =
              renderer_create_submesh(submesh, true /*skinned?*/);
          gym_state->model_mesh_handles[handle_idx] = mesh_handle;

          // Load material asset for this submesh
          if (submesh->material_path.len > 0) {
            MaterialAsset_Handle material_asset_handle = asset_request(
                MaterialAsset, asset_system, ctx, submesh->material_path.value);
            gym_state->submesh_material_asset_handles[handle_idx] =
                material_asset_handle;
            LOG_INFO("Requested material load [%]: %, handle: idx=%, gen=%",
                     FMT_UINT(handle_idx),
                     FMT_STR(submesh->material_path.value),
                     FMT_UINT(material_asset_handle.idx),
                     FMT_UINT(material_asset_handle.gen));
          } else {
            // No material specified, set invalid handle
            gym_state->submesh_material_asset_handles[handle_idx] =
                (MaterialAsset_Handle){0};
            LOG_WARN("No material path for submesh [%]", FMT_UINT(handle_idx));
          }

          if (handle_is_valid(mesh_handle)) {
            LOG_INFO("Created mesh handle [%]: idx=%, gen=%",
                     FMT_UINT(handle_idx), FMT_UINT(mesh_handle.idx),
                     FMT_UINT(mesh_handle.gen));
          } else {
            LOG_INFO("Failed to create mesh handle [%]", FMT_UINT(handle_idx));
          }
          handle_idx++;
        }
      }
    }
    model_loaded = true;
  }

  // Process material assets and load their texture dependencies
  local_persist b32 materials_processed = false;
  if (!materials_processed && gym_state->submesh_material_asset_handles &&
      gym_state->num_model_meshes > 0) {
    b32 all_material_assets_ready = true;

    // Check if all material assets are loaded
    for (u32 i = 0; i < gym_state->num_model_meshes; i++) {
      MaterialAsset_Handle mat_handle =
          gym_state->submesh_material_asset_handles[i];
      if (mat_handle.idx != 0 && !asset_is_ready(asset_system, mat_handle)) {
        all_material_assets_ready = false;
        break;
      }
    }

    if (all_material_assets_ready) {
      // Process each material asset and load textures
      for (u32 i = 0; i < gym_state->num_model_meshes; i++) {
        MaterialAsset_Handle mat_handle =
            gym_state->submesh_material_asset_handles[i];
        if (mat_handle.idx != 0) {
          MaterialAsset *mat_asset =
              asset_get_data(MaterialAsset, asset_system, mat_handle);
          if (mat_asset) {
            LOG_INFO("Processing material [%]: %", FMT_UINT(i),
                     FMT_STR(mat_asset->name.value));

            // Load textures for this material's properties
            for (u32 prop_idx = 0; prop_idx < mat_asset->properties.len;
                 prop_idx++) {
              MaterialAssetProperty *prop =
                  &mat_asset->properties.items[prop_idx];
              if (prop->type == MAT_PROP_TEXTURE) {
                // Request texture loading
                Texture_Handle tex_handle = asset_request(
                    Texture, asset_system, ctx, prop->texture_path.value);
                LOG_INFO("Requested texture load for material [%] property "
                         "'%': %, handle: idx=%, gen=%",
                         FMT_UINT(i), FMT_STR(prop->name.value),
                         FMT_STR(prop->texture_path.value),
                         FMT_UINT(tex_handle.idx), FMT_UINT(tex_handle.gen));
              }
            }
          }
        }
      }
      materials_processed = true;
      LOG_INFO("All material assets processed, texture loading initiated");
    }
  }

  // Convert MaterialAssets to Materials once textures are loaded
  local_persist b32 materials_created = false;
  if (materials_processed && !materials_created &&
      gym_state->submesh_material_asset_handles &&
      gym_state->num_model_meshes > 0) {
    b32 all_textures_ready = true;

    // Check if all texture dependencies are loaded
    for (u32 i = 0; i < gym_state->num_model_meshes; i++) {
      MaterialAsset_Handle mat_handle =
          gym_state->submesh_material_asset_handles[i];
      if (mat_handle.idx != 0) {
        MaterialAsset *mat_asset =
            asset_get_data(MaterialAsset, asset_system, mat_handle);
        if (mat_asset) {
          // Check if all textures for this material are ready
          for (u32 prop_idx = 0; prop_idx < mat_asset->properties.len;
               prop_idx++) {
            MaterialAssetProperty *prop =
                &mat_asset->properties.items[prop_idx];
            if (prop->type == MAT_PROP_TEXTURE) {
              Texture_Handle tex_handle = asset_request(
                  Texture, asset_system, ctx, prop->texture_path.value);
              if (!asset_is_ready(asset_system, tex_handle)) {
                all_textures_ready = false;
                break;
              }
            }
          }
          if (!all_textures_ready)
            break;
        }
      }
    }

    if (all_textures_ready) {
      // Create materials from assets
      for (u32 i = 0; i < gym_state->num_model_meshes; i++) {
        MaterialAsset_Handle mat_handle =
            gym_state->submesh_material_asset_handles[i];
        if (mat_handle.idx != 0) {
          MaterialAsset *mat_asset =
              asset_get_data(MaterialAsset, asset_system, mat_handle);
          if (mat_asset) {
            // Load shader for this material
            // LoadShaderParams shader_params = {.shader_name =
            //                                       mat_asset->shader_path.value};
            LoadShaderParams shader_params = {.shader_name = "triangle"};
            Handle shader_handle = load_shader(shader_params);
            debug_assert_msg(handle_is_valid(shader_handle),
                             "Couldn't load shader for path %",
                             FMT_STR(mat_asset->shader_path.value));

            // Build material properties with loaded textures
            MaterialProperty *props =
                ALLOC_ARRAY(&ctx->temp_allocator, MaterialProperty,
                            mat_asset->properties.len);

            for (u32 prop_idx = 0; prop_idx < mat_asset->properties.len;
                 prop_idx++) {
              MaterialAssetProperty *asset_prop =
                  &mat_asset->properties.items[prop_idx];
              MaterialProperty *mat_prop = &props[prop_idx];

              mat_prop->name = asset_prop->name;
              mat_prop->type = asset_prop->type;

              if (asset_prop->type == MAT_PROP_TEXTURE) {
                Texture_Handle tex_handle = asset_request(
                    Texture, asset_system, ctx, asset_prop->texture_path.value);
                Texture *tex =
                    asset_get_data_unsafe(Texture, asset_system, tex_handle);
                assert(tex);
                assert(handle_is_valid(tex->gpu_tex_handle));
                mat_prop->value.texture =
                    cast_handle(Texture_Handle, tex->gpu_tex_handle);
              } else if (asset_prop->type == MAT_PROP_VEC3) {
                mat_prop->value.vec3_val[0] = asset_prop->color.r;
                mat_prop->value.vec3_val[1] = asset_prop->color.g;
                mat_prop->value.vec3_val[2] = asset_prop->color.b;
              }
            }

            // Create the material
            gym_state->submesh_material_handles[i] =
                load_material(shader_handle, props, mat_asset->properties.len,
                              mat_asset->transparent);

            if (handle_is_valid(gym_state->submesh_material_handles[i])) {
              LOG_INFO("Created material [%]: %, handle: idx=%, gen=%",
                       FMT_UINT(i), FMT_STR(mat_asset->name.value),
                       FMT_UINT(gym_state->submesh_material_handles[i].idx),
                       FMT_UINT(gym_state->submesh_material_handles[i].gen));
            } else {
              LOG_WARN("Failed to create material [%]: %", FMT_UINT(i),
                       FMT_STR(mat_asset->name.value));
            }
          }
        } else {
          // No material asset, set invalid handle
          gym_state->submesh_material_handles[i] = (Handle){0};
        }
      }
      materials_created = true;
      LOG_INFO("All materials created from assets");
    }
  }

  // Convert animation assets to animations once loaded
  if (materials_created && !gym_state->all_animations_converted) {
    Model3DData *model_data =
        asset_get_data(Model3DData, asset_system, gym_state->anya_model_handle);

    if (model_data) {
      b32 all_ready = true;
      for (u32 i = 0; i < NUM_ANIMATIONS; i++) {
        if (!gym_state->animations_loaded[i] &&
            asset_is_ready(asset_system, gym_state->animation_handles[i])) {
          AnimationAsset *anim_asset = asset_get_data(
              AnimationAsset, asset_system, gym_state->animation_handles[i]);
          if (anim_asset) {
            gym_state->animations[i] =
                animation_from_asset(anim_asset, model_data, &ctx->allocator);
            gym_state->animations_loaded[i] = true;
            LOG_INFO("Converted animation [%] to runtime format", FMT_UINT(i));
          }
        }
        if (!gym_state->animations_loaded[i]) {
          all_ready = false;
        }
      }

      if (all_ready) {
        gym_state->all_animations_converted = true;
        LOG_INFO("All animations converted to runtime format");
      }
    }
  }

  // Initialize animation system after all animations are converted
  if (materials_created && gym_state->all_animations_converted &&
      !gym_state->animation_initialized) {

    Model3DData *model_data =
        asset_get_data(Model3DData, asset_system, gym_state->anya_model_handle);

    if (model_data) {
      LOG_INFO("Initializing animation system...");

      // Initialize AnimatedEntity
      animated_entity_init(&gym_state->animated_entity, model_data,
                           &ctx->allocator);

      // Create SkinnedModel manually
      gym_state->skinned_model.meshes =
          arr_new_ALLOC(&ctx->allocator, SkinnedMesh, model_data->num_meshes);
      gym_state->skinned_model.joint_matrices =
          arr_new_ALLOC(&ctx->allocator, mat4, model_data->len_joints);

      // Create SkinnedMesh entries for each mesh
      for (u32 i = 0; i < model_data->num_meshes; i++) {
        MeshData *mesh_data = &model_data->meshes[i];
        SkinnedMesh skinned_mesh = {0};
        skinned_mesh.submeshes = arr_new_ALLOC(&ctx->allocator, SkinnedSubMesh,
                                               mesh_data->submeshes.len);

        u32 submesh_idx = 0;
        for (u32 j = 0; j < mesh_data->submeshes.len; j++) {
          u32 global_submesh_idx =
              i * mesh_data->submeshes.len + j; // Simplified indexing
          if (global_submesh_idx < gym_state->num_model_meshes &&
              submesh_idx < skinned_mesh.submeshes.len) {
            skinned_mesh.submeshes.items[submesh_idx] = (SkinnedSubMesh){
                .mesh_handle =
                    gym_state->model_mesh_handles[global_submesh_idx],
                .material_handle =
                    gym_state->submesh_material_handles[global_submesh_idx]};
            submesh_idx++;
          }
        }
        gym_state->skinned_model.meshes.items[i] = skinned_mesh;
      }

      // Initialize joint matrices to identity
      for (u32 i = 0; i < model_data->len_joints; i++) {
        mat4_identity(gym_state->skinned_model.joint_matrices.items[i]);
      }

      // Start with Look Around animation (index 1)
      Animation *start_animation =
          gym_state->animations[gym_state->current_animation_index];
      animated_entity_play_animation(&gym_state->animated_entity,
                                     start_animation, 0.0f, 1.0f, true);

      gym_state->animation_initialized = true;
      LOG_INFO("Animation system initialized with Look Around animation");
    }
  }

  input_update(input, &memory->input_events, memory->time.now);
  camera_update(camera, input, memory->time.dt);

  // Set up directional light pointing in camera direction
  DirectionalLightBlock lights = {
      .count = 1.0f,
      .lights[0] = {
          .direction = {0.0f, 0.0f,
                        1.0f}, // Forward direction (will be rotated by camera)
          .color = {1.0f, 1.0f, 1.0f},
          .intensity = 1.25f}};

  // Apply camera rotation to light direction
  vec3 forward = {0.0f, 0.0f, 1.0f};
  glm_quat_rotatev(camera->rot, forward, lights.lights[0].direction);

  renderer_set_lights(&lights);

  // Update animation and draw the model
  if (gym_state->animation_initialized && materials_created) {
    Model3DData *model_data =
        asset_get_data(Model3DData, asset_system, gym_state->anya_model_handle);

    // Update animation
    animated_entity_update(&gym_state->animated_entity, memory->time.dt);
    animated_entity_evaluate_pose(&gym_state->animated_entity, model_data);
    animated_entity_apply_pose(&gym_state->animated_entity, model_data,
                               &gym_state->skinned_model);

    for (u32 j = 0; j < 1; j++) {
      // Render all model submeshes with animated joint matrices
      mat4 model_matrix;
      quaternion rot;
      quat_from_euler(VEC3(glm_rad(90), 0, 0), rot);
      mat_trs(VEC3(j * 0.1, 0, 0), rot, VEC3(0.01, 0.01, 0.01), model_matrix);
      for (u32 i = 0; i < gym_state->num_model_meshes; i++) {
        Handle mesh_handle = gym_state->model_mesh_handles[i];
        Handle material_handle = gym_state->submesh_material_handles[i];

        if (handle_is_valid(mesh_handle) && handle_is_valid(material_handle)) {
          renderer_draw_skinned_mesh(
              mesh_handle, material_handle, model_matrix,
              gym_state->skinned_model.joint_matrices.items,
              gym_state->skinned_model.joint_matrices.len, &blendshape_params);
        } else if (handle_is_valid(mesh_handle)) {
          // Fallback: skip rendering if no valid material
          LOG_WARN("Skipping submesh [%] - no valid material", FMT_UINT(i));
        }
      }
    }
  }

  Clay_RenderCommandArray ui_commands = create_ui();
  renderer_draw_ui(ui_commands);

  input_end_frame(input);

  ALLOC_RESET(&ctx->temp_allocator);
}
