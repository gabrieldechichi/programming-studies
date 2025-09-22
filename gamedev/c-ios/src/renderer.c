// Renderer system ported from renderer_sokol.c
// Provides high-level rendering abstractions on top of gpu_backend

#include "renderer.h"
#include "gpu_backend.h"
#include "lib/array.h"
#include "lib/handle.h"
#include "lib/profiler.h"
#include "lib/string.h"
#include "lib/string_builder.h"
#include "memory.h"
#include "vendor/cglm/vec3.h"
#include "vendor/cglm/mat4.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define internal static

// ===== Internal Type Definitions =====

// ===== Internal Structures =====

// GPU Shader
typedef struct {
  gpu_pipeline_t *pipeline;
  const char *name;
} GPUShader;

TYPED_HANDLE_DEFINE(GPUShader);
HANDLE_ARRAY_DEFINE(GPUShader);

// GPU SubMesh
typedef struct {
  gpu_buffer_t *vertex_buffer;
  gpu_buffer_t *index_buffer;
  u32 index_count;
  u32 num_blendshapes;
  b32 is_skinned;
  b32 has_blendshapes;
} GPUSubMesh;

TYPED_HANDLE_DEFINE(GPUSubMesh);
HANDLE_ARRAY_DEFINE(GPUSubMesh);

// GPU Material
typedef struct {
  Handle shader_handle;
  MaterialProperty *properties;
  u32 property_count;
  b32 transparent;
} GPUMaterial;

TYPED_HANDLE_DEFINE(GPUMaterial);
HANDLE_ARRAY_DEFINE(GPUMaterial);

// GPU Texture
typedef struct {
  gpu_texture_t *texture;
  u32 width;
  u32 height;
  b32 is_set;
} GPUTexture;

TYPED_HANDLE_DEFINE(GPUTexture);
HANDLE_ARRAY_DEFINE(GPUTexture);

// Additional handle arrays needed from header
HANDLE_ARRAY_DEFINE(Image);
HANDLE_ARRAY_DEFINE(Texture);

// Skinned mesh instance for batching
typedef struct {
  Handle mesh_handle;
  mat4 model_matrix;
  mat4 *joint_transforms;
  u32 num_joints;
  BlendshapeParams *blendshape_params;
} SkinnedMeshInstance;

slice_define(SkinnedMeshInstance);

// Material batch for grouped rendering
typedef struct {
  Handle material_handle;
  SkinnedMeshInstance_Slice instances;
} MaterialBatch;

slice_define(MaterialBatch);

// Main renderer structure
typedef struct {
  // Memory allocators (provided externally)
  Allocator *permanent_allocator;
  Allocator *temp_allocator;

  // GPU device
  gpu_device_t *device;

  // Resource handle arrays
  HandleArray_GPUTexture gpu_textures;
  HandleArray_GPUSubMesh gpu_submeshes;
  HandleArray_GPUMaterial gpu_materials;
  HandleArray_GPUShader gpu_shaders;

  // Current state
  CameraUniformBlock current_camera;
  DirectionalLightBlock current_lights;

  // Command buffer
  RenderCommand_Slice render_cmds;

  b32 initialized;
} Renderer;

// Global renderer instance
static Renderer *g_renderer = NULL;

// ===== Internal Functions =====

internal void add_render_command(RenderCommand command) {
  if (!g_renderer) return;
  slice_append(g_renderer->render_cmds, command);
}

// ===== Public API =====

void renderer_init(gpu_device_t *device, Allocator *permanent_allocator, Allocator *temp_allocator) {
  if (g_renderer) {
    printf("[Renderer] Already initialized\n");
    return;
  }

  // Allocate renderer structure
  g_renderer = ALLOC(permanent_allocator, Renderer);
  memset(g_renderer, 0, sizeof(Renderer));

  g_renderer->permanent_allocator = permanent_allocator;
  g_renderer->temp_allocator = temp_allocator;
  g_renderer->device = device;

  // Initialize command buffer
  g_renderer->render_cmds = slice_new_ALLOC(permanent_allocator, RenderCommand, 4096);

  // Initialize handle arrays
  g_renderer->gpu_textures = ha_init(GPUTexture, permanent_allocator, 32);
  g_renderer->gpu_submeshes = ha_init(GPUSubMesh, permanent_allocator, 64);
  g_renderer->gpu_materials = ha_init(GPUMaterial, permanent_allocator, 32);
  g_renderer->gpu_shaders = ha_init(GPUShader, permanent_allocator, 16);

  // Initialize camera with identity matrices
  glm_mat4_identity(g_renderer->current_camera.view_matrix);
  glm_mat4_identity(g_renderer->current_camera.projection_matrix);
  glm_mat4_identity(g_renderer->current_camera.view_proj_matrix);
  glm_vec3_zero(g_renderer->current_camera.camera_pos);

  // Initialize default light
  g_renderer->current_lights.count = 1.0f;  // One light
  g_renderer->current_lights._padding[0] = 0.0f;
  g_renderer->current_lights._padding[1] = 0.0f;
  g_renderer->current_lights._padding[2] = 0.0f;

  // Light 0 - direction (normalized)
  vec3 light_dir = {0.5f, -1.0f, -0.5f};
  glm_vec3_normalize(light_dir);
  glm_vec3_copy(light_dir, g_renderer->current_lights.lights[0].direction);
  g_renderer->current_lights.lights[0]._padding1 = 0.0f;

  // Light 0 - color and intensity
  g_renderer->current_lights.lights[0].color[0] = 1.0f;  // R
  g_renderer->current_lights.lights[0].color[1] = 1.0f;  // G
  g_renderer->current_lights.lights[0].color[2] = 1.0f;  // B
  g_renderer->current_lights.lights[0].intensity = 1.0f;

  g_renderer->initialized = true;
  printf("[Renderer] Initialized\n");
}

void renderer_cleanup(void) {
  if (!g_renderer) return;

  // TODO: Clean up GPU resources

  g_renderer->initialized = false;
  g_renderer = NULL;
  printf("[Renderer] Cleaned up\n");
}

void renderer_reset_commands(void) {
  if (!g_renderer) return;
  g_renderer->render_cmds.len = 0;

  // Reset temporary allocator
  ALLOC_RESET(g_renderer->temp_allocator);
}

void renderer_clear(Color color) {
  RenderCommand cmd = {
      .type = RENDER_CMD_CLEAR,
      .data.clear = {.color = color}
  };
  add_render_command(cmd);
}

Handle renderer_create_submesh(SubMeshData *mesh_data, b32 is_skinned) {
  if (!g_renderer || !mesh_data || !mesh_data->vertex_buffer) {
    return INVALID_HANDLE;
  }

  // Create vertex buffer
  gpu_buffer_t *vertex_buffer = gpu_create_buffer(
      g_renderer->device,
      mesh_data->vertex_buffer,
      mesh_data->len_vertex_buffer * sizeof(float)
  );

  if (!vertex_buffer) {
    return INVALID_HANDLE;
  }

  // Create index buffer (optional - NULL if no indices)
  gpu_buffer_t *index_buffer = NULL;
  u32 count_for_draw = 0;

  if (mesh_data->indices && mesh_data->len_indices > 0) {
    index_buffer = gpu_create_buffer(
        g_renderer->device,
        mesh_data->indices,
        mesh_data->len_indices * sizeof(u32)
    );

    if (!index_buffer) {
      gpu_destroy_buffer(vertex_buffer);
      return INVALID_HANDLE;
    }
    count_for_draw = mesh_data->len_indices;
  } else {
    // No indices - use vertex count for drawing
    count_for_draw = mesh_data->len_vertices;
  }

  // Store submesh data
  GPUSubMesh new_submesh = {
      .vertex_buffer = vertex_buffer,
      .index_buffer = index_buffer,
      .index_count = count_for_draw,  // Either index count or vertex count
      .num_blendshapes = mesh_data->len_blendshapes,
      .is_skinned = is_skinned,
      .has_blendshapes = mesh_data->len_blendshapes > 0
  };

  // Add to handle array and return the handle
  Handle handle = cast_handle(Handle, ha_add(GPUSubMesh, &g_renderer->gpu_submeshes, new_submesh));
  return handle;
}

// Internal helper to register a shader with a pipeline - used by video_renderer.c
Handle renderer_load_shader(const char *shader_name, gpu_pipeline_t *pipeline) {
  if (!g_renderer || !shader_name || !pipeline) {
    return INVALID_HANDLE;
  }

  GPUShader new_shader = {
      .pipeline = pipeline,
      .name = shader_name
  };

  Handle handle = cast_handle(Handle, ha_add(GPUShader, &g_renderer->gpu_shaders, new_shader));
  return handle;
}

// Public API function matching header
Handle load_shader(LoadShaderParams params) {
  // For now, we only support loading shaders that have already been registered
  // This is a stub that returns invalid handle
  // In practice, video_renderer.c uses renderer_load_shader directly
  return INVALID_HANDLE;
}

Handle load_material(Handle shader_handle, MaterialProperty *properties,
                             u32 property_count, b32 transparent) {
  if (!g_renderer || !handle_is_valid(shader_handle)) {
    return INVALID_HANDLE;
  }

  // Validate shader handle exists
  if (!ha_get(GPUShader, &g_renderer->gpu_shaders, shader_handle)) {
    return INVALID_HANDLE;
  }

  // Copy properties
  MaterialProperty *prop_copy = NULL;
  if (property_count > 0 && properties) {
    prop_copy = ALLOC_ARRAY(g_renderer->permanent_allocator, MaterialProperty, property_count);
    for (u32 i = 0; i < property_count; i++) {
      prop_copy[i] = properties[i];
    }
  }

  GPUMaterial new_material = {
      .shader_handle = shader_handle,
      .properties = prop_copy,
      .property_count = property_count,
      .transparent = transparent
  };

  Handle handle = cast_handle(Handle, ha_add(GPUMaterial, &g_renderer->gpu_materials, new_material));
  return handle;
}

void renderer_draw_mesh(Handle mesh_handle, Handle material_handle,
                       mat4 model_matrix) {
  if (!g_renderer || !handle_is_valid(mesh_handle) || !handle_is_valid(material_handle)) {
    return;
  }

  RenderCommand cmd = {
      .type = RENDER_CMD_DRAW_MESH,
      .data.draw_mesh = {
          .mesh_handle = mesh_handle,
          .material_handle = material_handle,
      }
  };

  glm_mat4_copy(model_matrix, cmd.data.draw_mesh.model_matrix);
  add_render_command(cmd);
}

void renderer_draw_skinned_mesh(Handle mesh_handle, Handle material_handle,
                               mat4 model_matrix, mat4 *joint_transforms,
                               u32 num_joints, BlendshapeParams *blendshape_params) {
  if (!g_renderer || !handle_is_valid(mesh_handle) || !handle_is_valid(material_handle)) {
    return;
  }

  if (!joint_transforms || num_joints == 0) {
    return;
  }

  RenderCommand cmd = {
      .type = RENDER_CMD_DRAW_SKINNED_MESH,
      .data.draw_skinned_mesh = {
          .mesh_handle = mesh_handle,
          .material_handle = material_handle,
          .joint_transforms = joint_transforms,
          .num_joints = num_joints,
          .blendshape_params = blendshape_params,
      }
  };

  glm_mat4_copy(model_matrix, cmd.data.draw_skinned_mesh.model_matrix);
  add_render_command(cmd);
}

void renderer_update_camera(const CameraUniformBlock *camera_uniforms) {
  if (!g_renderer || !camera_uniforms) return;
  g_renderer->current_camera = *camera_uniforms;
}

void renderer_set_lights(const DirectionalLightBlock *lights) {
  if (!g_renderer || !lights) return;
  g_renderer->current_lights = *lights;
}

// Execute accumulated render commands
void renderer_execute_commands(gpu_texture_t *render_target, gpu_command_buffer_t *cmd_buffer) {
  if (!g_renderer || !render_target || !cmd_buffer) return;

  // Process commands and batch skinned meshes
  PROFILE_BEGIN("Process render commands");

  // First pass: handle immediate commands (clear, simple meshes)
  Color clear_color = {.r = 0.0f, .g = 0.0f, .b = 0.0f, .a = 1.0f};
  b32 should_clear = false;

  arr_foreach_ptr(g_renderer->render_cmds, cmd) {
    switch (cmd->type) {
      case RENDER_CMD_CLEAR: {
        // Store clear color for pass begin
        clear_color = cmd->data.clear.color;
        should_clear = true;
      } break;

      case RENDER_CMD_DRAW_MESH: {
        // Get mesh and material
        GPUSubMesh *mesh = ha_get(GPUSubMesh, &g_renderer->gpu_submeshes, cmd->data.draw_mesh.mesh_handle);
        GPUMaterial *material = ha_get(GPUMaterial, &g_renderer->gpu_materials, cmd->data.draw_mesh.material_handle);

        if (!mesh || !material) continue;

        // Get shader
        GPUShader *gpu_shader = ha_get(GPUShader, &g_renderer->gpu_shaders, material->shader_handle);
        if (!gpu_shader || !gpu_shader->pipeline) continue;

        // Begin render pass
        gpu_render_encoder_t *encoder = gpu_begin_render_pass(
            cmd_buffer, render_target,
            clear_color.r, clear_color.g, clear_color.b, clear_color.a
        );

        // Set pipeline
        gpu_set_pipeline(encoder, gpu_shader->pipeline);

        // Set vertex buffer
        gpu_set_vertex_buffer(encoder, mesh->vertex_buffer, 0);

        // Set uniforms (model matrix at index 1)
        gpu_set_uniforms(encoder, 1, &cmd->data.draw_mesh.model_matrix, sizeof(mat4));

        // Draw (for triangle without indices, use vertex count which is 3)
        // Note: if no indices, index_count stores vertex count
        gpu_draw(encoder, mesh->index_count);

        // End render pass
        gpu_end_render_pass(encoder);

      } break;

      case RENDER_CMD_DRAW_SKINNED_MESH: {
        // Get mesh and material
        GPUSubMesh *mesh = ha_get(GPUSubMesh, &g_renderer->gpu_submeshes, cmd->data.draw_skinned_mesh.mesh_handle);
        GPUMaterial *material = ha_get(GPUMaterial, &g_renderer->gpu_materials, cmd->data.draw_skinned_mesh.material_handle);

        if (!mesh || !material) continue;

        // Get shader
        GPUShader *gpu_shader = ha_get(GPUShader, &g_renderer->gpu_shaders, material->shader_handle);
        if (!gpu_shader || !gpu_shader->pipeline) continue;

        // Begin render pass
        gpu_render_encoder_t *encoder = gpu_begin_render_pass(
            cmd_buffer, render_target,
            clear_color.r, clear_color.g, clear_color.b, clear_color.a
        );

        // Set pipeline
        gpu_set_pipeline(encoder, gpu_shader->pipeline);

        // Set vertex buffer
        gpu_set_vertex_buffer(encoder, mesh->vertex_buffer, 0);

        // Update ALL uniforms for toon shader
        if (gpu_shader->pipeline->has_uniforms) {
          // For toon shader, we need to update all uniforms
          vec3 material_color = {1.0f, 1.0f, 0.0f}; // Default white

          //todo: implicity dependency on toon shader
          // Extract material color if available
          for (u32 i = 0; i < material->property_count; i++) {
            if (material->properties[i].type == MAT_PROP_VEC3 &&
                strcmp(material->properties[i].name.value, "uColor") == 0) {
              glm_vec3_copy(material->properties[i].value.vec3_val, material_color);
              break;
            }
          }

          //todo: implicit dependency on toon shader
          // Update uniforms using slot-based API (matching shader bindings)
          gpu_update_uniforms(gpu_shader->pipeline, 0, &g_renderer->current_camera, sizeof(CameraUniformBlock));
          gpu_update_uniforms(gpu_shader->pipeline, 1, cmd->data.draw_skinned_mesh.joint_transforms,
                            sizeof(float) * 16 * cmd->data.draw_skinned_mesh.num_joints);
          gpu_update_uniforms(gpu_shader->pipeline, 2, &cmd->data.draw_skinned_mesh.model_matrix, sizeof(mat4));
          gpu_update_uniforms(gpu_shader->pipeline, 3, material_color, sizeof(vec3));
          gpu_update_uniforms(gpu_shader->pipeline, 4, &g_renderer->current_lights, sizeof(DirectionalLightBlock));
          if (cmd->data.draw_skinned_mesh.blendshape_params) {
            gpu_update_uniforms(gpu_shader->pipeline, 6, cmd->data.draw_skinned_mesh.blendshape_params,
                              sizeof(BlendshapeParams)); // binding 6!
          }
        }

        // Also set model matrix as push constant for compatibility
        gpu_set_uniforms(encoder, 1, &cmd->data.draw_skinned_mesh.model_matrix, sizeof(mat4));

        // Draw
        gpu_draw(encoder, mesh->index_count);

        // End render pass
        gpu_end_render_pass(encoder);
      } break;

      case RENDER_CMD_DRAW_SKYBOX:
        // Skybox rendering not implemented yet
        break;

      default:
        break;
    }
  }
  PROFILE_END();
}

// ===== Stub implementations for new API functions =====

Handle renderer_reserve_texture(void) {
  if (!g_renderer) {
    return INVALID_HANDLE;
  }

  // Reserve a GPU texture slot
  GPUTexture new_texture = {
      .texture = NULL,
      .width = 0,
      .height = 0,
      .is_set = false
  };

  Handle handle = cast_handle(Handle, ha_add(GPUTexture, &g_renderer->gpu_textures, new_texture));
  return handle;
}

b32 renderer_set_texture(Handle tex_handle, Image *image) {
  if (!g_renderer || !handle_is_valid(tex_handle) || !image) {
    return false;
  }

  GPUTexture *gpu_tex = ha_get(GPUTexture, &g_renderer->gpu_textures, tex_handle);
  if (!gpu_tex) {
    return false;
  }

  // Create GPU texture from image data
  if (gpu_tex->texture && gpu_tex->is_set) {
    // Destroy old texture if it exists
    gpu_destroy_texture(gpu_tex->texture);
  }

  gpu_tex->texture = gpu_create_texture(g_renderer->device, image->width, image->height);
  if (!gpu_tex->texture) {
    return false;
  }

  // Upload texture data would go here
  // For now, just mark as set
  gpu_tex->width = image->width;
  gpu_tex->height = image->height;
  gpu_tex->is_set = true;

  return true;
}

void renderer_draw_skybox(Handle material_handle) {
  if (!g_renderer || !handle_is_valid(material_handle)) {
    return;
  }

  RenderCommand cmd = {
      .type = RENDER_CMD_DRAW_SKYBOX,
      .data.draw_skybox = {
          .material_handle = material_handle
      }
  };

  add_render_command(cmd);
}

void renderer_handle_resize(i32 width, i32 height) {
  // Stub implementation - video renderer doesn't need dynamic resize
  // as it renders to fixed-size textures
  (void)width;
  (void)height;
}