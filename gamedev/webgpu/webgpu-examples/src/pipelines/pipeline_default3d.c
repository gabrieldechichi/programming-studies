#include "pipeline_default3d.h"
#include "lib.h"
#include "webgpu/webgpu.h"
#include "wgpuex.h"

static const char *shaderName = "default3d";

ShaderDefault3DPipelineResult
shaderDefault3dCreatePipeline(WGPUDevice device,
                              WGPUTextureFormat textureFormat) {
    ShaderDefault3DPipeline pipeline;

    WGPUShaderModuleResult shaderModuleResult =
        wgpuCreateWGSLShaderModule(device, "shaders/default3d.wgsl");

    if (shaderModuleResult.errorCode) {
        return (ShaderDefault3DPipelineResult){
            .errorCode = shaderModuleResult.errorCode};
    }

    WGPUVertexAttribute vertexBufAttributes[] = {
        {.format = WGPUVertexFormat_Float32x3,
         .offset = offsetof(ShaderDefault3DVertexIn, pos),
         .shaderLocation = 0},
        {.format = WGPUVertexFormat_Float32x4,
         .offset = offsetof(ShaderDefault3DVertexIn, col),
         .shaderLocation = 1},
    };

    WGPUVertexBufferLayout vertexBuffers[] = {
        {.arrayStride = (sizeof(ShaderDefault3DVertexIn)),
         .stepMode = WGPUVertexStepMode_Vertex,
         .attributeCount = ARRAY_LEN(vertexBufAttributes),
         .attributes = vertexBufAttributes}};

    WGPUVertexState vertex = {.module = shaderModuleResult.value,
                              .entryPoint = "vs_main",
                              .bufferCount = ARRAY_LEN(vertexBuffers),
                              .buffers = vertexBuffers};

    WGPUBlendState blendState = {
        .color =
            {
                .operation = WGPUBlendOperation_Add,
                .srcFactor = WGPUBlendFactor_SrcAlpha,
                .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha,
            },
        .alpha = {
            .operation = WGPUBlendOperation_Add,
            .srcFactor = WGPUBlendFactor_Zero,
            .dstFactor = WGPUBlendFactor_One,
        }};

    WGPUColorTargetState targetState = {
        .format = textureFormat,
        .blend = &blendState,
        .writeMask = WGPUColorWriteMask_All,
    };

    WGPUFragmentState fragment = {
        .module = shaderModuleResult.value,
        .entryPoint = "fs_main",
        .targetCount = 1,
        .targets = &targetState,
    };

    WGPURenderPipelineDescriptor pipelineDesc = {
        .label = shaderName,
        .vertex = vertex,
        .fragment = &fragment,
        .primitive = {.topology = WGPUPrimitiveTopology_TriangleList,
                      .frontFace = WGPUFrontFace_CCW,
                      .cullMode = WGPUCullMode_Back},
        .multisample = {
            .count = 1, .mask = ~0, .alphaToCoverageEnabled = false}};

    pipeline.pipeline = wgpuDeviceCreateRenderPipeline(device, &pipelineDesc);
    return (ShaderDefault3DPipelineResult){.value = pipeline};
}
