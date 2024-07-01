#include "GLFW/glfw3.h"
#include "glfw3webgpu.h"
#include "lib.h"
#include "lib/string.h"
#include "pipelines/pipeline_default3d.h"
#include "renderers/renderer_basic3d.h"
#include "stb_ds.h"
#include "stdbool.h"
#include "stdio.h"
#include "webgpu.h"
#include "wgpuex.h"
#include <stdint.h>
#include <string.h>

const unsigned int WIDTH = 800;
const unsigned int HEIGHT = 600;

typedef struct {
    WGPUDevice device;
    WGPULimits deviceLimits;
    WGPUQueue queue;
    WGPUSurface surface;
    RendererBasic3D renderer;
    WGPUTextureFormat textureFormat;
} WgpuState;

typedef struct {
    GLFWwindow *window;
    WgpuState wgpu;
} AppData;

typedef ErrorCode (*AppInitCallback)(AppData *appData);
typedef void (*AppUpdateCallback)(AppData *appData);
typedef bool (*AppIsRunningCallback)(AppData *appData);
typedef void (*AppTerminateCallback)(AppData *appData);

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

    WGPUSupportedLimits limits = {0};
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

ErrorCode appInit(AppData *appData) {
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize glfw\n");
        return ERR_CODE_FAIL;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); // Start the window minimized
    appData->window =
        glfwCreateWindow(WIDTH, HEIGHT, "WebGPU examples", NULL, NULL);
    if (!appData->window) {
        fprintf(stderr, "Failed to create wgpu window\n");
        return ERR_CODE_FAIL;
    }

    //  instance
    WGPUInstance instance = wgpuCreateInstance(NULL);
    assert(instance);

    // surface
    appData->wgpu.surface = glfwGetWGPUSurface(instance, appData->window);

    // adapter
    WGPURequestAdapterOptions options = {
        .compatibleSurface = appData->wgpu.surface,
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

    appData->wgpu.device = wgpuRequestDeviceSync(adapter, &deviceDesc).device;
    inspectDevice(appData->wgpu.device);

    WGPUSupportedLimits deviceSupportedLimits = {0};
    if (!wgpuDeviceGetLimits(appData->wgpu.device, &deviceSupportedLimits)) {
        return ERR_CODE_FAIL;
    }
    appData->wgpu.deviceLimits = deviceSupportedLimits.limits;

    // configure surface
    {
        appData->wgpu.textureFormat =
            wgpuSurfaceGetPreferredFormat(appData->wgpu.surface, adapter);
        WGPUSurfaceConfiguration surfaceConf = {
            .nextInChain = NULL,
            .width = WIDTH,
            .height = HEIGHT,
            .device = appData->wgpu.device,
            .format = appData->wgpu.textureFormat,
            .usage = WGPUTextureUsage_RenderAttachment,
            .presentMode = WGPUPresentMode_Fifo,
            .alphaMode = WGPUCompositeAlphaMode_Auto,
        };
        wgpuSurfaceConfigure(appData->wgpu.surface, &surfaceConf);
    }

    // queue
    {
        appData->wgpu.queue = wgpuDeviceGetQueue(appData->wgpu.device);
        wgpuQueueOnSubmittedWorkDone(appData->wgpu.queue,
                                     wgpuQueueWorkDoneCallback, NULL);
    }

    wgpuInstanceRelease(instance);
    wgpuAdapterRelease(adapter);

    // pipeline
    {
        RendererBasic3DResult rendererResult = rendererBasic3dCreate(
            appData->wgpu.device, appData->wgpu.deviceLimits, appData->wgpu.textureFormat);
        if (rendererResult.errorCode) {
            return rendererResult.errorCode;
        }
        appData->wgpu.renderer = rendererResult.value;
    }

    glfwShowWindow(appData->window);
    return ERR_CODE_SUCCESS;
}

bool appIsRunning(AppData *appData) {
    return !glfwWindowShouldClose(appData->window);
}

// TODO: separate update and render
void appUpdate(AppData *appData) {
    glfwPollEvents();
    if (glfwGetKey(appData->window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(appData->window, GLFW_TRUE);
        return;
    }

    // START grab surface texture
    WGPUSurfaceTexture surfaceTex;
    wgpuSurfaceGetCurrentTexture(appData->wgpu.surface, &surfaceTex);
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
    WGPUCommandEncoderDescriptor cmdencoderDesc =
        (WGPUCommandEncoderDescriptor){.label = "My command encoder"};
    WGPUCommandEncoder cmdencoder =
        wgpuDeviceCreateCommandEncoder(appData->wgpu.device, &cmdencoderDesc);
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

        rendererBasic3dRender(appData->wgpu.renderer, passEncoder,
                              appData->wgpu.queue);

        wgpuRenderPassEncoderEnd(passEncoder);
        wgpuRenderPassEncoderRelease(passEncoder);
    }

    // command buffer commit
    WGPUCommandBufferDescriptor cmdBuffDesc = {.label = "Frame Command Buffer"};
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(cmdencoder, &cmdBuffDesc);
    wgpuCommandEncoderRelease(cmdencoder);

    // submit queue
    wgpuQueueSubmit(appData->wgpu.queue, 1, &cmd);
    wgpuCommandBufferRelease(cmd);

    wgpuSurfacePresent(appData->wgpu.surface);
    wgpuTextureViewRelease(targetView);
}

void appTerminate(AppData *appData) {
    rendererBasic3dFree(appData->wgpu.renderer);
    wgpuSurfaceUnconfigure(appData->wgpu.surface);
    wgpuSurfaceRelease(appData->wgpu.surface);
    wgpuQueueRelease(appData->wgpu.queue);
    wgpuDeviceRelease(appData->wgpu.device);

    glfwTerminate();
    glfwDestroyWindow(appData->window);
}

int main(void) {
    AppData appData = {0};
    App app = (App){
        .init = appInit,
        .isRunning = appIsRunning,
        .update = appUpdate,
        .terminate = appTerminate,
    };

    ErrorCode err = app.init(&appData);
    if (err) {
        return err;
    }

    while (app.isRunning(&appData)) {
        app.update(&appData);
    }
    app.terminate(&appData);

    return ERR_CODE_SUCCESS;
}
