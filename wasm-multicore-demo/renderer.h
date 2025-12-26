#ifndef H_RENDERER
#define H_RENDERER

#include "gpu.h"
#include "lib/handle.h"
#include "lib/memory.h"

// Standard uniforms at binding 0 for all shaders
typedef struct {
  mat4 model;
  mat4 view;
  mat4 proj;
  mat4 view_proj;
} GlobalUniforms;

#define GPU_UNIFORM_DESC_VERTEX(_type, _binding)                               \
  {.stage = GPU_STAGE_VERTEX, .size = sizeof(_type), .binding = (_binding)}
#define GPU_UNIFORM_DESC_FRAG(_type, _binding)                                 \
  {.stage = GPU_STAGE_FRAGMENT, .size = sizeof(_type), .binding = (_binding)}
#define GPU_TEXTURE_BINDING_FRAG(_tex_binding, _sampler_binding)               \
  {.stage = GPU_STAGE_FRAGMENT,                                                \
   .sampler_binding = (_sampler_binding),                                      \
   .texture_binding = (_tex_binding)}

#define GLOBAL_UNIFORMS_DESC                                                   \
  {.stage = GPU_STAGE_VERTEX, .size = sizeof(GlobalUniforms), .binding = 0}

typedef struct {
  void *vertices;
  u32 vertex_size; // Total bytes of vertex data
  void *indices;
  u32 index_size;  // Total bytes of index data
  u32 index_count; // Number of indices (for draw call)
  GpuIndexFormat index_format;
} MeshDesc;

#define MAX_MATERIAL_PROPERTIES 8

typedef enum {
  MAT_PROP_FLOAT,
  MAT_PROP_VEC2,
  MAT_PROP_VEC3,
  MAT_PROP_VEC4,
  MAT_PROP_MAT4,
  MAT_PROP_TEXTURE,
} MaterialPropertyType;

typedef struct {
  const char *name;
  MaterialPropertyType type;
  u8 binding;
} MaterialPropertyDesc;

arr_define(MaterialPropertyDesc);

typedef struct {
  GpuShaderDesc shader_desc;

  // pipeline stuff
  // todo: option for default layouts
  GpuVertexLayout vertex_layout;
  GpuPrimitiveTopology primitive;
  // todo: use flags here
  b32 depth_test;
  b32 depth_write;

  StaticArray(MaterialPropertyDesc, MAX_MATERIAL_PROPERTIES) properties;
} MaterialDesc;

typedef struct {
  u32 name_hash;
  u8 binding;
  MaterialPropertyType type;
  union {
    f32 f;
    vec2 v2;
    vec3 v3;
    vec4 v4;
    mat4 m4;
    GpuTexture tex;
  };
} MaterialProperty;

typedef struct {
  GpuShader shader;
  GpuPipeline pipeline;

  StaticArray(MaterialProperty, MAX_MATERIAL_PROPERTIES) properties;
} Material;
TYPED_HANDLE_DEFINE(Material);
HANDLE_ARRAY_DEFINE(Material);

// Instance buffer for instanced rendering via storage buffers
typedef struct {
  u32 stride;        // Bytes per instance
  u32 max_instances; // Capacity
} InstanceBufferDesc;

typedef struct {
  GpuBuffer buffer;
  u32 stride;
  u32 max_instances;
  u32 instance_count; // Current count, set by update
} InstanceBuffer;
TYPED_HANDLE_DEFINE(InstanceBuffer);
HANDLE_ARRAY_DEFINE(InstanceBuffer);

typedef struct {
  GpuMesh_Handle mesh;
  Material_Handle material;
  mat4 model_matrix;
} RenderDrawMeshCmd;

typedef struct {
  GpuMesh_Handle mesh;
  Material_Handle material;
  InstanceBuffer_Handle instances;
} RenderDrawMeshInstancedCmd;

typedef enum {
  RENDER_CMD_DRAW_MESH,
  RENDER_CMD_DRAW_MESH_INSTANCED,
} RenderCmdType;

typedef struct {
  RenderCmdType type;
  union {
    RenderDrawMeshCmd draw_mesh;
    RenderDrawMeshInstancedCmd draw_mesh_instanced;
  };
} RenderCmd;

arr_define(RenderCmd);

void renderer_init(ArenaAllocator *arena, u8 thread_count);

GpuMesh_Handle renderer_upload_mesh(MeshDesc *desc);

Material_Handle renderer_create_material(MaterialDesc *desc);

// Instance buffer API (main thread only for create/update)
InstanceBuffer_Handle renderer_create_instance_buffer(InstanceBufferDesc *desc);
void renderer_update_instance_buffer(InstanceBuffer_Handle handle, void *data,
                                     u32 instance_count);

// Material property setters
void material_set_float(Material_Handle mat, const char *name, f32 value);
void material_set_vec4(Material_Handle mat, const char *name, vec4 value);
void material_set_texture(Material_Handle mat, const char *name,
                          GpuTexture tex);

// Main thread only: called before parallel work begins
void renderer_begin_frame(mat4 view, mat4 proj, GpuColor clear_color);

// Thread safe, lock-free append to command queue
void renderer_draw_mesh(GpuMesh_Handle mesh, Material_Handle material,
                        mat4 model_matrix);
void renderer_draw_mesh_instanced(GpuMesh_Handle mesh, Material_Handle material,
                                  InstanceBuffer_Handle instances);

// Main thread only: called after parallel work completes
void renderer_end_frame(void);

#endif
