#ifndef H_RENDERER
#define H_RENDERER

#include "gpu.h"
#include "lib/handle.h"
#include "lib/memory.h"

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
} MaterialPropertyType;

typedef struct {
    const char *name;
    MaterialPropertyType type;
    u8 binding;
} MaterialPropertyDesc;

typedef struct {
  GpuShaderDesc shader_desc;

  // pipeline stuff
  // todo: option for default layouts
  GpuVertexLayout vertex_layout;
  GpuPrimitiveTopology primitive;
  // todo: use flags here
  b32 depth_test;
  b32 depth_write;

  // material properties
  MaterialPropertyDesc properties[MAX_MATERIAL_PROPERTIES];
  u8 property_count;
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
    };
} MaterialProperty;

typedef struct {
  GpuShader shader;
  GpuPipeline pipeline;

  MaterialProperty properties[MAX_MATERIAL_PROPERTIES];
  u8 property_count;
} Material;
TYPED_HANDLE_DEFINE(Material);
HANDLE_ARRAY_DEFINE(Material);

typedef struct {
  GpuMesh_Handle mesh;
  Material_Handle material;
  mat4 model_matrix;
} RenderDrawMeshCmd;

typedef enum {
  RENDER_CMD_DRAW_MESH,
} RenderCmdType;

typedef struct {
  RenderCmdType type;
  union {
    RenderDrawMeshCmd draw_mesh;
  };
} RenderCmd;

arr_define(RenderCmd);

void renderer_init(ArenaAllocator *arena, u8 thread_count);

GpuMesh_Handle renderer_upload_mesh(MeshDesc *desc);

Material_Handle renderer_create_material(MaterialDesc *desc);

// Material property setters
void material_set_float(Material_Handle mat, const char *name, f32 value);
void material_set_vec4(Material_Handle mat, const char *name, vec4 value);

// Main thread only: called before parallel work begins
void renderer_begin_frame(mat4 view, mat4 proj, GpuColor clear_color);

// Thread safe, lock-free append to command queue
void renderer_draw_mesh(GpuMesh_Handle mesh, Material_Handle material, mat4 model_matrix);

// Main thread only: called after parallel work completes
void renderer_end_frame(void);

#endif
