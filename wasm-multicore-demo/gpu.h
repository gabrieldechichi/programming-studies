#ifndef H_GPU
#define H_GPU

#include "lib/typedefs.h"
#include "lib/handle.h"
#include "lib/array.h"
#include "lib/math.h"
#include "lib/memory.h"

// Uniform buffer constants
#define GPU_UNIFORM_BUFFER_SIZE MB(1)
#define GPU_UNIFORM_ALIGNMENT 256  // WebGPU minUniformBufferOffsetAlignment

// Resource handles
typedef Handle GpuBuffer;
typedef Handle GpuShader;
typedef Handle GpuPipeline;

#define GPU_INVALID_HANDLE INVALID_HANDLE

// Enums
typedef enum {
    GPU_BUFFER_VERTEX = 0,
    GPU_BUFFER_INDEX = 1,
    GPU_BUFFER_UNIFORM = 2,
} GpuBufferType;

typedef enum {
    GPU_VERTEX_FORMAT_FLOAT2 = 0,
    GPU_VERTEX_FORMAT_FLOAT3 = 1,
    GPU_VERTEX_FORMAT_FLOAT4 = 2,
} GpuVertexFormat;

typedef enum {
    GPU_INDEX_FORMAT_U16 = 0,
    GPU_INDEX_FORMAT_U32 = 1,
} GpuIndexFormat;

typedef enum {
    GPU_PRIMITIVE_TRIANGLES = 0,
    GPU_PRIMITIVE_LINES = 1,
} GpuPrimitiveTopology;

// Descriptors
typedef struct {
    GpuBufferType type;
    u32 size;
    void *data;  // initial data (NULL for dynamic)
} GpuBufferDesc;

typedef struct {
    const char *vs_code;  // WGSL source
    const char *fs_code;  // WGSL source
} GpuShaderDesc;

typedef struct {
    GpuVertexFormat format;
    u32 offset;
    u32 shader_location;
} GpuVertexAttr;

#define GPU_MAX_VERTEX_ATTRS 8

typedef struct {
    u32 stride;
    GpuVertexAttr attrs[GPU_MAX_VERTEX_ATTRS];
    u32 attr_count;
} GpuVertexLayout;

typedef struct {
    GpuShader shader;
    GpuVertexLayout vertex_layout;
    GpuPrimitiveTopology primitive;
    b32 depth_test;
    b32 depth_write;
} GpuPipelineDesc;

#define GPU_MAX_VERTEX_BUFFERS 4

typedef struct {
    GpuBuffer vertex_buffers[GPU_MAX_VERTEX_BUFFERS];
    u32 vertex_buffer_count;
    GpuBuffer index_buffer;
    GpuIndexFormat index_format;
    GpuBuffer uniform_buffer;
} GpuBindings;

typedef struct {
    f32 r, g, b, a;
} GpuColor;

typedef struct {
    GpuColor clear_color;
    f32 clear_depth;
} GpuPassDesc;

// Dynamic uniform buffer for batched rendering
typedef struct {
    ArenaAllocator arena;  // CPU-side staging (256-aligned base, reset each frame)
    GpuBuffer gpu_buf;     // GPU-side buffer
} GpuUniformBuffer;

// API Functions
void gpu_init(void);

GpuBuffer gpu_make_buffer(GpuBufferDesc *desc);
void gpu_update_buffer(GpuBuffer buf, void *data, u32 size);
void gpu_destroy_buffer(GpuBuffer buf);

GpuShader gpu_make_shader(GpuShaderDesc *desc);
void gpu_destroy_shader(GpuShader shd);

GpuPipeline gpu_make_pipeline(GpuPipelineDesc *desc);
void gpu_destroy_pipeline(GpuPipeline pip);

// Rendering (per-frame) - low level
void gpu_begin_pass(GpuPassDesc *desc);
void gpu_apply_pipeline(GpuPipeline pip);
void gpu_apply_bindings(GpuBindings *bindings);
void gpu_draw(u32 vertex_count, u32 instance_count);
void gpu_draw_indexed(u32 index_count, u32 instance_count);
void gpu_end_pass(void);
void gpu_commit(void);

// Dynamic uniform buffer API
void gpu_uniform_init(GpuUniformBuffer *ub, ArenaAllocator *parent_arena, u32 size);
u32 gpu_uniform_alloc(GpuUniformBuffer *ub, void *data, u32 size);
void gpu_uniform_flush(GpuUniformBuffer *ub);
void gpu_uniform_reset(GpuUniformBuffer *ub);

// Apply bindings with dynamic uniform offset
void gpu_apply_bindings_dynamic(GpuBindings *bindings, GpuBuffer uniform_buf, u32 uniform_offset);

// =============================================================================
// Render Commands - high level API for multithreaded rendering
// =============================================================================

typedef struct {
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
arr_define_concurrent(RenderCmd);

// Renderer initialization (call after gpu_init, sets up shared resources)
void renderer_init(void *arena);

// Called by main thread before parallel work begins
void renderer_begin_frame(mat4 view, mat4 proj, GpuColor clear_color);

// Called by ANY thread - lock-free append to command queue
void renderer_draw_mesh(mat4 model_matrix);

// Called by main thread after parallel work completes
void renderer_end_frame(void);

#endif
