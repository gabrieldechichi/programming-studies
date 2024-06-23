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

typedef struct {
    WGPUCompilationInfoRequestStatus status;
    size_t messageCount;
    WGPUCompilationMessage const *messages;

} WGPUShaderCompilationResponse;

WGPURequestAdapterResponse
wgpuRequestAdapterSync(WGPUInstance instance,
                       WGPURequestAdapterOptions *options);
WGPURequestDeviceResponse
wgpuRequestDeviceSync(WGPUAdapter adapter, WGPUDeviceDescriptor *deviceDesc);
WGPUShaderCompilationResponse
wgpuShaderCompilationInfoSync(WGPUShaderModule shaderModule);
WGPUBuffer createVertexBuffer(WGPUDevice device, const char *label,
                              int vertexLength, float *vertices);
WGPUBuffer createIndexBuffer16(WGPUDevice device, const char *label,
                               int indexLength, uint16_t *indices);

#endif
