#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED

#include "renderer_basic3d.h"
#include "cglm/affine-pre.h"
#include "cglm/affine.h"
#include "cglm/cam.h"
#include "cglm/mat4.h"
#include "cglm/types.h"
#include "cglm/util.h"
#include "lib.h"
#include "pipelines/pipeline_default3d.h"
#include "stb/stb_ds.h"
#include "webgpu/webgpu.h"
#include "wgpuex.h"
#include <GLFW/glfw3.h>
#include <cglm/util.h>
#include <stdint.h>

RendererBasic3DResult rendererBasic3dCreate(WGPUDevice device,
                                            WGPULimits deviceLimits,
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

    int uniformBufferStride =
        ceilToNextMultiple(sizeof(ShaderDefault3DUniforms),
                           deviceLimits.minUniformBufferOffsetAlignment);

    renderer.uniformBuffer = createUniformBuffer(
        device, "Uniforms", uniformBufferStride / sizeof(float));
    renderer.uniformBufferStride = uniformBufferStride;

    WGPUBindGroupEntry binding = {
        .binding = 0,
        .buffer = renderer.uniformBuffer,
        .offset = 0,
        .size = sizeof(ShaderDefault3DUniforms),
    };

    WGPUBindGroupDescriptor uniformBindGroupDesc = {
        .label = "Uniform bind group",
        .layout = renderer.pipeline.uniformsGroupLayout,
        .entryCount = renderer.pipeline.uniformsGroupLayoutDesc.entryCount,
        .entries = &binding,
    };

    renderer.uniformBindGroup =
        wgpuDeviceCreateBindGroup(device, &uniformBindGroupDesc);

    return (RendererBasic3DResult){.value = renderer};
}

void rendererBasic3dRender(RendererBasic3D renderer,
                           WGPURenderPassEncoder passEncoder, WGPUQueue queue) {
    wgpuRenderPassEncoderSetPipeline(passEncoder, renderer.pipeline.pipeline);
    wgpuRenderPassEncoderSetIndexBuffer(
        passEncoder, renderer.indexBuffer, WGPUIndexFormat_Uint16, 0,
        renderer.indexBufferLen * sizeof(uint16_t));
    wgpuRenderPassEncoderSetVertexBuffer(
        passEncoder, 0, renderer.vertexBuffer, 0,
        renderer.vertexBufferLen * sizeof(float));

    mat4 model = GLM_MAT4_IDENTITY_INIT;
    mat4 view = GLM_MAT4_IDENTITY_INIT;
    mat4 projection = GLM_MAT4_IDENTITY_INIT;
    glm_scale(model, (vec3){0.5, 0.6, 0.5});
    glm_rotate_x(model, glm_rad(45), model);
    glm_rotate_z(model, glm_rad(20), model);
    glm_rotate_y(model, glfwGetTime(), model);

    glm_translate(view, (vec3){0.0, 0.0, -2.0});

    glm_perspective(glm_rad(45), 1, 0.01, 10, projection);

    ShaderDefault3DUniforms uniforms = {.time = glfwGetTime()};

    glm_mat4_copy(model, uniforms.modelMatrix);
    glm_mat4_copy(view, uniforms.viewMatrix);
    glm_mat4_copy(projection, uniforms.projectionMatrix);

    wgpuQueueWriteBuffer(queue, renderer.uniformBuffer, 0, &uniforms,
                         sizeof(ShaderDefault3DUniforms));

    wgpuRenderPassEncoderSetBindGroup(passEncoder, 0, renderer.uniformBindGroup,
                                      0, NULL);

    wgpuRenderPassEncoderDrawIndexed(passEncoder, renderer.indexBufferLen, 1, 0,
                                     0, 0);
}

void rendererBasic3dFree(RendererBasic3D renderer) {
    wgpuBufferRelease(renderer.indexBuffer);
    wgpuBufferRelease(renderer.vertexBuffer);
    wgpuRenderPipelineRelease(renderer.pipeline.pipeline);
}
