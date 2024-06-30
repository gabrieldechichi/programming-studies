#include "default2d.h"
#include "lib/string.h"
#include "webgpu/webgpu.h"

shader_default2d_pipeline_result_t
shader_default2d_createPipeline(WGPUDevice device, 
                                WGPUTextureFormat textureFormat) {

    shader_default2d_pipeline pipeline;

    string_result_t shaderSourceResult =
        fileReadAllText("shaders/default2d.wgsl");
    if (shaderSourceResult.error_code) {
        return (shader_default2d_pipeline_result_t){
            .error_code = shaderSourceResult.error_code};
    }

    WGPUShaderModuleWGSLDescriptor wgslDesc = {
        .chain = {.sType = WGPUSType_ShaderModuleWGSLDescriptor},
        .code = shaderSourceResult.value,
    };

    WGPUShaderModuleDescriptor moduleDesc = {
        .nextInChain = &wgslDesc.chain,
        .label = "Hello WGPU",
    };

    WGPUShaderModule module = wgpuDeviceCreateShaderModule(device, &moduleDesc);

    str_free(&shaderSourceResult.value);

    WGPUVertexAttribute vertexBuffAttributes[] = {
        {.format = WGPUVertexFormat_Float32x2,
         .offset = 0,
         .shaderLocation = 0},
        {.format = WGPUVertexFormat_Float32x4,
         .offset = 2 * sizeof(float),
         .shaderLocation = 1},
    };

    WGPUVertexBufferLayout vertexBuffers[1] = {
        {.arrayStride = (2 + 4) * sizeof(float),
         .stepMode = WGPUVertexStepMode_Vertex,
         .attributeCount = ARRAY_LEN(vertexBuffAttributes),
         .attributes = vertexBuffAttributes}};

    WGPUVertexState vertex = {
        .module = module,
        .entryPoint = "vs_main",
        .bufferCount = 1,
        .buffers = vertexBuffers,
    };

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

        .module = module,
        .entryPoint = "fs_main",
        .targetCount = 1,
        .targets = &targetState,
    };

    WGPUBindGroupLayoutEntry uniformEntries[1] = {
        {.binding = 0,
         .visibility = WGPUShaderStage_Vertex,
         .buffer = {
             .type = WGPUBufferBindingType_Uniform,
             .minBindingSize = sizeof(shader_default2d_uniforms_t),
             .hasDynamicOffset = true,
         }}};

    pipeline.bindGroupLayoutDesc = (WGPUBindGroupLayoutDescriptor){
        .label = "Bind group",
        .entryCount = 1,
        .entries = uniformEntries,
    };

    pipeline.bindGroupLayout =
        wgpuDeviceCreateBindGroupLayout(device, &pipeline.bindGroupLayoutDesc);

    WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {

        .label = "Default 2D",
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = &pipeline.bindGroupLayout};

    WGPURenderPipelineDescriptor pipelineDesc = {
        .label = "Default 2D",
        .layout = wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDesc),
        .vertex = vertex,
        .fragment = &fragment,
        .primitive =
            {
                .topology = WGPUPrimitiveTopology_TriangleList,
                .stripIndexFormat = WGPUIndexFormat_Undefined,
                .frontFace = WGPUFrontFace_CCW,
                .cullMode = WGPUCullMode_Back,
            },
        .depthStencil = NULL,
        .multisample = {.count = 1,
                        .mask = ~0,
                        .alphaToCoverageEnabled = false},
    };

    pipeline.pipeline = wgpuDeviceCreateRenderPipeline(device, &pipelineDesc);

    return (shader_default2d_pipeline_result_t){.value = pipeline};
}
