#include "GLFW/glfw3.h"
#include "glfw3webgpu.h"
#include "lib.h"
#include "lib/string.h"
#include "stb_ds.h"
#include "stdbool.h"
#include "stdio.h"
#include "webgpu.h"
#include "wgpuex.h"
#include "pipelines/default2d.h"
#include <stdint.h>
#include <string.h>

const unsigned int WIDTH = 800;
const unsigned int HEIGHT = 600;

typedef struct {
    WGPUDevice device;
    WGPULimits deviceLimits;
    WGPUQueue queue;
    WGPUSurface surface;
    shader_default2d_pipeline pipeline2d;
    WGPUTextureFormat textureFormat;
} WgpuState;

typedef struct {
    GLFWwindow *window;
    WgpuState wgpu;
} AppData;

typedef error_code_t (*AppInitCallback)(AppData *app_data);
typedef void (*AppUpdateCallback)(AppData *app_data);
typedef bool (*AppIsRunningCallback)(AppData *app_data);
typedef void (*AppTerminateCallback)(AppData *app_data);

typedef struct {
    AppInitCallback init;
    AppIsRunningCallback isRunning;
    AppUpdateCallback update;
    AppTerminateCallback terminate;
} App;

void printLimits(WGPULimits limits) {
    printf("\t- maxTextureDimension1D: %d\n", limits.maxTextureDimension1D);
    printf("\t- maxTextureDimension2D: %d\n", limits.maxTextureDimension2D);
    printf("\t- maxTextureDimension3D: %d\n", limits.maxTextureDimension3D);
    printf("\t- maxTextureArrayLayers: %d\n", limits.maxTextureArrayLayers);
    printf("\t- maxVertexBuffers: %d\n", limits.maxVertexBuffers);
    printf("\t- maxVertexAttributes: %d\n", limits.maxVertexAttributes);
    printf("\t- minUniformBufferOffsetAlignment: %d\n",
           limits.minUniformBufferOffsetAlignment);
}

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
    WGPUSupportedLimits limits = {0};
    if (wgpuAdapterGetLimits(adapter, &limits)) {
        printf("Adapter limits:\n");
        printLimits(limits.limits);
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
        printLimits(limits.limits);
    }
}

void wgpuQueueWorkDoneCallback(WGPUQueueWorkDoneStatus status, void *userdata) {
    UNUSED(userdata);
    printf("Queue %s work finished: 0x%X\n", "default queue", status);
}


error_code_t appInit(AppData *app_data) {
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize glfw\n");
        return ERR_CODE_FAIL;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); // Start the window minimized
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

    WGPUSupportedLimits adapterLimits = {0};
    if (!wgpuAdapterGetLimits(adapter, &adapterLimits)) {
        return ERR_CODE_FAIL;
    }

    // device
    // TODO: pass reasonable limits when requesting device
    WGPURequiredLimits requiredLimits[1] = {{.limits = adapterLimits.limits}};
    requiredLimits[0].limits.minUniformBufferOffsetAlignment = 32;
    WGPUDeviceDescriptor deviceDesc = {
        .requiredLimits = requiredLimits,
    };

    app_data->wgpu.device = wgpuRequestDeviceSync(adapter, &deviceDesc).device;
    inspectDevice(app_data->wgpu.device);

    WGPUSupportedLimits deviceSupportedLimits = {0};
    if (!wgpuDeviceGetLimits(app_data->wgpu.device, &deviceSupportedLimits)) {
        return ERR_CODE_FAIL;
    }
    app_data->wgpu.deviceLimits = deviceSupportedLimits.limits;

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

        shader_default2d_pipeline_result_t pipeline2dResult =
            shader_default2d_createPipeline(app_data->wgpu.device,
                                            app_data->wgpu.deviceLimits,
                                            app_data->wgpu.textureFormat);
        if (pipeline2dResult.error_code) {
            return pipeline2dResult.error_code;
        }
        app_data->wgpu.pipeline2d = pipeline2dResult.value;
    }

    glfwShowWindow(app_data->window);
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
            .clearValue = {0.5, 0.5, 0.5, 1}};

        WGPURenderPassDescriptor renderPassDesc = {.label = "Frame render pass",
                                                   .colorAttachmentCount = 1,
                                                   .colorAttachments =
                                                       &colorAttachment};

        WGPURenderPassEncoder passEncoder =
            wgpuCommandEncoderBeginRenderPass(cmdencoder, &renderPassDesc);

        shader_default2d_pipelineRender(app_data->wgpu.pipeline2d, passEncoder,
                                        app_data->wgpu.queue);

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
    shader_default2d_free(app_data->wgpu.pipeline2d);
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

    error_code_t err = app.init(&app_data);
    if (err) {
        return err;
    }

    while (app.isRunning(&app_data)) {
        app.update(&app_data);
    }
    app.terminate(&app_data);

    return ERR_CODE_SUCCESS;
}
