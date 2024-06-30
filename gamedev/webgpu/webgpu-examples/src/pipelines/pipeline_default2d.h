#ifndef SHADER_DEFAULT2D_H
#define SHADER_DEFAULT2D_H

#include "lib.h"
#include "webgpu/webgpu.h"

typedef struct {
    float pos[2];
    float col[4];
} shader_default2d_VertexIn;

typedef struct {
    float pos[4];
    float col[4];
} shader_default2d_VertexOut;

typedef struct {
    float time;
    float _time_padding[3];
    float color[4];
} shader_default2d_uniforms_t;

typedef struct {
    WGPURenderPipeline pipeline;
    WGPUBindGroupLayout bindGroupLayout;
    WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc;
} shader_default2d_pipeline;

RESULT_STRUCT(shader_default2d_pipeline);

shader_default2d_pipeline_result_t
shader_default2d_createPipeline(WGPUDevice device,
                                WGPUTextureFormat textureFormat);
#endif
