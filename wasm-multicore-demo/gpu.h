#ifndef H_GPU
#define H_GPU

#include "lib/typedefs.h"
#include "lib/handle.h"

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

// API Functions
void gpu_init(void);

GpuBuffer gpu_make_buffer(GpuBufferDesc *desc);
void gpu_update_buffer(GpuBuffer buf, void *data, u32 size);
void gpu_destroy_buffer(GpuBuffer buf);

GpuShader gpu_make_shader(GpuShaderDesc *desc);
void gpu_destroy_shader(GpuShader shd);

GpuPipeline gpu_make_pipeline(GpuPipelineDesc *desc);
void gpu_destroy_pipeline(GpuPipeline pip);

// Rendering (per-frame)
void gpu_begin_pass(GpuPassDesc *desc);
void gpu_apply_pipeline(GpuPipeline pip);
void gpu_apply_bindings(GpuBindings *bindings);
void gpu_draw(u32 vertex_count, u32 instance_count);
void gpu_draw_indexed(u32 index_count, u32 instance_count);
void gpu_end_pass(void);
void gpu_commit(void);

#endif
