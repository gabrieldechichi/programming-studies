#ifndef SHADER_DEFAULT2D_H
#define SHADER_DEFAULT2D_H

#include "lib.h"
#include "webgpu/webgpu.h"

typedef struct {
    float pos[2];
    float col[4];
} ShaderDefault2DVertexIn;

typedef struct {
    float pos[4];
    float col[4];
} ShaderDefault2DVertexOut;

typedef struct {
    float time;
    float _time_padding[3];
    float color[4];
} ShaderDefault2DUniforms;

typedef struct {
    WGPURenderPipeline pipeline;
    WGPUBindGroupLayout bindGroupLayout;
    WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc;
} ShaderDefault2DPipeline;

RESULT_STRUCT(ShaderDefault2DPipeline);

ShaderDefault2DPipelineResult
shaderDefault2dCreatePipeline(WGPUDevice device,
                                WGPUTextureFormat textureFormat);
#endif
