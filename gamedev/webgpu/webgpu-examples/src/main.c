#include "GLFW/glfw3.h"
#include "lib.h"
#include "stb_ds.h"
#include "stdio.h"
#include "webgpu.h"
#include "wgpuex.h"

const unsigned int WIDTH = 800;
const unsigned int HEIGHT = 600;

typedef struct {
    WGPUInstance instance;
    WGPUAdapter adapter;
    WGPUDevice device;
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

int main(void) {
    if (!glfwInit()) {
        println("Failed to initialize GLFW");
        return ERR_CODE_FAIL;
    }

    AppData app_data = {0};

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow *window =
        glfwCreateWindow(WIDTH, HEIGHT, "WebgGPU example", NULL, NULL);
    if (!window) {
        println("Failed to create GLFW wnidow");
        glfwTerminate();

        return ERR_CODE_FAIL;
    }

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

    WGPUInstance instance = wgpuCreateInstance(&desc);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }

    wgpuInstanceRelease(app_data.wgpu.instance);
    wgpuAdapterRelease(app_data.wgpu.adapter);
    wgpuDeviceRelease(app_data.wgpu.device);

    glfwDestroyWindow(window);
    glfwTerminate();
    return ERR_CODE_SUCCESS;
}
