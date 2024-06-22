#include "wgpuex.h"
#include "assert.h"
#include "lib.h"
#include "webgpu.h"
#include <stdio.h>

static void _requestAdapterCallback(WGPURequestAdapterStatus status,
                                    WGPUAdapter adapter, char const *message,
                                    void *userdata) {
    UNUSED(message);
    WGPURequestAdapterResponse *resp = userdata;
    resp->status = status;
    if (status == WGPURequestAdapterStatus_Success) {
        resp->adapter = adapter;
    }
}

static void _requestDeviceCallback(WGPURequestDeviceStatus status,
                                   WGPUDevice device, char const *message,
                                   void *userdata) {
    UNUSED(message);
    WGPURequestDeviceResponse *resp = userdata;
    resp->status = status;
    if (status == WGPURequestDeviceStatus_Success) {
        resp->device = device;
    }
}

WGPURequestAdapterResponse
wgpuRequestAdapterSync(WGPUInstance instance,
                       WGPURequestAdapterOptions *options) {
    WGPURequestAdapterResponse resp = {0};
    wgpuInstanceRequestAdapter(instance, options, _requestAdapterCallback,
                               &resp);
    // TODO: await with emiscripten

    assert(resp.adapter);
    return resp;
}

void defaultDeviceLostCallback(WGPUDeviceLostReason reason, char const *message,
                               void *userdata) {
    UNUSED(userdata);
    printf("Device Lost 0x(%X): %s\n", reason, message);
}

void defaultDeviceErrorCallback(WGPUErrorType type, char const *message,
                                void *userdata) {
    UNUSED(userdata);
    printf("Device Error 0x(%X): %s\n", type, message);
}

WGPURequestDeviceResponse
wgpuRequestDeviceSync(WGPUAdapter adapter,
                      WGPUDeviceDescriptor *deviceDescPtr) {

    WGPUDeviceDescriptor deviceDesc =
        deviceDescPtr != NULL
            ? *deviceDescPtr
            : (WGPUDeviceDescriptor){
                  .nextInChain = NULL,       //?
                  .label = "My Device",      // useful for debugging errors
                  .requiredFeatureCount = 0, // no feature limit
                  .requiredFeatures = NULL,  // array of required features
                  .requiredLimits = NULL,
                  .defaultQueue =
                      (WGPUQueueDescriptor){.label = "default queue",
                                            .nextInChain = NULL},
                  // likely useful in the future
                  .deviceLostCallback = NULL,
                  .deviceLostUserdata = NULL,
              };

    if (!deviceDesc.deviceLostCallback) {
        deviceDesc.deviceLostCallback = defaultDeviceLostCallback;
    }

    WGPURequestDeviceResponse resp = {0};

    wgpuAdapterRequestDevice(adapter, &deviceDesc, _requestDeviceCallback,
                             &resp);
    // TODO: await with emiscripten

    assert(resp.device);

    wgpuDeviceSetUncapturedErrorCallback(resp.device,
                                         defaultDeviceErrorCallback, NULL);
    return resp;
}
