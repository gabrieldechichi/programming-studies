#include "default2d.h"
#include "lib/string.h"
#include "wgpuex.h"
#include "GLFW/glfw3.h"

shader_default2d_pipeline_result_t
shader_default2d_createPipeline(WGPUDevice device, WGPULimits deviceLimits,
                                WGPUTextureFormat textureFormat) {
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

    WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc = {
        .label = "Bind group",
        .entryCount = 1,
        .entries = uniformEntries,
    };

    WGPUBindGroupLayout bindGroupLayout =
        wgpuDeviceCreateBindGroupLayout(device, &bindGroupLayoutDesc);

    WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {

        .label = "Default 2D",
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = &bindGroupLayout};

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


    shader_default2d_pipeline pipeline;
    pipeline.pipeline = wgpuDeviceCreateRenderPipeline(device, &pipelineDesc);

    wgpuShaderModuleRelease(module);

    // TODO: Move mesh away from pipeline
    mesh_result_t meshResult = loadGeometry("./resources/geometry/wgpu.geo");

    if (meshResult.error_code) {
        return (shader_default2d_pipeline_result_t){.error_code =
                                                        meshResult.error_code};
    }

    mesh_t mesh = meshResult.value;

    pipeline.vertexBuffer = createVertexBuffer(
        device, "Geometry Buffer", arrlen(mesh.vertices), mesh.vertices);
    pipeline.vertexBufferLen = arrlen(mesh.vertices);

    pipeline.indexBuffer = createIndexBuffer16(
        device, "Indices", arrlen(mesh.indices), mesh.indices);
    pipeline.indexBufferLen = arrlen(mesh.indices);

    int uniformBufferStride =
        ceilToNextMultiple(sizeof(shader_default2d_uniforms_t),
                           deviceLimits.minUniformBufferOffsetAlignment);
    pipeline.uniformBuffer = createUniformBuffer(
        device, "Uniform",
        (uniformBufferStride + sizeof(shader_default2d_uniforms_t)) /
            sizeof(float));
    pipeline.uniformBufferStride = uniformBufferStride;

    WGPUBindGroupEntry binding = {
        .binding = 0,
        .buffer = pipeline.uniformBuffer,
        .offset = 0,
        .size = sizeof(shader_default2d_uniforms_t),
    };

    WGPUBindGroupDescriptor uniformBindGroupDesc = {
        .label = "Uniform bind group",
        .layout = bindGroupLayout,
        .entryCount = bindGroupLayoutDesc.entryCount,
        .entries = &binding,
    };

    pipeline.uniformBindGroup =
        wgpuDeviceCreateBindGroup(device, &uniformBindGroupDesc);

    return (shader_default2d_pipeline_result_t){.value = pipeline};
}

void shader_default2d_pipelineRender(shader_default2d_pipeline pipeline,
                                     WGPURenderPassEncoder passEncoder,
                                     WGPUQueue queue) {
    wgpuRenderPassEncoderSetPipeline(passEncoder, pipeline.pipeline);
    wgpuRenderPassEncoderSetIndexBuffer(
        passEncoder, pipeline.indexBuffer, WGPUIndexFormat_Uint16, 0,
        pipeline.indexBufferLen * sizeof(uint16_t));
    wgpuRenderPassEncoderSetVertexBuffer(
        passEncoder, 0, pipeline.vertexBuffer, 0,
        pipeline.vertexBufferLen * sizeof(float));

    // draw 1
    {
        shader_default2d_uniforms_t uniforms = {
            .time = glfwGetTime(),
            .color = {0.5, 0.8, 0.5, 1.0},
        };

        wgpuQueueWriteBuffer(queue, pipeline.uniformBuffer, 0, &uniforms,
                             sizeof(uniforms));

        shader_default2d_uniforms_t uniforms2 = {
            .time = 2 * glfwGetTime() + 0.5,
            .color = {1, 1, 0, 1},
        };

        wgpuQueueWriteBuffer(queue, pipeline.uniformBuffer,
                             pipeline.uniformBufferStride, &uniforms2,
                             sizeof(uniforms2));

        uint32_t dynamicOffsets[1] = {0};

        wgpuRenderPassEncoderSetBindGroup(
            passEncoder, 0, pipeline.uniformBindGroup,
            ARRAY_LEN(dynamicOffsets), dynamicOffsets);

        wgpuRenderPassEncoderDrawIndexed(passEncoder, pipeline.indexBufferLen,
                                         1, 0, 0, 0);

        dynamicOffsets[0] = pipeline.uniformBufferStride;

        wgpuRenderPassEncoderSetBindGroup(
            passEncoder, 0, pipeline.uniformBindGroup,
            ARRAY_LEN(dynamicOffsets), dynamicOffsets);

        wgpuRenderPassEncoderDrawIndexed(passEncoder, pipeline.indexBufferLen,
                                         1, 0, 0, 0);
    }
}

void shader_default2d_free(shader_default2d_pipeline pipeline) {
    wgpuBufferRelease(pipeline.indexBuffer);
    wgpuBufferRelease(pipeline.vertexBuffer);
    wgpuRenderPipelineRelease(pipeline.pipeline);
}

