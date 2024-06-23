#include "wgpuex.h"
#include "assert.h"
#include "lib.h"
#include "webgpu.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

void shaderCompilationInfoCallback(
    WGPUCompilationInfoRequestStatus status,
    struct WGPUCompilationInfo const *compilationInfo, void *userdata) {
    WGPUShaderCompilationResponse *response =
        (WGPUShaderCompilationResponse *)userdata;
    response->status = status;
    if (compilationInfo) {
        response->messageCount = compilationInfo->messageCount;
        response->messages = compilationInfo->messages;
    }
}

WGPUShaderCompilationResponse
wgpuShaderCompilationInfoSync(WGPUShaderModule shaderModule) {
    WGPUShaderCompilationResponse response = {0};
    wgpuShaderModuleGetCompilationInfo(
        shaderModule, shaderCompilationInfoCallback, &response);
    // TODO: emscripten wait

    for (size_t i = 0; i < response.messageCount; i++) {
        WGPUCompilationMessage msg = response.messages[i];

        printf("(0x%X): %s (%lu:%lu)\n", msg.type, msg.message, msg.lineNum,
               msg.linePos);
    }

    return response;
}

WGPUBuffer createVertexBuffer(WGPUDevice device, const char *label,
                              int vertexLength, float *vertices) {
    WGPUBufferDescriptor bufferDesc = {.label = label,
                                       .usage = WGPUBufferUsage_Vertex,
                                       .size = vertexLength * sizeof(float),
                                       .mappedAtCreation = true};

    WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &bufferDesc);
    float *elements =
        (float *)wgpuBufferGetMappedRange(buffer, 0, bufferDesc.size);
    memcpy(elements, vertices, bufferDesc.size);
    wgpuBufferUnmap(buffer);
    return buffer;
}

WGPUBuffer createIndexBuffer16(WGPUDevice device, const char *label,
                               int indexLength, uint16_t *indices) {
    size_t indexSize = indexLength * sizeof(uint16_t);
    WGPUBufferDescriptor bufferDesc = {
        .label = label,
        .usage = WGPUBufferUsage_Index,
        .size = alignTo(indexSize, WGPU_COPY_BUFFER_ALIGNMENT),
        .mappedAtCreation = true};

    WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &bufferDesc);
    uint16_t *elements =
        (uint16_t *)wgpuBufferGetMappedRange(buffer, 0, bufferDesc.size);
    memcpy(elements, indices, indexSize);
    wgpuBufferUnmap(buffer);
    return buffer;
}
