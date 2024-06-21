#include "wgpuex.h"
#include "assert.h"
#include "webgpu.h"

static void _requestAdapterCallback(WGPURequestAdapterStatus status,
                                    WGPUAdapter adapter, char const *message,
                                    void *userdata) {
    WGPURequestAdapterResponse *resp = userdata;
    resp->status = status;
    if (status == WGPURequestAdapterStatus_Success) {
        resp->adapter = adapter;
    }
}

static void _requestDeviceCallback(WGPURequestDeviceStatus status,
                                   WGPUDevice device, char const *message,
                                   void *userdata) {
    WGPURequestDeviceResponse *resp = userdata;
    resp->status = status;
    if (status == WGPURequestDeviceStatus_Success) {
        resp->device = device;
    }
}

WGPURequestAdapterResponse wgpuRequestAdapterSync(WGPUInstance instance, WGPURequestAdapterOptions* options) {
    WGPURequestAdapterResponse resp = {0};
    wgpuInstanceRequestAdapter(instance, options, _requestAdapterCallback,
                               &resp);
    // TODO: await with emiscripten

    assert(resp.adapter);
    return resp;
}

WGPURequestDeviceResponse wgpuRequestDeviceSync(WGPUAdapter adapter) {
    WGPURequestDeviceResponse resp = {0};

    wgpuAdapterRequestDevice(adapter, NULL, _requestDeviceCallback, &resp);
    // TODO: await with emiscripten

    assert(resp.device);
    return resp;
}
