#ifndef SHADER_DEFAULT3D_H
#define SHADER_DEFAULT3D_H

#include "lib.h"
#include "webgpu/webgpu.h"
#include "cglm/cglm.h"

typedef struct {
    vec3 pos;
    vec3 normal;
    vec4 col;
} ShaderDefault3DVertexIn;

typedef struct {
    vec4 pos;
    vec3 normal;
    vec4 col;
} ShaderDefault3DVertexOut;

typedef struct {
    mat4 modelMatrix;
    mat4 viewMatrix;
    mat4 projectionMatrix;
    float time;
    float _time_padding[3];
} ShaderDefault3DUniforms;

typedef struct {
    WGPURenderPipeline pipeline;
    WGPUBindGroupLayout uniformsGroupLayout;
    WGPUBindGroupLayoutDescriptor uniformsGroupLayoutDesc;
} ShaderDefault3DPipeline;

RESULT_STRUCT(ShaderDefault3DPipeline);

ShaderDefault3DPipelineResult
shaderDefault3dCreatePipeline(WGPUDevice device,
                              WGPUTextureFormat textureFormat);
#endif
