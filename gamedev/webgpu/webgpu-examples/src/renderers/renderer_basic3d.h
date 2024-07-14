#ifndef RENDERER_BASIC3D_H
#define RENDERER_BASIC3D_H


#include "lib.h"
#include "pipelines/pipeline_default3d.h"
#include "webgpu/webgpu.h"

typedef struct {
    ShaderDefault3DPipeline pipeline;
    WGPUBuffer vertexBuffer;
    int vertexBufferLen;
    WGPUBuffer uniformBuffer;
    int uniformBufferStride;
    WGPUBindGroup uniformBindGroup;
} RendererBasic3D;

RESULT_STRUCT(RendererBasic3D);

RendererBasic3DResult
rendererBasic3dCreate(WGPUDevice device, WGPULimits deviceLimits, WGPUTextureFormat textureFormat);

void rendererBasic3dFree(RendererBasic3D renderer);

void rendererBasic3dRender(RendererBasic3D renderer,
                           WGPURenderPassEncoder passEncoder, WGPUQueue queue);
#endif
