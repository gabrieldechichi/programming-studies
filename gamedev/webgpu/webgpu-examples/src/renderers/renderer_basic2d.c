#include "renderer_basic2d.h"
#include "GLFW/glfw3.h"
#include "lib.h"
#include "pipelines/pipeline_default2d.h"
#include "stb/stb_ds.h"
#include "webgpu/webgpu.h"
#include "wgpuex.h"

RendererBasic2DResult
rendererBasic2dCreate(WGPUDevice device, WGPULimits deviceLimits,
                        WGPUTextureFormat textureFormat) {
    ShaderDefault2DPipelineResult pipelineResult =
        shaderDefault2dCreatePipeline(device, textureFormat);
    if (pipelineResult.errorCode) {
        return (RendererBasic2DResult){.errorCode =
                                               pipelineResult.errorCode};
    }

    RendererBasic2D renderer = {0};
    renderer.pipeline = pipelineResult.value;

    MeshResult meshResult = loadGeometry("./resources/geometry/wgpu.geo");

    if (meshResult.errorCode) {
        return (RendererBasic2DResult){.errorCode = meshResult.errorCode};
    }

    Mesh mesh = meshResult.value;

    renderer.vertexBuffer = createVertexBuffer(
        device, "Geometry Buffer", arrlen(mesh.vertices), mesh.vertices);
    renderer.vertexBufferLen = arrlen(mesh.vertices);

    renderer.indexBuffer = createIndexBuffer16(
        device, "Indices", arrlen(mesh.indices), mesh.indices);
    renderer.indexBufferLen = arrlen(mesh.indices);

    int uniformBufferStride =
        ceilToNextMultiple(sizeof(ShaderDefault2DUniforms),
                           deviceLimits.minUniformBufferOffsetAlignment);
    renderer.uniformBuffer = createUniformBuffer(
        device, "Uniform",
        (uniformBufferStride + sizeof(ShaderDefault2DUniforms)) /
            sizeof(float));
    renderer.uniformBufferStride = uniformBufferStride;

    WGPUBindGroupEntry binding = {
        .binding = 0,
        .buffer = renderer.uniformBuffer,
        .offset = 0,
        .size = sizeof(ShaderDefault2DUniforms),
    };

    WGPUBindGroupDescriptor uniformBindGroupDesc = {
        .label = "Uniform bind group",
        .layout = renderer.pipeline.bindGroupLayout,
        .entryCount = renderer.pipeline.bindGroupLayoutDesc.entryCount,
        .entries = &binding,
    };

    renderer.uniformBindGroup =
        wgpuDeviceCreateBindGroup(device, &uniformBindGroupDesc);

    return (RendererBasic2DResult){.value = renderer};
}

void rendererBasic2dRender(RendererBasic2D renderer,
                             WGPURenderPassEncoder passEncoder,
                             WGPUQueue queue) {
    wgpuRenderPassEncoderSetPipeline(passEncoder, renderer.pipeline.pipeline);
    wgpuRenderPassEncoderSetIndexBuffer(
        passEncoder, renderer.indexBuffer, WGPUIndexFormat_Uint16, 0,
        renderer.indexBufferLen * sizeof(uint16_t));
    wgpuRenderPassEncoderSetVertexBuffer(
        passEncoder, 0, renderer.vertexBuffer, 0,
        renderer.vertexBufferLen * sizeof(float));

    // draw 1
    {
        ShaderDefault2DUniforms uniforms = {
            .time = (float)glfwGetTime(),
            .color = {0.5, 0.8, 0.5, 1.0},
        };

        wgpuQueueWriteBuffer(queue, renderer.uniformBuffer, 0, &uniforms,
                             sizeof(uniforms));

        ShaderDefault2DUniforms uniforms2 = {
            .time = 2.0 * (float)glfwGetTime() + 0.5,
            .color = {1, 1, 0, 1},
        };

        wgpuQueueWriteBuffer(queue, renderer.uniformBuffer,
                             renderer.uniformBufferStride, &uniforms2,
                             sizeof(uniforms2));

        uint32_t dynamicOffsets[1] = {0};

        wgpuRenderPassEncoderSetBindGroup(
            passEncoder, 0, renderer.uniformBindGroup,
            ARRAY_LEN(dynamicOffsets), dynamicOffsets);

        wgpuRenderPassEncoderDrawIndexed(passEncoder, renderer.indexBufferLen, 1, 0, 0, 0);

        dynamicOffsets[0] = renderer.uniformBufferStride;

        wgpuRenderPassEncoderSetBindGroup(
            passEncoder, 0, renderer.uniformBindGroup,
            ARRAY_LEN(dynamicOffsets), dynamicOffsets);

        wgpuRenderPassEncoderDrawIndexed(passEncoder, renderer.indexBufferLen,
                                         1, 0, 0, 0);
    }
}

void rendererBasic2dFree(RendererBasic2D renderer) {
    wgpuBufferRelease(renderer.indexBuffer);
    wgpuBufferRelease(renderer.vertexBuffer);
    wgpuBufferRelease(renderer.uniformBuffer);
    wgpuBindGroupRelease(renderer.uniformBindGroup);
    wgpuRenderPipelineRelease(renderer.pipeline.pipeline);
}
