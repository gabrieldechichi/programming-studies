#include "renderer_basic3d.h"
#include "lib.h"
#include "pipelines/pipeline_default3d.h"
#include "stb/stb_ds.h"
#include "webgpu/webgpu.h"
#include "wgpuex.h"
#include <stdint.h>

RendererBasic3DResult rendererBasic3dCreate(WGPUDevice device,
                                            WGPUTextureFormat textureFormat) {

    ShaderDefault3DPipelineResult pipelineResult =
        shaderDefault3dCreatePipeline(device, textureFormat);
    RETURN_IF_ERROR(pipelineResult, RendererBasic3DResult);

    MeshResult meshResult = loadGeometry("./resources/geometry/pyramid.geo");
    RETURN_IF_ERROR(meshResult, RendererBasic3DResult);

    RendererBasic3D renderer = {0};
    renderer.pipeline = pipelineResult.value;

    Mesh mesh = meshResult.value;
    renderer.vertexBuffer = createVertexBuffer(
        device, "Pyramid Vertex", arrlen(mesh.vertices), mesh.vertices);
    renderer.vertexBufferLen = arrlen(mesh.vertices);

    renderer.indexBuffer = createIndexBuffer16(
        device, "Pyramid Indices", arrlen(mesh.indices), mesh.indices);
    renderer.indexBufferLen = arrlen(mesh.indices);

    return (RendererBasic3DResult){.value = renderer};
}

void rendererBasic3dRender(RendererBasic3D renderer,
                           WGPURenderPassEncoder passEncoder, WGPUQueue queue) {
    UNUSED(queue);
    wgpuRenderPassEncoderSetPipeline(passEncoder, renderer.pipeline.pipeline);
    wgpuRenderPassEncoderSetIndexBuffer(
        passEncoder, renderer.indexBuffer, WGPUIndexFormat_Uint16, 0,
        renderer.indexBufferLen * sizeof(uint16_t));
    wgpuRenderPassEncoderSetVertexBuffer(
        passEncoder, 0, renderer.vertexBuffer, 0,
        renderer.vertexBufferLen * sizeof(float));

    wgpuRenderPassEncoderDrawIndexed(passEncoder, renderer.indexBufferLen, 1, 0,
                                     0, 0);
}

void rendererBasic3dFree(RendererBasic3D renderer) {
    wgpuBufferRelease(renderer.indexBuffer);
    wgpuBufferRelease(renderer.vertexBuffer);
    wgpuRenderPipelineRelease(renderer.pipeline.pipeline);
}
