#ifndef RENDERER_BASIC2D_H
#define RENDERER_BASIC2D_H

#include "lib.h"
#include "pipelines/pipeline_default2d.h"

typedef struct {
    ShaderDefault2DPipeline pipeline;
    WGPUBuffer vertexBuffer;
    int vertexBufferLen;
    WGPUBuffer indexBuffer;
    int indexBufferLen;
    WGPUBuffer uniformBuffer;
    int uniformBufferStride;
    WGPUBindGroup uniformBindGroup;
} RendererBasic2D;

RESULT_STRUCT(RendererBasic2D);

RendererBasic2DResult
rendererBasic2dCreate(WGPUDevice device, WGPULimits deviceLimits,
                        WGPUTextureFormat textureFormat);

void rendererBasic2dRender(RendererBasic2D renderer,
                             WGPURenderPassEncoder passEncoder,
                             WGPUQueue queue);

void rendererBasic2dFree(RendererBasic2D renderer);
#endif
