// Renderer system ported from renderer_sokol.c
// Provides high-level rendering abstractions on top of gpu_backend

#include "renderer.h"
#include "lib/array.h"
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

// Render command types
typedef enum {
  RENDER_CMD_CLEAR,
  RENDER_CMD_DRAW_MESH,
  RENDER_CMD_DRAW_SKINNED_MESH,
  RENDER_CMD_MAX
} RenderCommandType;

// Render commands
typedef struct {
  RenderCommandType type;
  union {
    struct {
      Color color;
    } clear;
    struct {
      Handle mesh_handle;
      Handle material_handle;
      mat4 model_matrix;
    } draw_mesh;
    struct {
      Handle mesh_handle;
      Handle material_handle;
      mat4 model_matrix;
      mat4 *joint_transforms;
      u32 num_joints;
      BlendshapeParams *blendshape_params;
    } draw_skinned_mesh;
  } data;
} RenderCommand;

slice_define(RenderCommand);

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

  // Batching system
  MaterialBatch_Slice material_batches;

  b32 initialized;
} Renderer;

// Global renderer instance
static Renderer *g_renderer = NULL;

// ===== Internal Functions =====

internal void add_render_command(RenderCommand command) {
  if (!g_renderer) return;
  slice_append(g_renderer->render_cmds, command);
}

internal void collect_skinned_mesh_instance(
    Handle material_handle, Handle mesh_handle,
    mat4 model_matrix, mat4 *joint_transforms,
    u32 num_joints, BlendshapeParams *blendshape_params) {

  // Find or create material batch
  MaterialBatch *batch = NULL;
  for (u32 i = 0; i < g_renderer->material_batches.len; i++) {
    if (handle_equals(g_renderer->material_batches.items[i].material_handle,
                      material_handle)) {
      batch = &g_renderer->material_batches.items[i];
      break;
    }
  }

  if (!batch) {
    // Create new batch for this material
    MaterialBatch new_batch = {
        .material_handle = material_handle,
        .instances = slice_new_ALLOC(g_renderer->temp_allocator,
                                     SkinnedMeshInstance, 2048)
    };
    slice_append(g_renderer->material_batches, new_batch);
    batch = &g_renderer->material_batches.items[g_renderer->material_batches.len - 1];
  }

  // Add instance to batch
  SkinnedMeshInstance instance = {
      .mesh_handle = mesh_handle,
      .joint_transforms = joint_transforms,
      .num_joints = num_joints,
      .blendshape_params = blendshape_params
  };
  glm_mat4_copy(model_matrix, instance.model_matrix);
  slice_append(batch->instances, instance);
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

  // Initialize batching system
  g_renderer->material_batches = slice_new_ALLOC(permanent_allocator, MaterialBatch, 32);

  // Initialize handle arrays
  g_renderer->gpu_textures = ha_init(GPUTexture, permanent_allocator, 32);
  g_renderer->gpu_submeshes = ha_init(GPUSubMesh, permanent_allocator, 64);
  g_renderer->gpu_materials = ha_init(GPUMaterial, permanent_allocator, 32);
  g_renderer->gpu_shaders = ha_init(GPUShader, permanent_allocator, 16);

  // Initialize camera with identity matrices
  glm_mat4_identity(g_renderer->current_camera.view);
  glm_mat4_identity(g_renderer->current_camera.projection);
  glm_vec3_zero(g_renderer->current_camera.camera_pos);

  // Initialize default light
  glm_vec3_copy((vec3){0.5f, -1.0f, -0.5f}, g_renderer->current_lights.light_direction);
  glm_vec3_normalize(g_renderer->current_lights.light_direction);
  glm_vec3_copy((vec3){1.0f, 1.0f, 1.0f}, g_renderer->current_lights.light_color);
  g_renderer->current_lights.light_intensity = 1.0f;
  glm_vec3_copy((vec3){0.2f, 0.2f, 0.3f}, g_renderer->current_lights.ambient_color);
  g_renderer->current_lights.ambient_intensity = 1.0f;

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

  // Reset batches
  for (u32 i = 0; i < g_renderer->material_batches.len; i++) {
    g_renderer->material_batches.items[i].instances.len = 0;
  }
  g_renderer->material_batches.len = 0;

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
  if (!g_renderer || !mesh_data || !mesh_data->vertex_buffer || !mesh_data->indices) {
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

  // Create index buffer
  gpu_buffer_t *index_buffer = gpu_create_buffer(
      g_renderer->device,
      mesh_data->indices,
      mesh_data->len_indices * sizeof(u32)
  );

  if (!index_buffer) {
    gpu_destroy_buffer(vertex_buffer);
    return INVALID_HANDLE;
  }

  // Store submesh data
  GPUSubMesh new_submesh = {
      .vertex_buffer = vertex_buffer,
      .index_buffer = index_buffer,
      .index_count = mesh_data->len_indices,
      .num_blendshapes = mesh_data->len_blendshapes,
      .is_skinned = is_skinned,
      .has_blendshapes = mesh_data->len_blendshapes > 0
  };

  // Add to handle array and return the handle
  Handle handle = cast_handle(Handle, ha_add(GPUSubMesh, &g_renderer->gpu_submeshes, new_submesh));
  return handle;
}

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

Handle renderer_load_material(Handle shader_handle, MaterialProperty *properties,
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
  arr_foreach_ptr(g_renderer->render_cmds, cmd) {
    switch (cmd->type) {
      case RENDER_CMD_CLEAR: {
        // Clear operations handled at pass begin
      } break;

      case RENDER_CMD_DRAW_SKINNED_MESH: {
        // Collect for batching
        collect_skinned_mesh_instance(
            cmd->data.draw_skinned_mesh.material_handle,
            cmd->data.draw_skinned_mesh.mesh_handle,
            cmd->data.draw_skinned_mesh.model_matrix,
            cmd->data.draw_skinned_mesh.joint_transforms,
            cmd->data.draw_skinned_mesh.num_joints,
            cmd->data.draw_skinned_mesh.blendshape_params
        );
      } break;

      default:
        break;
    }
  }
  PROFILE_END();

  // Render batched meshes
  if (g_renderer->material_batches.len > 0) {
    PROFILE_BEGIN("Render batched meshes");

    for (u32 batch_idx = 0; batch_idx < g_renderer->material_batches.len; batch_idx++) {
      MaterialBatch *batch = &g_renderer->material_batches.items[batch_idx];
      if (batch->instances.len == 0) continue;

      // Get material
      GPUMaterial *material = ha_get(GPUMaterial, &g_renderer->gpu_materials, batch->material_handle);
      if (!material) continue;

      // Get shader
      GPUShader *gpu_shader = ha_get(GPUShader, &g_renderer->gpu_shaders, material->shader_handle);
      if (!gpu_shader) continue;

      // TODO: Apply pipeline and render instances
      // This will be implemented when we have the full GPU backend integration
    }

    PROFILE_END();
  }
}

// Helper function to create a cube mesh
SubMeshData* create_cube_mesh_data(Allocator *allocator) {
  // Vertex format: pos(3) + normal(3) + uv(2) + joints(4 bytes) + weights(4)
  // Total: 3 + 3 + 2 + 1 + 4 = 13 floats per vertex (joints packed as 1 float)
  const u32 num_vertices = 24; // 4 vertices per face * 6 faces
  const u32 floats_per_vertex = 13;
  const u32 total_floats = num_vertices * floats_per_vertex;

  float *vertices = ALLOC_ARRAY(allocator, float, total_floats);

  // Helper macro to add a vertex
  #define ADD_VERTEX(px, py, pz, nx, ny, nz, u, v) \
    vertices[idx++] = px; vertices[idx++] = py; vertices[idx++] = pz; \
    vertices[idx++] = nx; vertices[idx++] = ny; vertices[idx++] = nz; \
    vertices[idx++] = u; vertices[idx++] = v; \
    *((u32*)&vertices[idx++]) = 0x00000000; \
    vertices[idx++] = 1.0f; vertices[idx++] = 0.0f; \
    vertices[idx++] = 0.0f; vertices[idx++] = 0.0f;

  u32 idx = 0;

  // Front face (z = 0.5, normal = +z)
  ADD_VERTEX(-0.5f, -0.5f,  0.5f,  0.0f, 0.0f, 1.0f,  0.0f, 1.0f); // 0: bottom-left
  ADD_VERTEX( 0.5f, -0.5f,  0.5f,  0.0f, 0.0f, 1.0f,  1.0f, 1.0f); // 1: bottom-right
  ADD_VERTEX( 0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 1.0f,  1.0f, 0.0f); // 2: top-right
  ADD_VERTEX(-0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 1.0f,  0.0f, 0.0f); // 3: top-left

  // Back face (z = -0.5, normal = -z)
  ADD_VERTEX( 0.5f, -0.5f, -0.5f,  0.0f, 0.0f, -1.0f,  0.0f, 1.0f); // 4: bottom-right
  ADD_VERTEX(-0.5f, -0.5f, -0.5f,  0.0f, 0.0f, -1.0f,  1.0f, 1.0f); // 5: bottom-left
  ADD_VERTEX(-0.5f,  0.5f, -0.5f,  0.0f, 0.0f, -1.0f,  1.0f, 0.0f); // 6: top-left
  ADD_VERTEX( 0.5f,  0.5f, -0.5f,  0.0f, 0.0f, -1.0f,  0.0f, 0.0f); // 7: top-right

  // Left face (x = -0.5, normal = -x)
  ADD_VERTEX(-0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f,  0.0f, 1.0f); // 8: bottom-back
  ADD_VERTEX(-0.5f, -0.5f,  0.5f, -1.0f, 0.0f, 0.0f,  1.0f, 1.0f); // 9: bottom-front
  ADD_VERTEX(-0.5f,  0.5f,  0.5f, -1.0f, 0.0f, 0.0f,  1.0f, 0.0f); // 10: top-front
  ADD_VERTEX(-0.5f,  0.5f, -0.5f, -1.0f, 0.0f, 0.0f,  0.0f, 0.0f); // 11: top-back

  // Right face (x = 0.5, normal = +x)
  ADD_VERTEX( 0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f,  0.0f, 1.0f); // 12: bottom-front
  ADD_VERTEX( 0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f,  1.0f, 1.0f); // 13: bottom-back
  ADD_VERTEX( 0.5f,  0.5f, -0.5f,  1.0f, 0.0f, 0.0f,  1.0f, 0.0f); // 14: top-back
  ADD_VERTEX( 0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 0.0f,  0.0f, 0.0f); // 15: top-front

  // Top face (y = 0.5, normal = +y)
  ADD_VERTEX(-0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 0.0f,  0.0f, 1.0f); // 16: front-left
  ADD_VERTEX( 0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 0.0f,  1.0f, 1.0f); // 17: front-right
  ADD_VERTEX( 0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.0f,  1.0f, 0.0f); // 18: back-right
  ADD_VERTEX(-0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.0f,  0.0f, 0.0f); // 19: back-left

  // Bottom face (y = -0.5, normal = -y)
  ADD_VERTEX(-0.5f, -0.5f, -0.5f,  0.0f, -1.0f, 0.0f,  0.0f, 1.0f); // 20: back-left
  ADD_VERTEX( 0.5f, -0.5f, -0.5f,  0.0f, -1.0f, 0.0f,  1.0f, 1.0f); // 21: back-right
  ADD_VERTEX( 0.5f, -0.5f,  0.5f,  0.0f, -1.0f, 0.0f,  1.0f, 0.0f); // 22: front-right
  ADD_VERTEX(-0.5f, -0.5f,  0.5f,  0.0f, -1.0f, 0.0f,  0.0f, 0.0f); // 23: front-left

  #undef ADD_VERTEX

  // Create index buffer (36 indices for 12 triangles)
  u32 *indices = ALLOC_ARRAY(allocator, u32, 36);
  u32 indices_data[] = {
    // Front face
    0, 1, 2,  2, 3, 0,
    // Back face
    4, 5, 6,  6, 7, 4,
    // Left face
    8, 9, 10, 10, 11, 8,
    // Right face
    12, 13, 14, 14, 15, 12,
    // Top face
    16, 17, 18, 18, 19, 16,
    // Bottom face
    20, 21, 22, 22, 23, 20
  };
  memcpy(indices, indices_data, sizeof(indices_data));

  SubMeshData *mesh_data = ALLOC(allocator, SubMeshData);
  mesh_data->vertex_buffer = vertices;
  mesh_data->indices = indices;
  mesh_data->len_vertex_buffer = total_floats;
  mesh_data->len_indices = 36;
  mesh_data->len_vertices = num_vertices;
  mesh_data->len_blendshapes = 0;
  mesh_data->blendshape_deltas = NULL;

  return mesh_data;
}