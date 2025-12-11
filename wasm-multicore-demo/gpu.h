#ifndef H_GPU
#define H_GPU

#include "lib/typedefs.h"
#include "lib/handle.h"
#include "lib/array.h"
#include "lib/math.h"
#include "lib/memory.h"

// Uniform buffer constants
#define GPU_UNIFORM_BUFFER_SIZE MB(1)
//todo: query at runtime
#define GPU_UNIFORM_ALIGNMENT 256  // WebGPU minUniformBufferOffsetAlignment
#define GPU_MAX_UNIFORMBLOCK_SLOTS 4

// Resource handles
typedef Handle GpuBuffer;
typedef Handle GpuShader;
typedef Handle GpuPipeline;

#define GPU_INVALID_HANDLE INVALID_HANDLE

// Shader stage visibility for uniform blocks
typedef enum {
    GPU_STAGE_NONE = 0,
    GPU_STAGE_VERTEX = 1,
    GPU_STAGE_FRAGMENT = 2,
    GPU_STAGE_VERTEX_FRAGMENT = 3,
} GpuShaderStage;

// Uniform block description (part of shader)
typedef struct {
    GpuShaderStage stage;  // which shader stage(s) can access this
    u32 size;              // size of uniform struct
    u32 binding;           // WGSL @binding(n)
} GpuUniformBlockDesc;

// Resource slot types (stored in handle arrays, actual GPU data lives in JS)
typedef struct { u8 _unused; } GpuBufferSlot;
typedef struct {
    GpuUniformBlockDesc uniform_blocks[GPU_MAX_UNIFORMBLOCK_SLOTS];
    u32 uniform_block_count;
} GpuShaderSlot;
typedef struct { u8 _unused; } GpuPipelineSlot;

HANDLE_ARRAY_DEFINE(GpuBufferSlot);
HANDLE_ARRAY_DEFINE(GpuShaderSlot);
HANDLE_ARRAY_DEFINE(GpuPipelineSlot);

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
    GpuUniformBlockDesc uniform_blocks[GPU_MAX_UNIFORMBLOCK_SLOTS];
    u32 uniform_block_count;
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
    //todo: use flags here
    b32 depth_test;
    b32 depth_write;
} GpuPipelineDesc;

#define GPU_MAX_VERTEX_BUFFERS 4

typedef struct {
    GpuBuffer vertex_buffers[GPU_MAX_VERTEX_BUFFERS];
    u32 vertex_buffer_count;
    GpuBuffer index_buffer;
    GpuIndexFormat index_format;
} GpuBindings;

//todo: unions?
typedef struct {
    f32 r, g, b, a;
} GpuColor;

typedef struct {
    GpuColor clear_color;
    f32 clear_depth;
} GpuPassDesc;

typedef struct {
    GpuBuffer vbuf;
    GpuBuffer ibuf;
    u32 index_count;
    GpuIndexFormat index_format;
} GpuMesh;

TYPED_HANDLE_DEFINE(GpuMesh);   // -> Mesh_Handle
HANDLE_ARRAY_DEFINE(GpuMesh);   // -> HandleArray_Mesh

// API Functions
void gpu_init(ArenaAllocator *arena, u32 uniform_buffer_size);

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
void gpu_apply_uniforms(u32 slot, void *data, u32 size);
void gpu_apply_bindings(GpuBindings *bindings);
void gpu_draw(u32 vertex_count, u32 instance_count);
void gpu_draw_indexed(u32 index_count, u32 instance_count);
void gpu_end_pass(void);
void gpu_commit(void);

#endif
