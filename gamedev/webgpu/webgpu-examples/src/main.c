#include "lib.h"
#include "stb_ds.h"
#include "stdbool.h"
#include "stdio.h"
#include "webgpu.h"
#include "wgpu.h"
#include "wgpuex.h"

const unsigned int WIDTH = 800;
const unsigned int HEIGHT = 600;

typedef struct {
    WGPUInstance instance;
    WGPUAdapter adapter;
    WGPUDevice device;
    WGPUQueue queue;
} WgpuState;

typedef struct {
    WgpuState wgpu;
} AppData;

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

int main(void) {
    AppData app_data = {0};

    // instance
    {
        app_data.wgpu.instance = wgpuCreateInstance(NULL);
        assert(app_data.wgpu.instance);
    }

    // adapter
    {
        WGPURequestAdapterOptions options = (WGPURequestAdapterOptions){
            .powerPreference = WGPUPowerPreference_HighPerformance};

        app_data.wgpu.adapter =
            wgpuRequestAdapterSync(app_data.wgpu.instance, &options).adapter;

        inspectAdapter(app_data.wgpu.adapter);
    }

    // device
    {
        app_data.wgpu.device =
            wgpuRequestDeviceSync(app_data.wgpu.adapter, NULL).device;
        inspectDevice(app_data.wgpu.device);
    }

    // queue
    {
        app_data.wgpu.queue = wgpuDeviceGetQueue(app_data.wgpu.device);
        wgpuQueueOnSubmittedWorkDone(app_data.wgpu.queue,
                                     wgpuQueueWorkDoneCallback, NULL);
    }

    // command
    {
        WGPUCommandEncoderDescriptor cmdencoder_desc =
            (WGPUCommandEncoderDescriptor){.label = "My command encoder"};
        WGPUCommandEncoder cmdencoder = wgpuDeviceCreateCommandEncoder(
            app_data.wgpu.device, &cmdencoder_desc);
        wgpuCommandEncoderInsertDebugMarker(cmdencoder, "Do one thing");
        wgpuCommandEncoderInsertDebugMarker(cmdencoder, "Do another thing");

        WGPUCommandBufferDescriptor cmdbuff_desc =
            (WGPUCommandBufferDescriptor){.label = "My command buffer"};

        WGPUCommandBuffer cmdbuff =
            wgpuCommandEncoderFinish(cmdencoder, &cmdbuff_desc);

        printf("Submitting command\n");
        wgpuQueueSubmit(app_data.wgpu.queue, 1, &cmdbuff);
        wgpuCommandBufferRelease(cmdbuff);
        wgpuCommandEncoderRelease(cmdencoder);

        while (true) {
            printf("Waiting for queue...\n");
            bool queue_empty = wgpuDevicePoll(app_data.wgpu.device, true, NULL);
            if (queue_empty) {
                break;
            }
        }
        printf("Finish submit\n");
    }

    wgpuInstanceRelease(app_data.wgpu.instance);
    wgpuAdapterRelease(app_data.wgpu.adapter);
    wgpuDeviceRelease(app_data.wgpu.device);
    wgpuQueueRelease(app_data.wgpu.queue);

    return ERR_CODE_SUCCESS;
}
