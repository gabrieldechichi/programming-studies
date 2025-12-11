#ifndef H_RENDERER
#define H_RENDERER

#include "gpu.h"

// =============================================================================
// Mesh - GPU-side vertex/index buffers
// =============================================================================

typedef struct {
    GpuBuffer vbuf;
    GpuBuffer ibuf;
    u32 index_count;
    GpuIndexFormat index_format;
} Mesh;

TYPED_HANDLE_DEFINE(Mesh);   // -> Mesh_Handle
HANDLE_ARRAY_DEFINE(Mesh);   // -> HandleArray_Mesh

typedef struct {
    void *vertices;
    u32 vertex_size;          // Total bytes of vertex data
    void *indices;
    u32 index_size;           // Total bytes of index data
    u32 index_count;          // Number of indices (for draw call)
    GpuIndexFormat index_format;
} MeshDesc;

// =============================================================================
// Render Commands - for multithreaded rendering
// =============================================================================

typedef struct {
  Mesh_Handle mesh;
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

// =============================================================================
// Renderer API
// =============================================================================

// Renderer initialization (call after gpu_init, sets up shared resources)
void renderer_init(void *arena);

// Upload mesh to GPU, returns handle for drawing
Mesh_Handle renderer_upload_mesh(MeshDesc *desc);

// Called by main thread before parallel work begins
void renderer_begin_frame(mat4 view, mat4 proj, GpuColor clear_color);

// Called by ANY thread - lock-free append to command queue
void renderer_draw_mesh(Mesh_Handle mesh, mat4 model_matrix);

// Called by main thread after parallel work completes
void renderer_end_frame(void);

#endif
