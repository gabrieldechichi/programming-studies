#ifndef RENDERER_BASIC2D_H
#define RENDERER_BASIC2D_H

#include "lib.h"
#include "pipelines/default2d.h"

typedef struct {
    shader_default2d_pipeline pipeline;
    WGPUBuffer vertexBuffer;
    int vertexBufferLen;
    WGPUBuffer indexBuffer;
    int indexBufferLen;
    WGPUBuffer uniformBuffer;
    int uniformBufferStride;
    WGPUBindGroup uniformBindGroup;
} renderer_basic2d;

RESULT_STRUCT(renderer_basic2d);

renderer_basic2d_result_t
renderer_basic2d_create(WGPUDevice device, WGPULimits deviceLimits,
                        WGPUTextureFormat textureFormat);

void renderer_basic2d_render(renderer_basic2d renderer,
                             WGPURenderPassEncoder passEncoder,
                             WGPUQueue queue);

void renderer_basic2d_free(renderer_basic2d renderer);
#endif
