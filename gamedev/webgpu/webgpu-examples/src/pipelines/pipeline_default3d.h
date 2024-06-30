#ifndef SHADER_DEFAULT3D_H
#define SHADER_DEFAULT3D_H

#include "lib.h"
#include "webgpu/webgpu.h"

typedef struct {
    float pos[3];
    float col[4];
} ShaderDefault3DVertexIn;

typedef struct {
    float pos[4];
    float col[4];
} ShaderDefault3DVertexOut;

typedef struct {
    WGPURenderPipeline pipeline;
} ShaderDefault3DPipeline;

RESULT_STRUCT(ShaderDefault3DPipeline);

ShaderDefault3DPipelineResult
shaderDefault3dCreatePipeline(WGPUDevice device,
                              WGPUTextureFormat textureFormat);
#endif
