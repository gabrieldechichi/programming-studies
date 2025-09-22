#include "game.h"
#include "assets.h"
#include "context.h"
#include "lib/handle.h"
#include "lib/math.h"
#include "lib/memory.h"
#include "lib/string.h"
#include "lib/typedefs.h"
#include "platform/platform.h"
#include "renderer/renderer.h"

// Global handles for testing
internal Handle g_triangle_mesh_handle = INVALID_HANDLE;
internal Handle g_triangle_material_handle = INVALID_HANDLE;

// Global game context and asset system
internal GameContext g_game_context;
internal AssetSystem g_asset_system;
internal Texture_Handle g_white_texture_handle;
internal Handle g_triangle_shader_handle = INVALID_HANDLE;
internal Model3DData_Handle g_anya_model_handle = {0};
internal ArenaAllocator permanent_arena = {0};
internal ArenaAllocator temporary_arena = {0};

void gym_init(GameMemory *memory) {
  LOG_INFO("Game initialized");
  LOG_INFO("Permanent memory: % MB",
           FMT_FLOAT(BYTES_TO_MB(memory->pernament_memory_size)));
  LOG_INFO("Temporary memory: % MB",
           FMT_FLOAT(BYTES_TO_MB(memory->temporary_memory_size)));

  // Initialize arena allocators from GameMemory
  permanent_arena = arena_from_buffer((u8 *)memory->permanent_memory,
                                      memory->pernament_memory_size);
  temporary_arena = arena_from_buffer((u8 *)memory->temporary_memory,
                                      memory->temporary_memory_size);

  // Create GameContext with allocators
  g_game_context.allocator = make_arena_allocator(&permanent_arena);
  g_game_context.temp_allocator = make_arena_allocator(&temporary_arena);

  // Initialize asset system
  g_asset_system = asset_system_init(&g_game_context.allocator, 1024);

  // Load white pixel texture
  g_white_texture_handle = asset_request(
      Texture, &g_asset_system, &g_game_context, "assets/white_pixel.png");

  LOG_INFO("Asset system initialized with texture handle: idx=%, gen=%",
           FMT_UINT(g_white_texture_handle.idx),
           FMT_UINT(g_white_texture_handle.gen));

  // Test shader loading
  LoadShaderParams shader_params = {.shader_name = "triangle"};
  g_triangle_shader_handle = load_shader(shader_params);

  if (handle_is_valid(g_triangle_shader_handle)) {
    LOG_INFO("Triangle shader loaded successfully! Handle: idx=%, gen=%",
             FMT_UINT(g_triangle_shader_handle.idx),
             FMT_UINT(g_triangle_shader_handle.gen));
  } else {
    LOG_INFO("Failed to load triangle shader");
  }

  // Test loading invalid shader
  LoadShaderParams invalid_params = {.shader_name = "nonexistent_shader"};
  Handle invalid_handle = load_shader(invalid_params);

  if (handle_is_valid(invalid_handle)) {
    LOG_INFO("ERROR: Invalid shader was loaded!");
  } else {
    LOG_INFO("Correctly rejected invalid shader 'nonexistent_shader'");
  }

  // Test submesh creation with simple triangle data
  f32 triangle_vertices[] = {
      // positions    colors
      0.0f,  0.5f,  1.0f, 0.0f, 0.0f, 1.0f, // top (red)
      0.5f,  -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, // bottom right (green)
      -0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 1.0f  // bottom left (blue)
  };

  u32 triangle_indices[] = {0, 1, 2};

  SubMeshData triangle_mesh = {
      .len_vertices = 3,
      .vertex_stride = 6 * sizeof(f32), // position(2) + color(4)
      .len_vertex_buffer = 18,          // 3 vertices * 6 floats per vertex
      .vertex_buffer = (u8 *)triangle_vertices,
      .len_indices = 3,
      .indices = triangle_indices,
      .len_blendshapes = 0,
      .blendshape_deltas = NULL,
      .material_path = {0} // Empty string
  };

  g_triangle_mesh_handle = renderer_create_submesh(&triangle_mesh, false);

  if (handle_is_valid(g_triangle_mesh_handle)) {
    LOG_INFO("Triangle mesh created successfully! Handle: idx=%, gen=%",
             FMT_UINT(g_triangle_mesh_handle.idx),
             FMT_UINT(g_triangle_mesh_handle.gen));
  } else {
    LOG_INFO("Failed to create triangle mesh");
  }

  // Test with invalid mesh data (null vertex buffer)
  SubMeshData invalid_mesh = {.len_vertices = 0,
                              .vertex_buffer = NULL,
                              .len_indices = 0,
                              .indices = NULL};

  Handle invalid_mesh_handle = renderer_create_submesh(&invalid_mesh, false);

  if (handle_is_valid(invalid_mesh_handle)) {
    LOG_INFO("ERROR: Invalid mesh was created!");
  } else {
    LOG_INFO("Correctly rejected invalid mesh data");
  }

  // Store shader handle for later material creation (after texture loads)
  // We'll create the material in the update loop once texture is ready

  // Load Anya 3D model
  g_anya_model_handle = asset_request(
      Model3DData, &g_asset_system, &g_game_context, "assets/anya/anya.hasset");
  LOG_INFO("Requested Anya model load, handle: idx=%, gen=%",
           FMT_UINT(g_anya_model_handle.idx),
           FMT_UINT(g_anya_model_handle.gen));
}

void gym_update_and_render(GameMemory *memory) {
  // Update asset system (processes pending loads)
  asset_system_update(&g_asset_system, &g_game_context);

  // Clear the screen with a dark blue color
  Color clear_color = color_from_rgba(0.1f, 0.1f, 0.3f, 1.0f);
  renderer_clear(clear_color);

  // Check model loading status and log mesh names once ready
  static b32 model_logged = false;
  if (!model_logged && asset_is_ready(&g_asset_system, g_anya_model_handle)) {
    Model3DData *model =
        asset_get_data(Model3DData, &g_asset_system, g_anya_model_handle);
    if (model) {
      LOG_INFO("Anya model loaded successfully!");
      LOG_INFO("Model version: %", FMT_UINT(model->version));
      LOG_INFO("Number of meshes: %", FMT_UINT(model->num_meshes));
      LOG_INFO("Number of joints: %", FMT_UINT(model->len_joints));

      // Log all mesh names
      for (u32 i = 0; i < model->num_meshes; i++) {
        MeshData *mesh = &model->meshes[i];
        LOG_INFO("Mesh [%]: % (% submeshes, % blendshapes)", FMT_UINT(i),
                 FMT_STR(mesh->mesh_name.value), FMT_UINT(mesh->submeshes.len),
                 FMT_UINT(mesh->blendshape_names.len));
      }
    }
    model_logged = true;
  }

  // Check texture loading status and create material once ready
  static b32 material_created = false;
  if (!material_created &&
      asset_is_ready(&g_asset_system, g_white_texture_handle)) {
    Texture *texture =
        asset_get_data(Texture, &g_asset_system, g_white_texture_handle);
    if (texture) {
      LOG_INFO(
          "White pixel texture loaded! Size: %x%, GPU handle: idx=%, gen=%",
          FMT_UINT(texture->image.width), FMT_UINT(texture->image.height),
          FMT_UINT(texture->gpu_tex_handle.idx),
          FMT_UINT(texture->gpu_tex_handle.gen));

      // Create material with both color and texture properties
      MaterialProperty props[] = {
          {
              .name = {.len = 14, .value = "material_color"},
              .type = MAT_PROP_VEC3,
              .value.vec3_val = {1.0f, 0.0f, 0.0f}
              // White color to show texture
          },
          {.name = {.len = 7, .value = "texture"},
           .type = MAT_PROP_TEXTURE,
           .value.texture = g_white_texture_handle}};

      g_triangle_material_handle =
          load_material(g_triangle_shader_handle, props, 2, false);

      if (handle_is_valid(g_triangle_material_handle)) {
        LOG_INFO(
            "Material with texture loaded successfully! Handle: idx=%, gen=%",
            FMT_UINT(g_triangle_material_handle.idx),
            FMT_UINT(g_triangle_material_handle.gen));
      } else {
        LOG_INFO("Failed to load material with texture");
      }
    }
    material_created = true;
  }

  // Draw the triangle mesh if we have valid handles
  if (handle_is_valid(g_triangle_mesh_handle) &&
      handle_is_valid(g_triangle_material_handle)) {
    mat4 model_matrix;
    mat4_identity(model_matrix);
    renderer_draw_mesh(g_triangle_mesh_handle, g_triangle_material_handle,
                       model_matrix);
  }

  // Process input events
  for (u32 i = 0; i < memory->input_events.len; i++) {
    GameInputEvent *event = &memory->input_events.events[i];

    if (event->type == EVENT_KEYDOWN) {
      switch (event->key.type) {
      case KEY_W:
        LOG_INFO("W key pressed");
        break;
      case KEY_A:
        LOG_INFO("A key pressed");
        break;
      case KEY_S:
        LOG_INFO("S key pressed");
        break;
      case KEY_D:
        LOG_INFO("D key pressed");
        break;
      case KEY_SPACE:
        LOG_INFO("SPACE key pressed");
        break;
      case MOUSE_LEFT:
        LOG_INFO("Left mouse button pressed at (%, %)",
                 FMT_FLOAT(memory->input_events.mouse_x),
                 FMT_FLOAT(memory->input_events.mouse_y));
        break;
      case MOUSE_RIGHT:
        LOG_INFO("Right mouse button pressed");
        break;
      case MOUSE_MIDDLE:
        LOG_INFO("Middle mouse button pressed");
        break;
      default:
        break;
      }
    } else if (event->type == EVENT_KEYUP) {
      switch (event->key.type) {
      case KEY_W:
        LOG_INFO("W key released");
        break;
      case KEY_A:
        LOG_INFO("A key released");
        break;
      case KEY_S:
        LOG_INFO("S key released");
        break;
      case KEY_D:
        LOG_INFO("D key released");
        break;
      case KEY_SPACE:
        LOG_INFO("SPACE key released");
        break;
      default:
        break;
      }
    } else if (event->type == EVENT_TOUCH_START) {
      LOG_INFO("Touch started: id=%, pos=(%, %)", FMT_UINT(event->touch.id),
               FMT_FLOAT(event->touch.x), FMT_FLOAT(event->touch.y));
    }
  }

  // Only log timing info occasionally to avoid spam
  local_persist f32 last_log_time = 0.0f;
  if (memory->time.now - last_log_time > 5.0f) { // Log every 5 seconds
    LOG_INFO("Game running - time: %, dt: %, canvas: %x%",
             FMT_FLOAT(memory->time.now), FMT_FLOAT(memory->time.dt),
             FMT_UINT(memory->canvas.width), FMT_UINT(memory->canvas.height));
    last_log_time = memory->time.now;
  }
}
