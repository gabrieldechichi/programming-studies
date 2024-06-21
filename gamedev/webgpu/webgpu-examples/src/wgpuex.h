#ifndef WGPUEX_H
#define WGPUEX_H

#include "webgpu.h"

typedef struct {
    WGPUAdapter adapter;
    WGPURequestAdapterStatus status;
} WGPURequestAdapterResponse;

typedef struct {
    WGPUDevice device;
    WGPURequestDeviceStatus status;
} WGPURequestDeviceResponse;

WGPURequestAdapterResponse wgpuRequestAdapterSync(WGPUInstance instance, WGPURequestAdapterOptions* options);
WGPURequestDeviceResponse wgpuRequestDeviceSync(WGPUAdapter adapter);

#endif
