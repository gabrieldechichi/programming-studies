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

typedef struct {
  GpuMesh_Handle mesh;
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

typedef struct {
  GpuShaderDesc shader_desc;

  // pipeline stuff
  // todo: option for default layouts
  GpuVertexLayout vertex_layout;
  GpuPrimitiveTopology primitive;
  // todo: use flags here
  b32 depth_test;
  b32 depth_write;

  // todo: property names?
} MaterialDesc;

typedef struct {
  GpuShader shader;
  GpuPipeline pipeline;

  // todo: property values?
} Material;
TYPED_HANDLE_DEFINE(Material);
HANDLE_ARRAY_DEFINE(Material);

void renderer_init(ArenaAllocator *arena, u8 thread_count);

GpuMesh_Handle renderer_upload_mesh(MeshDesc *desc);
GpuMesh_Handle renderer_upload_mesh(MeshDesc *desc);

Material_Handle renderer_create_material(MaterialDesc *desc);

// Main thread only: called before parallel work begins
void renderer_begin_frame(mat4 view, mat4 proj, GpuColor clear_color);

// Thread safe, lock-free append to command queue
void renderer_draw_mesh(GpuMesh_Handle mesh, mat4 model_matrix);

// Main thread only: called after parallel work completes
void renderer_end_frame(void);

#endif
