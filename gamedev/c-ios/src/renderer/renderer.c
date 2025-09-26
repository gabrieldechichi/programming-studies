// Renderer system ported from renderer_sokol.c
// Provides high-level rendering abstractions on top of gpu_backend

#include "renderer.h"
#include "gpu_backend.h"
#include "lib/array.h"
#include "lib/assert.h"
#include "lib/handle.h"
#include "lib/profiler.h"
#include "lib/string.h"
#include "lib/string_builder.h"
#include "memory.h"
#include "shader_reflection.h"
#include "shaders/simple_quad_reflection.h"
#include "shaders/toon_shading_reflection.h"
#include "vendor/cglm/mat4.h"
#include "vendor/cglm/vec3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define internal static

// ===== Shader Registry =====

typedef struct ShaderRegistryEntry {
  const char *name;
  const ShaderReflection *reflection;
  gpu_pipeline_t *pipeline; // Lazily initialized
} ShaderRegistryEntry;

// Shader registry with reflection data
static ShaderRegistryEntry shader_registry[] = {
    // Toon shading shader with full reflection data
    {.name = "toon_shading",
     .reflection = &toon_shading_reflection,
     .pipeline = NULL},
    // Simple quad shader for testing
    {.name = "simple_quad",
     .reflection = &simple_quad_reflection,
     .pipeline = NULL},
};

static const int shader_registry_count =
    sizeof(shader_registry) / sizeof(shader_registry[0]);

// ===== Internal Type Definitions =====

// ===== Internal Structures =====

// GPU Shader
typedef struct {
  gpu_pipeline_t *pipeline;
  const char *name;
  const ShaderReflection *reflection;

  // Fast lookup tables built at shader load time
  i32 uniform_bindings[UNIFORM_SEMANTIC_COUNT];
  i32 texture_bindings[TEXTURE_SEMANTIC_COUNT];
} GPUShader;

TYPED_HANDLE_DEFINE(GPUShader);
HANDLE_ARRAY_DEFINE(GPUShader);

// GPU SubMesh
typedef struct {
  gpu_buffer_t *vertex_buffer;
  gpu_buffer_t *index_buffer;
  gpu_buffer_t *blendshape_buffer; // Storage buffer for blendshape deltas
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
  MaterialBindingCache
      *binding_cache; // Pre-computed bindings for fast rendering
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

  // Default resources
  gpu_buffer_t *default_blendshape_buffer; // Default buffer for meshes without
                                           // blendshapes

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

  // Batching system for skinned meshes
  MaterialBatch_Slice material_batches;

  b32 initialized;
} Renderer;

// Global renderer instance
static Renderer *g_renderer = NULL;

// ===== Internal Functions =====

internal void add_render_command(RenderCommand command) {
  if (!g_renderer)
    return;
  slice_append(g_renderer->render_cmds, command);
}

// ===== Public API =====

void renderer_init(gpu_device_t *device, Allocator *permanent_allocator,
                   Allocator *temp_allocator) {
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
  g_renderer->render_cmds =
      slice_new_ALLOC(permanent_allocator, RenderCommand, 4096);

  // Initialize material batches for skinned mesh batching
  g_renderer->material_batches =
      slice_new_ALLOC(permanent_allocator, MaterialBatch, 32);

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
  g_renderer->current_lights.count = 1.0f; // One light
  g_renderer->current_lights._padding[0] = 0.0f;
  g_renderer->current_lights._padding[1] = 0.0f;
  g_renderer->current_lights._padding[2] = 0.0f;

  // Light 0 - direction (normalized)
  vec3 light_dir = {0.5f, -1.0f, -0.5f};
  glm_vec3_normalize(light_dir);
  glm_vec3_copy(light_dir, g_renderer->current_lights.lights[0].direction);
  g_renderer->current_lights.lights[0]._padding1 = 0.0f;

  // Light 0 - color and intensity
  g_renderer->current_lights.lights[0].color[0] = 1.0f; // R
  g_renderer->current_lights.lights[0].color[1] = 1.0f; // G
  g_renderer->current_lights.lights[0].color[2] = 1.0f; // B
  g_renderer->current_lights.lights[0].intensity = 1.0f;

  // Create default blendshape buffer for meshes without blendshapes
  {
    // Create a minimal buffer with one dummy BlendshapeDelta (2 vec4s)
    struct {
      vec4 position;
      vec4 normal;
    } dummy_delta = {
        .position = {0.0f, 0.0f, 0.0f, 0.0f},
        .normal = {0.0f, 0.0f, 0.0f, 0.0f},
    };

    g_renderer->default_blendshape_buffer = gpu_create_storage_buffer(
        g_renderer->device, &dummy_delta, sizeof(dummy_delta));

    if (!g_renderer->default_blendshape_buffer) {
      printf(
          "[Renderer] WARNING: Failed to create default blendshape buffer\n");
    }
  }

  g_renderer->initialized = true;
  printf("[Renderer] Initialized\n");
}

void renderer_reset_commands(void) {
  if (!g_renderer)
    return;
  g_renderer->render_cmds.len = 0;

  // Clear material batches for next frame
  for (u32 i = 0; i < g_renderer->material_batches.len; i++) {
    g_renderer->material_batches.items[i].instances.len = 0;
  }
  g_renderer->material_batches.len = 0;

  // Reset descriptor pools for all shaders at frame start
  // Note: We need to iterate through handles, not raw items, to skip empty
  // slots
  for (u32 i = 0; i < g_renderer->gpu_shaders.handles.len; i++) {
    Handle handle = g_renderer->gpu_shaders.handles.items[i];
    if (handle_is_valid(handle)) {
      GPUShader *shader = ha_get(GPUShader, &g_renderer->gpu_shaders, handle);
      if (shader && shader->pipeline) {
        gpu_reset_pipeline_descriptor_pool(shader->pipeline);
      }
    }
  }

  // Reset temporary allocator
  ALLOC_RESET(g_renderer->temp_allocator);
}

void renderer_clear(Color color) {
  RenderCommand cmd = {.type = RENDER_CMD_CLEAR,
                       .data.clear = {.color = color}};
  add_render_command(cmd);
}

Handle renderer_create_submesh(SubMeshData *mesh_data, b32 is_skinned) {
  if (!g_renderer || !mesh_data || !mesh_data->vertex_buffer) {
    return INVALID_HANDLE;
  }

  // Create vertex buffer
  gpu_buffer_t *vertex_buffer =
      gpu_create_buffer(g_renderer->device, mesh_data->vertex_buffer,
                        mesh_data->len_vertex_buffer * sizeof(float));

  if (!vertex_buffer) {
    return INVALID_HANDLE;
  }

  // Create index buffer (optional - NULL if no indices)
  gpu_buffer_t *index_buffer = NULL;

  index_buffer = gpu_create_buffer(g_renderer->device, mesh_data->indices,
                                   mesh_data->len_indices * sizeof(u32));

  if (!index_buffer) {
    gpu_destroy_buffer(vertex_buffer);
    return INVALID_HANDLE;
  }

  // Create blendshape buffer if needed
  gpu_buffer_t *blendshape_buffer = NULL;
  if (mesh_data->len_blendshapes > 0 && mesh_data->blendshape_deltas) {
    // Pack deltas as BlendshapeDelta structs (2 vec4s each)
    // Layout: [vertex_id * blendshape_count + blendshape_id] for shader
    // indexing
    u32 num_deltas = mesh_data->len_vertices * mesh_data->len_blendshapes;
    struct {
      vec4 position;
      vec4 normal;
    } *packed_deltas = ALLOC_ARRAY(
        g_renderer->temp_allocator,
        struct {
          vec4 position;
          vec4 normal;
        },
        num_deltas);

    f32 *src = mesh_data->blendshape_deltas;
    // Reorganize data: for each vertex, store all its blendshapes contiguously
    for (u32 vertex_idx = 0; vertex_idx < mesh_data->len_vertices;
         vertex_idx++) {
      for (u32 bs_idx = 0; bs_idx < mesh_data->len_blendshapes; bs_idx++) {
        u32 dest_idx = vertex_idx * mesh_data->len_blendshapes + bs_idx;
        // Source data is organized as: blendshape_idx * vertices * 6 +
        // vertex_idx * 6
        u32 src_idx = bs_idx * mesh_data->len_vertices * 6 + vertex_idx * 6;

        // Copy position delta (3 floats) and pad to vec4
        packed_deltas[dest_idx].position[0] = src[src_idx + 0];
        packed_deltas[dest_idx].position[1] = src[src_idx + 1];
        packed_deltas[dest_idx].position[2] = src[src_idx + 2];
        packed_deltas[dest_idx].position[3] = 0.0f;

        // Copy normal delta (3 floats) and pad to vec4
        packed_deltas[dest_idx].normal[0] = src[src_idx + 3];
        packed_deltas[dest_idx].normal[1] = src[src_idx + 4];
        packed_deltas[dest_idx].normal[2] = src[src_idx + 5];
        packed_deltas[dest_idx].normal[3] = 0.0f;
      }
    }

    // Create GPU storage buffer for blendshape data
    blendshape_buffer = gpu_create_storage_buffer(
        g_renderer->device, packed_deltas, num_deltas * sizeof(*packed_deltas));

    if (!blendshape_buffer) {
      printf("[Renderer] WARNING: Failed to create blendshape buffer\n");
      // Don't fail the entire mesh creation, just proceed without blendshapes
    }
  }

  // Store submesh data
  GPUSubMesh new_submesh = {
      .vertex_buffer = vertex_buffer,
      .index_buffer = index_buffer,
      .blendshape_buffer = blendshape_buffer,
      .index_count =
          mesh_data->len_indices, // Either index count or vertex count
      .num_blendshapes = mesh_data->len_blendshapes,
      .is_skinned = is_skinned,
      .has_blendshapes =
          (mesh_data->len_blendshapes > 0 && blendshape_buffer != NULL)};

  // Add to handle array and return the handle
  Handle handle = cast_handle(
      Handle, ha_add(GPUSubMesh, &g_renderer->gpu_submeshes, new_submesh));
  return handle;
}

// Internal helper to register a shader with a pipeline - used by
// video_renderer.c
Handle renderer_load_shader(const char *shader_name, gpu_pipeline_t *pipeline) {
  if (!g_renderer || !shader_name || !pipeline) {
    return INVALID_HANDLE;
  }

  GPUShader new_shader = {.pipeline = pipeline, .name = shader_name};

  Handle handle = cast_handle(
      Handle, ha_add(GPUShader, &g_renderer->gpu_shaders, new_shader));
  return handle;
}

// Convert uniform data type to GPU vertex format
internal u32 uniform_type_to_vertex_format(UniformDataType type) {
  switch (type) {
  case UNIFORM_TYPE_VEC2:
    return 0; // float2
  case UNIFORM_TYPE_VEC3:
    return 1; // float3
  case UNIFORM_TYPE_VEC4:
    return 2; // float4
  case UNIFORM_TYPE_IVEC4:
    return 3; // ubyte4 (for joints)
  default:
    return 1; // Default to float3
  }
}

// Convert shader stage flags to GPU stage flags
internal u32 shader_stages_to_gpu_stages(ShaderStageFlags stages) {
  u32 gpu_stages = 0;
  if (stages & SHADER_STAGE_VERTEX)
    gpu_stages |= GPU_STAGE_VERTEX;
  if (stages & SHADER_STAGE_FRAGMENT)
    gpu_stages |= GPU_STAGE_FRAGMENT;
  if (stages & SHADER_STAGE_COMPUTE)
    gpu_stages |= GPU_STAGE_COMPUTE;
  return gpu_stages;
}

// Helper function to create pipeline from reflection data
internal gpu_pipeline_t *
create_shader_pipeline_from_reflection(const ShaderReflection *reflection) {
  if (!g_renderer || !g_renderer->device || !reflection) {
    return NULL;
  }

  // Build vertex layout from reflection
  gpu_vertex_attr_t *attributes =
      ALLOC_ARRAY(g_renderer->temp_allocator, gpu_vertex_attr_t,
                  reflection->vertex_attribute_count);

  for (u32 i = 0; i < reflection->vertex_attribute_count; i++) {
    const VertexAttributeDesc *attr = &reflection->vertex_attributes[i];
    attributes[i] = (gpu_vertex_attr_t){
        .index = attr->location,
        .offset = attr->offset,
        .format = uniform_type_to_vertex_format(attr->type)};
  }

  gpu_vertex_layout_t vertex_layout = {.attributes = attributes,
                                       .num_attributes =
                                           reflection->vertex_attribute_count,
                                       .stride = reflection->vertex_stride};

  // Count resources by type
  u32 uniform_buffer_count = 0;
  u32 storage_buffer_count = 0;
  u32 texture_count = 0;

  for (u32 i = 0; i < reflection->resource_count; i++) {
    switch (reflection->resources[i].type) {
    case SHADER_RESOURCE_UNIFORM_BUFFER:
      uniform_buffer_count++;
      break;
    case SHADER_RESOURCE_STORAGE_BUFFER:
      storage_buffer_count++;
      break;
    case SHADER_RESOURCE_TEXTURE:
      texture_count++;
      break;
    default:
      break;
    }
  }

  // Build uniform buffer descriptors
  gpu_uniform_buffer_desc_t *uniform_buffers = NULL;
  if (uniform_buffer_count > 0) {
    uniform_buffers =
        ALLOC_ARRAY(g_renderer->temp_allocator, gpu_uniform_buffer_desc_t,
                    uniform_buffer_count);
    u32 ub_idx = 0;
    for (u32 i = 0; i < reflection->resource_count; i++) {
      const ShaderResourceDesc *res = &reflection->resources[i];
      if (res->type == SHADER_RESOURCE_UNIFORM_BUFFER) {
        uniform_buffers[ub_idx++] = (gpu_uniform_buffer_desc_t){
            .binding = res->binding,
            .size = res->size,
            .stage_flags = shader_stages_to_gpu_stages(res->stages)};
      }
    }
  }

  // Build storage buffer descriptors
  gpu_storage_buffer_desc_t *storage_buffers = NULL;
  if (storage_buffer_count > 0) {
    storage_buffers =
        ALLOC_ARRAY(g_renderer->temp_allocator, gpu_storage_buffer_desc_t,
                    storage_buffer_count);
    u32 sb_idx = 0;
    for (u32 i = 0; i < reflection->resource_count; i++) {
      const ShaderResourceDesc *res = &reflection->resources[i];
      if (res->type == SHADER_RESOURCE_STORAGE_BUFFER) {
        storage_buffers[sb_idx++] = (gpu_storage_buffer_desc_t){
            .binding = res->binding,
            .size = res->size,
            .stage_flags = shader_stages_to_gpu_stages(res->stages)};
      }
    }
  }

  // Build texture descriptors
  gpu_texture_desc_t *textures = NULL;
  if (texture_count > 0) {
    textures = ALLOC_ARRAY(g_renderer->temp_allocator, gpu_texture_desc_t,
                           texture_count);
    u32 tex_idx = 0;
    for (u32 i = 0; i < reflection->resource_count; i++) {
      const ShaderResourceDesc *res = &reflection->resources[i];
      if (res->type == SHADER_RESOURCE_TEXTURE) {
        textures[tex_idx++] = (gpu_texture_desc_t){
            .binding = res->binding,
            .stage_flags = shader_stages_to_gpu_stages(res->stages)};
      }
    }
  }

  // Create pipeline descriptor
  gpu_pipeline_desc_t pipeline_desc = {
      .vertex_shader_path = reflection->vertex_shader_path,
      .fragment_shader_path = reflection->fragment_shader_path,
      .vertex_layout = &vertex_layout,
      .uniform_buffers = uniform_buffers,
      .num_uniform_buffers = uniform_buffer_count,
      .storage_buffers = storage_buffers,
      .num_storage_buffers = storage_buffer_count,
      .texture_bindings = textures,
      .num_texture_bindings = texture_count,
      .depth_test = reflection->depth_test,
      .depth_write = reflection->depth_write,
      .cull_mode = reflection->cull_mode};

  // Try without prefix first (when running from out/linux)
  gpu_pipeline_t *pipeline =
      gpu_create_pipeline_desc(g_renderer->device, &pipeline_desc);

  if (!pipeline) {
    // Try with out/linux prefix (when running from project root)
    char vertex_path[256];
    char fragment_path[256];
    snprintf(vertex_path, sizeof(vertex_path), "out/linux/%s",
             reflection->vertex_shader_path);
    snprintf(fragment_path, sizeof(fragment_path), "out/linux/%s",
             reflection->fragment_shader_path);

    pipeline_desc.vertex_shader_path = vertex_path;
    pipeline_desc.fragment_shader_path = fragment_path;
    pipeline = gpu_create_pipeline_desc(g_renderer->device, &pipeline_desc);
  }

  return pipeline;
}

// Build fast lookup tables for shader resources
internal void build_shader_lookup_tables(GPUShader *shader,
                                         const ShaderReflection *reflection) {
  // Initialize all lookups to -1 (not found)
  for (u32 i = 0; i < UNIFORM_SEMANTIC_COUNT; i++) {
    shader->uniform_bindings[i] = -1;
  }
  for (u32 i = 0; i < TEXTURE_SEMANTIC_COUNT; i++) {
    shader->texture_bindings[i] = -1;
  }

  // Build uniform semantic -> binding lookup
  for (u32 i = 0; i < reflection->resource_count; i++) {
    const ShaderResourceDesc *res = &reflection->resources[i];

    if (res->type == SHADER_RESOURCE_UNIFORM_BUFFER) {
      UniformSemantic semantic = get_uniform_semantic(reflection, res->name);
      if (semantic != UNIFORM_SEMANTIC_NONE) {
        shader->uniform_bindings[semantic] = res->binding;
      }
    } else if (res->type == SHADER_RESOURCE_TEXTURE) {
      TextureSemantic semantic = get_texture_semantic(reflection, res->name);
      if (semantic != TEXTURE_SEMANTIC_NONE) {
        shader->texture_bindings[semantic] = res->binding;
      }
    }
  }
}

// Public API function matching header
Handle load_shader(LoadShaderParams params) {
  if (!g_renderer || !params.shader_name) {
    return INVALID_HANDLE;
  }

  // Find shader in registry
  ShaderRegistryEntry *entry = NULL;
  for (int i = 0; i < shader_registry_count; i++) {
    if (strcmp(shader_registry[i].name, params.shader_name) == 0) {
      entry = &shader_registry[i];
      break;
    }
  }

  debug_assert(entry);
  if (!entry) {
    printf("[Renderer] Shader '%s' not found in registry\n",
           params.shader_name);
    return INVALID_HANDLE;
  }

  // Create pipeline if it doesn't exist (lazy initialization)
  if (!entry->pipeline) {
    entry->pipeline = create_shader_pipeline_from_reflection(entry->reflection);
    if (!entry->pipeline) {
      printf("[Renderer] Failed to create pipeline for shader '%s'\n",
             params.shader_name);
      return INVALID_HANDLE;
    }
    printf("[Renderer] Created pipeline for shader '%s'\n", params.shader_name);
  }

  // Create GPUShader with reflection data
  GPUShader new_shader = {.pipeline = entry->pipeline,
                          .name = entry->name,
                          .reflection = entry->reflection};

  // Build fast lookup tables
  build_shader_lookup_tables(&new_shader, entry->reflection);

  Handle handle = cast_handle(
      Handle, ha_add(GPUShader, &g_renderer->gpu_shaders, new_shader));
  return handle;
}

// Create material binding cache for fast rendering
internal MaterialBindingCache *
create_material_binding_cache(const ShaderReflection *reflection,
                              MaterialProperty *properties,
                              u32 property_count) {

  if (!reflection || property_count == 0) {
    return NULL;
  }

  MaterialBindingCache *cache =
      ALLOC(g_renderer->permanent_allocator, MaterialBindingCache);
  cache->binding_count = 0;
  cache->uniform_data_size = 0;

  // First pass: count bindings and calculate uniform data size
  u32 uniform_count = 0;
  u32 texture_count = 0;
  u32 total_uniform_size = 0;

  for (u32 i = 0; i < property_count; i++) {
    MaterialProperty *prop = &properties[i];

    switch (prop->type) {
    case MAT_PROP_VEC3: {
      // Find matching uniform resource
      const ShaderResourceDesc *res =
          find_resource_by_name(reflection, "material_params");
      if (res && res->type == SHADER_RESOURCE_UNIFORM_BUFFER) {
        uniform_count++;
        total_uniform_size += 16; // vec3 padded to 16 bytes
      }
    } break;

    case MAT_PROP_TEXTURE: {
      // Find matching texture resource by name
      const ShaderResourceDesc *res = NULL;
      if (strcmp(prop->name.value, "uTexture") == 0) {
        res = find_resource_by_name(reflection, "diffuse_texture");
      } else if (strcmp(prop->name.value, "uDetailTexture") == 0) {
        res = find_resource_by_name(reflection, "detail_texture");
      }
      if (res && res->type == SHADER_RESOURCE_TEXTURE) {
        texture_count++;
      }
    } break;

    default:
      break;
    }
  }

  // Allocate bindings array
  cache->binding_count = uniform_count + texture_count;
  if (cache->binding_count == 0) {
    // Don't free - permanent allocator is never freed
    return NULL;
  }

  cache->bindings = ALLOC_ARRAY(g_renderer->permanent_allocator,
                                MaterialBinding, cache->binding_count);

  // Allocate uniform data block
  if (total_uniform_size > 0) {
    cache->uniform_data_block =
        ALLOC_ARRAY(g_renderer->permanent_allocator, u8, total_uniform_size);
    cache->uniform_data_size = total_uniform_size;
  }

  // Second pass: populate bindings and pack uniform data
  u32 binding_idx = 0;
  u32 uniform_offset = 0;

  for (u32 i = 0; i < property_count; i++) {
    MaterialProperty *prop = &properties[i];

    switch (prop->type) {
    case MAT_PROP_VEC3: {
      const ShaderResourceDesc *res =
          find_resource_by_name(reflection, "material_params");
      if (res && res->type == SHADER_RESOURCE_UNIFORM_BUFFER) {
        MaterialBinding *binding = &cache->bindings[binding_idx++];
        binding->binding_index = res->binding;
        binding->type = SHADER_RESOURCE_UNIFORM_BUFFER;
        binding->resource.uniform.data =
            (u8 *)cache->uniform_data_block + uniform_offset;
        binding->resource.uniform.size = 16;

        // Pack vec3 into uniform data (padded to 16 bytes)
        f32 *dest = (f32 *)binding->resource.uniform.data;
        glm_vec3_copy(prop->value.vec3_val, dest);
        dest[3] = 0.0f; // Padding

        uniform_offset += 16;
      }
    } break;

    case MAT_PROP_TEXTURE: {
      const ShaderResourceDesc *res = NULL;
      if (strcmp(prop->name.value, "uTexture") == 0) {
        res = find_resource_by_name(reflection, "diffuse_texture");
      } else if (strcmp(prop->name.value, "uDetailTexture") == 0) {
        res = find_resource_by_name(reflection, "detail_texture");
      }
      if (res && res->type == SHADER_RESOURCE_TEXTURE) {
        MaterialBinding *binding = &cache->bindings[binding_idx++];
        binding->binding_index = res->binding;
        binding->type = SHADER_RESOURCE_TEXTURE;
        binding->resource.texture.texture_handle_offset = i;
      }
    } break;

    default:
      break;
    }
  }

  return cache;
}

Handle load_material(Handle shader_handle, MaterialProperty *properties,
                     u32 property_count, b32 transparent) {
  if (!g_renderer || !handle_is_valid(shader_handle)) {
    return INVALID_HANDLE;
  }

  // Get shader to access reflection data
  GPUShader *shader =
      ha_get(GPUShader, &g_renderer->gpu_shaders, shader_handle);
  if (!shader) {
    return INVALID_HANDLE;
  }

  // Copy properties
  MaterialProperty *prop_copy = NULL;
  if (property_count > 0 && properties) {
    prop_copy = ALLOC_ARRAY(g_renderer->permanent_allocator, MaterialProperty,
                            property_count);
    for (u32 i = 0; i < property_count; i++) {
      prop_copy[i] = properties[i];
    }
  }

  // Create binding cache for fast rendering
  MaterialBindingCache *binding_cache = create_material_binding_cache(
      shader->reflection, prop_copy, property_count);

  GPUMaterial new_material = {.shader_handle = shader_handle,
                              .properties = prop_copy,
                              .property_count = property_count,
                              .transparent = transparent,
                              .binding_cache = binding_cache};

  Handle handle = cast_handle(
      Handle, ha_add(GPUMaterial, &g_renderer->gpu_materials, new_material));
  return handle;
}

void renderer_draw_mesh(Handle mesh_handle, Handle material_handle,
                        mat4 model_matrix) {
  if (!g_renderer || !handle_is_valid(mesh_handle) ||
      !handle_is_valid(material_handle)) {
    return;
  }

  RenderCommand cmd = {.type = RENDER_CMD_DRAW_MESH,
                       .data.draw_mesh = {
                           .mesh_handle = mesh_handle,
                           .material_handle = material_handle,
                       }};

  glm_mat4_copy(model_matrix, cmd.data.draw_mesh.model_matrix);
  add_render_command(cmd);
}

void renderer_draw_skinned_mesh(Handle mesh_handle, Handle material_handle,
                                mat4 model_matrix, mat4 *joint_transforms,
                                u32 num_joints,
                                BlendshapeParams *blendshape_params) {
  if (!g_renderer || !handle_is_valid(mesh_handle) ||
      !handle_is_valid(material_handle)) {
    return;
  }

  if (!joint_transforms || num_joints == 0) {
    return;
  }

  RenderCommand cmd = {.type = RENDER_CMD_DRAW_SKINNED_MESH,
                       .data.draw_skinned_mesh = {
                           .mesh_handle = mesh_handle,
                           .material_handle = material_handle,
                           .joint_transforms = joint_transforms,
                           .num_joints = num_joints,
                           .blendshape_params = blendshape_params,
                       }};

  glm_mat4_copy(model_matrix, cmd.data.draw_skinned_mesh.model_matrix);
  add_render_command(cmd);
}

void renderer_update_camera(const CameraUniformBlock *camera_uniforms) {
  if (!g_renderer || !camera_uniforms)
    return;
  g_renderer->current_camera = *camera_uniforms;
}

void renderer_set_lights(const DirectionalLightBlock *lights) {
  if (!g_renderer || !lights)
    return;
  g_renderer->current_lights = *lights;
}

// Helper to collect skinned mesh instances for batching
internal void
collect_skinned_mesh_instance(Handle material_handle, Handle mesh_handle,
                              mat4 model_matrix, mat4 *joint_transforms,
                              u32 num_joints,
                              BlendshapeParams *blendshape_params) {
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
    MaterialBatch new_batch = {.material_handle = material_handle,
                               .instances =
                                   slice_new_ALLOC(g_renderer->temp_allocator,
                                                   SkinnedMeshInstance, 2048)};
    slice_append(g_renderer->material_batches, new_batch);
    batch = &g_renderer->material_batches
                 .items[g_renderer->material_batches.len - 1];
  }

  // Add instance to batch
  SkinnedMeshInstance instance = {.mesh_handle = mesh_handle,
                                  .joint_transforms = joint_transforms,
                                  .num_joints = num_joints,
                                  .blendshape_params = blendshape_params};
  glm_mat4_copy(model_matrix, instance.model_matrix);
  slice_append(batch->instances, instance);
}

// Execute accumulated render commands
void renderer_execute_commands(gpu_texture_t *render_target,
                               gpu_command_buffer_t *cmd_buffer) {
  if (!g_renderer || !render_target || !cmd_buffer)
    return;

  // Process commands and batch skinned meshes
  PROFILE_BEGIN("Process render commands");

  // Pre-pass: Update all textures in pipeline descriptor sets before rendering
  arr_foreach_ptr(g_renderer->render_cmds, cmd) {
    // Handle textures for both regular and skinned meshes
    Handle material_handle = INVALID_HANDLE;
    if (cmd->type == RENDER_CMD_DRAW_MESH) {
      material_handle = cmd->data.draw_mesh.material_handle;
    } else if (cmd->type == RENDER_CMD_DRAW_SKINNED_MESH) {
      material_handle = cmd->data.draw_skinned_mesh.material_handle;
    }

    if (handle_is_valid(material_handle)) {
      // Get material for this mesh
      GPUMaterial *material =
          ha_get(GPUMaterial, &g_renderer->gpu_materials, material_handle);
      if (!material)
        continue;

      // Get shader for this material
      GPUShader *gpu_shader =
          ha_get(GPUShader, &g_renderer->gpu_shaders, material->shader_handle);
      if (!gpu_shader || !gpu_shader->pipeline)
        continue;

      debug_assert(material->binding_cache);
      if (!material->binding_cache) {
        continue;
      }

      MaterialBindingCache *cache = material->binding_cache;
      for (u32 i = 0; i < cache->binding_count; i++) {
        MaterialBinding *binding = &cache->bindings[i];
        if (binding->type == SHADER_RESOURCE_TEXTURE) {
          // Get texture from material properties
          u32 prop_idx = binding->resource.texture.texture_handle_offset;
          if (prop_idx < material->property_count &&
              material->properties[prop_idx].type == MAT_PROP_TEXTURE) {
            Texture_Handle tex_handle =
                material->properties[prop_idx].value.texture;
            Handle generic_handle = cast_handle(Handle, tex_handle);
            GPUTexture *gpu_tex =
                ha_get(GPUTexture, &g_renderer->gpu_textures, generic_handle);

            if (gpu_tex && gpu_tex->is_set && gpu_tex->texture) {
              gpu_update_pipeline_texture(gpu_shader->pipeline,
                                          gpu_tex->texture,
                                          binding->binding_index);
            }
          }
        }
      }
    }
  }

  // First pass: handle immediate commands (clear, simple meshes)
  Color clear_color = {0};

  // Collect clear color first
  arr_foreach_ptr(g_renderer->render_cmds, cmd) {
    if (cmd->type == RENDER_CMD_CLEAR) {
      clear_color = cmd->data.clear.color;
      break; // Use first clear command
    }
  }

  // Begin ONE render pass for ALL draw commands
  gpu_render_encoder_t *encoder = NULL;
  b32 render_pass_begun = false;

  arr_foreach_ptr(g_renderer->render_cmds, cmd) {
    switch (cmd->type) {
    case RENDER_CMD_CLEAR: {
      // Already handled above
    } break;

    case RENDER_CMD_DRAW_MESH: {
      // Get mesh and material
      GPUSubMesh *mesh = ha_get(GPUSubMesh, &g_renderer->gpu_submeshes,
                                cmd->data.draw_mesh.mesh_handle);
      GPUMaterial *material = ha_get(GPUMaterial, &g_renderer->gpu_materials,
                                     cmd->data.draw_mesh.material_handle);

      if (!mesh || !material)
        continue;

      // Get shader
      GPUShader *gpu_shader =
          ha_get(GPUShader, &g_renderer->gpu_shaders, material->shader_handle);
      if (!gpu_shader || !gpu_shader->pipeline) {
        continue;
      }

      // Begin render pass only once
      if (!render_pass_begun) {
        encoder = gpu_begin_render_pass(cmd_buffer, render_target);
        render_pass_begun = true;
      }

      // Set pipeline for this draw
      gpu_set_pipeline(encoder, gpu_shader->pipeline, clear_color.components);

      // Set vertex buffer
      gpu_set_vertex_buffer(encoder, mesh->vertex_buffer, 0);

      // Set index buffer (required for indexed drawing)
      gpu_set_index_buffer(encoder, mesh->index_buffer);

      // Allocate a new descriptor set for this draw
      gpu_descriptor_set_t *desc_set =
          gpu_allocate_descriptor_set(gpu_shader->pipeline);
      if (!desc_set) {
        printf("[Renderer] WARNING: Failed to allocate descriptor set for "
               "regular mesh\n");
        continue;
      }

      // Update uniforms using fast lookups from reflection system
      if (gpu_shader->pipeline->has_uniforms && gpu_shader->reflection) {
        // Update camera uniform
        i32 camera_binding =
            gpu_shader->uniform_bindings[UNIFORM_SEMANTIC_CAMERA];
        if (camera_binding >= 0) {
          gpu_update_descriptor_uniforms(desc_set, camera_binding,
                                         &g_renderer->current_camera,
                                         sizeof(CameraUniformBlock));
        }

        // Update model matrix
        i32 model_binding =
            gpu_shader->uniform_bindings[UNIFORM_SEMANTIC_MODEL];
        if (model_binding >= 0) {
          gpu_update_descriptor_uniforms(desc_set, model_binding,
                                         &cmd->data.draw_mesh.model_matrix,
                                         sizeof(mat4));
        }

        // Apply material uniforms from binding cache
        if (material->binding_cache) {
          MaterialBindingCache *cache = material->binding_cache;
          for (u32 i = 0; i < cache->binding_count; i++) {
            MaterialBinding *binding = &cache->bindings[i];
            if (binding->type == SHADER_RESOURCE_UNIFORM_BUFFER) {
              gpu_update_descriptor_uniforms(desc_set, binding->binding_index,
                                             binding->resource.uniform.data,
                                             binding->resource.uniform.size);
            } else if (binding->type == SHADER_RESOURCE_TEXTURE) {
              // Get texture from material properties
              u32 prop_idx = binding->resource.texture.texture_handle_offset;
              if (prop_idx < material->property_count &&
                  material->properties[prop_idx].type == MAT_PROP_TEXTURE) {
                Texture_Handle tex_handle =
                    material->properties[prop_idx].value.texture;
                Handle generic_handle = cast_handle(Handle, tex_handle);
                GPUTexture *gpu_tex = ha_get(
                    GPUTexture, &g_renderer->gpu_textures, generic_handle);

                if (gpu_tex && gpu_tex->is_set && gpu_tex->texture) {
                  gpu_update_descriptor_texture(desc_set, gpu_tex->texture,
                                                binding->binding_index);
                }
              }
            }
          }
        }

        // Update lights (if shader uses them)
        i32 lights_binding =
            gpu_shader->uniform_bindings[UNIFORM_SEMANTIC_LIGHTS];
        if (lights_binding >= 0) {
          gpu_update_descriptor_uniforms(desc_set, lights_binding,
                                         &g_renderer->current_lights,
                                         sizeof(DirectionalLightBlock));
        }
      }

      // Bind the descriptor set
      gpu_bind_descriptor_set(encoder, gpu_shader->pipeline, desc_set);

      // Draw the mesh
      gpu_draw(encoder, mesh->index_count);
    } break;

    case RENDER_CMD_DRAW_SKINNED_MESH: {
      // Collect skinned mesh for batching instead of rendering immediately
      collect_skinned_mesh_instance(
          cmd->data.draw_skinned_mesh.material_handle,
          cmd->data.draw_skinned_mesh.mesh_handle,
          cmd->data.draw_skinned_mesh.model_matrix,
          cmd->data.draw_skinned_mesh.joint_transforms,
          cmd->data.draw_skinned_mesh.num_joints,
          cmd->data.draw_skinned_mesh.blendshape_params);
    } break;

    case RENDER_CMD_DRAW_SKYBOX:
      // Skybox rendering not implemented yet
      break;

    default:
      break;
    }
  }

  // Now render all collected skinned meshes in batches
  if (g_renderer->material_batches.len > 0) {
    PROFILE_BEGIN("Render skinned mesh batches");

    for (u32 batch_idx = 0; batch_idx < g_renderer->material_batches.len;
         batch_idx++) {
      MaterialBatch *batch = &g_renderer->material_batches.items[batch_idx];
      if (batch->instances.len == 0)
        continue;

      PROFILE_BEGIN("skinned batch: single batch");

      // Get material from handle
      GPUMaterial *material = ha_get(GPUMaterial, &g_renderer->gpu_materials,
                                     batch->material_handle);
      if (!material)
        continue;

      // Get shader
      GPUShader *gpu_shader =
          ha_get(GPUShader, &g_renderer->gpu_shaders, material->shader_handle);
      if (!gpu_shader || !gpu_shader->pipeline)
        continue;

      debug_assert(material->binding_cache);
      if (!material->binding_cache)
        continue;

      // Begin render pass only once
      if (!render_pass_begun) {
        encoder = gpu_begin_render_pass(cmd_buffer, render_target);
        render_pass_begun = true;
      }

      PROFILE_BEGIN("skinned batch: set pipeline");
      // Apply pipeline once per material
      gpu_set_pipeline(encoder, gpu_shader->pipeline, clear_color.components);
      PROFILE_END();

      // Now render all instances with this material
      Handle current_mesh = INVALID_HANDLE;
      GPUSubMesh *current_submesh = NULL;

      for (u32 inst_idx = 0; inst_idx < batch->instances.len; inst_idx++) {
        PROFILE_BEGIN("skinned batch: single instance");
        SkinnedMeshInstance *instance = &batch->instances.items[inst_idx];

        // Only update vertex/index buffers if mesh changed
        if (!handle_equals(instance->mesh_handle, current_mesh)) {
          current_mesh = instance->mesh_handle;
          current_submesh =
              ha_get(GPUSubMesh, &g_renderer->gpu_submeshes, current_mesh);

          PROFILE_BEGIN("skinned batch: update mesh buffer");
          // Update buffers with new mesh
          gpu_set_vertex_buffer(encoder, current_submesh->vertex_buffer, 0);
          gpu_set_index_buffer(encoder, current_submesh->index_buffer);
          PROFILE_END();
        }

        PROFILE_BEGIN("skinned batch: allocate descriptor set");
        // Allocate a new descriptor set for this draw
        gpu_descriptor_set_t *desc_set =
            gpu_allocate_descriptor_set(gpu_shader->pipeline);

        PROFILE_END();
        // Apply per-instance uniforms to the new descriptor set
        if (gpu_shader->pipeline->has_uniforms && gpu_shader->reflection) {

          PROFILE_BEGIN("skinned batch: update uniforms");
          // Camera uniform
          i32 camera_binding =
              gpu_shader->uniform_bindings[UNIFORM_SEMANTIC_CAMERA];
          if (camera_binding >= 0) {
            gpu_update_descriptor_uniforms(desc_set, camera_binding,
                                           &g_renderer->current_camera,
                                           sizeof(CameraUniformBlock));
          }

          // Lights uniform
          i32 lights_binding =
              gpu_shader->uniform_bindings[UNIFORM_SEMANTIC_LIGHTS];
          if (lights_binding >= 0) {
            gpu_update_descriptor_uniforms(desc_set, lights_binding,
                                           &g_renderer->current_lights,
                                           sizeof(DirectionalLightBlock));
          }

          // Material uniforms
          MaterialBindingCache *cache = material->binding_cache;
          for (u32 i = 0; i < cache->binding_count; i++) {
            MaterialBinding *binding = &cache->bindings[i];
            if (binding->type == SHADER_RESOURCE_UNIFORM_BUFFER) {
              gpu_update_descriptor_uniforms(desc_set, binding->binding_index,
                                             binding->resource.uniform.data,
                                             binding->resource.uniform.size);
            } else if (binding->type == SHADER_RESOURCE_TEXTURE) {
              // Get texture from material properties
              u32 prop_idx = binding->resource.texture.texture_handle_offset;
              if (prop_idx < material->property_count &&
                  material->properties[prop_idx].type == MAT_PROP_TEXTURE) {
                Texture_Handle tex_handle =
                    material->properties[prop_idx].value.texture;
                Handle generic_handle = cast_handle(Handle, tex_handle);
                GPUTexture *gpu_tex = ha_get(
                    GPUTexture, &g_renderer->gpu_textures, generic_handle);

                if (gpu_tex && gpu_tex->is_set && gpu_tex->texture) {
                  gpu_update_descriptor_texture(desc_set, gpu_tex->texture,
                                                binding->binding_index);
                }
              }
            }
          }

          // Joint transforms
          i32 joints_binding =
              gpu_shader->uniform_bindings[UNIFORM_SEMANTIC_JOINTS];
          if (joints_binding >= 0 && instance->joint_transforms) {
            gpu_update_descriptor_uniforms(
                desc_set, joints_binding, instance->joint_transforms,
                sizeof(float) * 16 * instance->num_joints);
          }

          // Model matrix
          i32 model_binding =
              gpu_shader->uniform_bindings[UNIFORM_SEMANTIC_MODEL];
          if (model_binding >= 0) {
            gpu_update_descriptor_uniforms(
                desc_set, model_binding, &instance->model_matrix, sizeof(mat4));
          }

          // Blendshape params
          i32 blendshape_binding =
              gpu_shader->uniform_bindings[UNIFORM_SEMANTIC_BLENDSHAPES];
          if (blendshape_binding >= 0 && instance->blendshape_params) {
            gpu_update_descriptor_uniforms(desc_set, blendshape_binding,
                                           instance->blendshape_params,
                                           sizeof(BlendshapeParams));
          }

          // Bind blendshape storage buffer at binding 7 (matches shader)
          if (current_submesh->has_blendshapes &&
              current_submesh->blendshape_buffer) {
            gpu_update_descriptor_storage_buffer(
                desc_set, current_submesh->blendshape_buffer, 7);
          } else {
            // Use default blendshape buffer to avoid shader issues
            gpu_update_descriptor_storage_buffer(
                desc_set, g_renderer->default_blendshape_buffer, 7);
          }

          PROFILE_END();
        }
        PROFILE_BEGIN("skinned batch: bind descriptor set");
        // Bind the per-instance descriptor set
        gpu_bind_descriptor_set(encoder, gpu_shader->pipeline, desc_set);
        PROFILE_END();

        PROFILE_BEGIN("skinned batch: draw");
        // Draw
        gpu_draw(encoder, current_submesh->index_count);
        PROFILE_END();

        PROFILE_END();
      }

      PROFILE_END();
    }

    PROFILE_END();
  }

  // End render pass after ALL draws are complete
  if (render_pass_begun && encoder) {
    gpu_end_render_pass(encoder);
  }

  PROFILE_END();
}

Handle renderer_reserve_texture(void) {
  if (!g_renderer) {
    return INVALID_HANDLE;
  }

  // Reserve a GPU texture slot
  GPUTexture new_texture = {
      .texture = NULL, .width = 0, .height = 0, .is_set = false};

  Handle handle = cast_handle(
      Handle, ha_add(GPUTexture, &g_renderer->gpu_textures, new_texture));
  return handle;
}

b32 renderer_set_texture(Handle tex_handle, Image *image) {
  if (!g_renderer || !handle_is_valid(tex_handle) || !image || !image->data) {
    return false;
  }

  GPUTexture *gpu_tex =
      ha_get(GPUTexture, &g_renderer->gpu_textures, tex_handle);
  if (!gpu_tex) {
    return false;
  }

  // Destroy old texture if it exists
  if (gpu_tex->texture && gpu_tex->is_set) {
    gpu_destroy_texture(gpu_tex->texture);
    gpu_tex->texture = NULL;
    gpu_tex->is_set = false;
  }

  // Create GPU texture with data
  printf("[Renderer] Creating GPU texture: %dx%d, %u bytes\n", image->width,
         image->height, image->byte_len);
  gpu_tex->texture =
      gpu_create_texture_with_data(g_renderer->device, image->width,
                                   image->height, image->data, image->byte_len);

  if (!gpu_tex->texture) {
    return false;
  }

  // Store texture info
  gpu_tex->width = image->width;
  gpu_tex->height = image->height;
  gpu_tex->is_set = true;

  return true;
}

void renderer_draw_skybox(Handle material_handle) {
  if (!g_renderer || !handle_is_valid(material_handle)) {
    return;
  }

  // RenderCommand cmd = {
  //     .type = RENDER_CMD_DRAW_SKYBOX,
  //     .data.draw_skybox = {.material_handle = material_handle}};
  //
  // add_render_command(cmd);
}