#include "GLFW/glfw3.h"
#include "glfw3webgpu.h"
#include "lib.h"
#include "stb_ds.h"
#include "stdbool.h"
#include "stdio.h"
#include "webgpu.h"
#include "wgpuex.h"
#include <string.h>

const unsigned int WIDTH = 800;
const unsigned int HEIGHT = 600;

typedef struct {
    WGPUDevice device;
    WGPUQueue queue;
    WGPUSurface surface;
    WGPURenderPipeline pipeline;
    WGPUBuffer vertexBuffer;
    WGPUTextureFormat textureFormat;
} WgpuState;

typedef struct {
    GLFWwindow *window;
    WgpuState wgpu;
} AppData;

typedef int (*AppInitCallback)(AppData *app_data);
typedef void (*AppUpdateCallback)(AppData *app_data);
typedef bool (*AppIsRunningCallback)(AppData *app_data);
typedef void (*AppTerminateCallback)(AppData *app_data);

typedef struct {
    AppInitCallback init;
    AppIsRunningCallback isRunning;
    AppUpdateCallback update;
    AppTerminateCallback terminate;
} App;

void inspectAdapter(WGPUAdapter adapter) {
    WGPUAdapterProperties adapterProperties = {0};
    wgpuAdapterGetProperties(adapter, &adapterProperties);

    printf("Adapter properties:\n");
    printf("\t- vendorID: %d\n", adapterProperties.vendorID);
    if (adapterProperties.vendorName) {
        printf("\t- vendorName: %s\n", adapterProperties.vendorName);
    }
    if (adapterProperties.architecture) {
        printf("\t- architecture: %s\n", adapterProperties.architecture);
    }
    printf("\t- deviceID: %d\n", adapterProperties.deviceID);
    if (adapterProperties.name) {
        printf("\t- name: %s\n", adapterProperties.name);
    }
    if (adapterProperties.driverDescription) {
        printf("\t- driverDescription: %s\n",
               adapterProperties.driverDescription);
    }
}

void inspectDevice(WGPUDevice device) {
    size_t featureCount = wgpuDeviceEnumerateFeatures(device, NULL);
    if (featureCount > 0) {
        WGPUFeatureName *features = NULL;
        arrsetlen(features, featureCount);
        wgpuDeviceEnumerateFeatures(device, features);

        printf("Device features:\n");
        for (size_t i = 0; i < (size_t)arrlen(features); ++i) {
            printf("\t0x%X\n", features[i]);
        }
    }

    WGPUSupportedLimits limits = {};
    limits.nextInChain = NULL;

    WGPUBool success = wgpuDeviceGetLimits(device, &limits);

    if (success) {
        printf("Device Limits:\n");
        printf("\t- maxTextureDimension1D: %d\n",
               limits.limits.maxTextureDimension1D);
        printf("\t- maxTextureDimension2D: %d\n",
               limits.limits.maxTextureDimension2D);
        printf("\t- maxTextureDimension3D: %d\n",
               limits.limits.maxTextureDimension3D);
        printf("\t- maxTextureArrayLayers: %d\n",
               limits.limits.maxTextureArrayLayers);
    }
}

void wgpuQueueWorkDoneCallback(WGPUQueueWorkDoneStatus status, void *userdata) {
    UNUSED(userdata);
    printf("Queue %s work finished: 0x%X\n", "default queue", status);
}

int appInit(AppData *app_data) {
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize glfw\n");
        return ERR_CODE_FAIL;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    app_data->window =
        glfwCreateWindow(WIDTH, HEIGHT, "WebGPU examples", NULL, NULL);
    if (!app_data->window) {
        fprintf(stderr, "Failed to create wgpu window\n");
        return ERR_CODE_FAIL;
    }

    //  instance
    WGPUInstance instance = wgpuCreateInstance(NULL);
    assert(instance);

    // surface
    app_data->wgpu.surface = glfwGetWGPUSurface(instance, app_data->window);

    // adapter
    WGPURequestAdapterOptions options = {
        .compatibleSurface = app_data->wgpu.surface,
        .powerPreference = WGPUPowerPreference_HighPerformance};

    WGPUAdapter adapter = wgpuRequestAdapterSync(instance, &options).adapter;

    inspectAdapter(adapter);

    // device
    app_data->wgpu.device = wgpuRequestDeviceSync(adapter, NULL).device;
    inspectDevice(app_data->wgpu.device);

    // configure surface
    {
        app_data->wgpu.textureFormat =
            wgpuSurfaceGetPreferredFormat(app_data->wgpu.surface, adapter);
        WGPUSurfaceConfiguration surface_conf = {
            .nextInChain = NULL,
            .width = WIDTH,
            .height = HEIGHT,
            .device = app_data->wgpu.device,
            .format = app_data->wgpu.textureFormat,
            .usage = WGPUTextureUsage_RenderAttachment,
            .presentMode = WGPUPresentMode_Fifo,
            .alphaMode = WGPUCompositeAlphaMode_Auto,
        };
        wgpuSurfaceConfigure(app_data->wgpu.surface, &surface_conf);
    }

    // queue
    {
        app_data->wgpu.queue = wgpuDeviceGetQueue(app_data->wgpu.device);
        wgpuQueueOnSubmittedWorkDone(app_data->wgpu.queue,
                                     wgpuQueueWorkDoneCallback, NULL);
    }

    wgpuInstanceRelease(instance);
    wgpuAdapterRelease(adapter);

    // pipeline
    {
        char *shaderSource = fileReadAllText("shaders/triangle.wgsl");

        WGPUShaderModuleWGSLDescriptor wgslDesc = {
            .chain = {.sType = WGPUSType_ShaderModuleWGSLDescriptor},
            .code = shaderSource,
        };

        WGPUShaderModuleDescriptor moduleDesc = {
            .nextInChain = &wgslDesc.chain,
            .label = "Hello Triangle",
        };

        WGPUShaderModule module =
            wgpuDeviceCreateShaderModule(app_data->wgpu.device, &moduleDesc);

        free(shaderSource);

        WGPUVertexAttribute vertexBuffAttributes[1] = {
            {.format = WGPUVertexFormat_Float32x2,
             .offset = 0,
             .shaderLocation = 0}};

        WGPUVertexBufferLayout vertexBuffers[1] = {
            {.arrayStride = 2 * sizeof(float),
             .stepMode = WGPUVertexStepMode_Vertex,
             .attributeCount = 1,
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
            .format = app_data->wgpu.textureFormat,
            .blend = &blendState,
            .writeMask = WGPUColorWriteMask_All,
        };

        WGPUFragmentState fragment = {

            .module = module,
            .entryPoint = "fs_main",
            .targetCount = 1,
            .targets = &targetState,
        };

        WGPURenderPipelineDescriptor pipelineDesc = {
            .label = "Hello Triangle",
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
        app_data->wgpu.pipeline = wgpuDeviceCreateRenderPipeline(
            app_data->wgpu.device, &pipelineDesc);
        wgpuShaderModuleRelease(module);

        // clang-format off
        float vertices[6] = {
            -0.5, -0.5,
            0.5, -0.5,
            0.0, 0.5
        };
        // clang-format on

        app_data->wgpu.vertexBuffer =
            createVertexBuffer(app_data->wgpu.device, "Geometry Buffer",
                               ARRAY_LEN(vertices), vertices);
    }

    return ERR_CODE_SUCCESS;
}

bool appIsRunning(AppData *app_data) {
    return !glfwWindowShouldClose(app_data->window);
}

// TODO: separate update and render
void appUpdate(AppData *app_data) {
    glfwPollEvents();
    if (glfwGetKey(app_data->window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(app_data->window, GLFW_TRUE);
        return;
    }

    // START grab surface texture
    WGPUSurfaceTexture surfaceTex;
    wgpuSurfaceGetCurrentTexture(app_data->wgpu.surface, &surfaceTex);
    if (surfaceTex.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
        return;
    }

    WGPUTextureViewDescriptor texViewDescritor = {
        .label = "Frame",
        .format = wgpuTextureGetFormat(surfaceTex.texture),
        .dimension = WGPUTextureViewDimension_2D,
        .baseMipLevel = 0,
        .mipLevelCount = 1,
        .baseArrayLayer = 0,
        .arrayLayerCount = 1,
        .aspect = WGPUTextureAspect_All};

    WGPUTextureView targetView =
        wgpuTextureCreateView(surfaceTex.texture, &texViewDescritor);
    // END grab target tex

    // start command encodere
    WGPUCommandEncoderDescriptor cmdencoder_desc =
        (WGPUCommandEncoderDescriptor){.label = "My command encoder"};
    WGPUCommandEncoder cmdencoder =
        wgpuDeviceCreateCommandEncoder(app_data->wgpu.device, &cmdencoder_desc);
    // end cmd encoder

    // create render pass (only clearing the screen for now)
    {
        WGPURenderPassColorAttachment colorAttachment = {
            .view = targetView,
            .resolveTarget = NULL,
            .loadOp = WGPULoadOp_Clear,
            .storeOp = WGPUStoreOp_Store,
            .clearValue = {1, 0, 0, 1}};

        WGPURenderPassDescriptor renderPassDesc = {.label = "Frame render pass",
                                                   .colorAttachmentCount = 1,
                                                   .colorAttachments =
                                                       &colorAttachment};

        WGPURenderPassEncoder passEncoder =
            wgpuCommandEncoderBeginRenderPass(cmdencoder, &renderPassDesc);

        wgpuRenderPassEncoderSetPipeline(passEncoder, app_data->wgpu.pipeline);
        wgpuRenderPassEncoderSetVertexBuffer(
            passEncoder, 0, app_data->wgpu.vertexBuffer, 0,
            wgpuBufferGetSize(app_data->wgpu.vertexBuffer));
        wgpuRenderPassEncoderDraw(passEncoder, 3, 1, 0, 0);
        wgpuRenderPassEncoderEnd(passEncoder);
        wgpuRenderPassEncoderRelease(passEncoder);
    }

    // command buffer commit
    WGPUCommandBufferDescriptor cmdBuffDesc = {.label = "Frame Command Buffer"};
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(cmdencoder, &cmdBuffDesc);
    wgpuCommandEncoderRelease(cmdencoder);

    // submit queue
    wgpuQueueSubmit(app_data->wgpu.queue, 1, &cmd);
    wgpuCommandBufferRelease(cmd);

    wgpuSurfacePresent(app_data->wgpu.surface);
    wgpuTextureViewRelease(targetView);
}

void appTerminate(AppData *app_data) {
    wgpuRenderPipelineRelease(app_data->wgpu.pipeline);
    wgpuSurfaceUnconfigure(app_data->wgpu.surface);
    wgpuSurfaceRelease(app_data->wgpu.surface);
    wgpuQueueRelease(app_data->wgpu.queue);
    wgpuDeviceRelease(app_data->wgpu.device);

    glfwTerminate();
    glfwDestroyWindow(app_data->window);
}

int main(void) {
    AppData app_data = {0};
    App app = (App){
        .init = appInit,
        .isRunning = appIsRunning,
        .update = appUpdate,
        .terminate = appTerminate,
    };

    app.init(&app_data);
    while (app.isRunning(&app_data)) {
        app.update(&app_data);
    }
    app.terminate(&app_data);

    return ERR_CODE_SUCCESS;
}
