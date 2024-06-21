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

    app_data.wgpu.instance = wgpuCreateInstance(NULL);
    assert(app_data.wgpu.instance);
    app_data.wgpu.adapter =
        wgpuRequestAdapterSync(app_data.wgpu.instance, NULL).adapter;
    app_data.wgpu.device = wgpuRequestDeviceSync(app_data.wgpu.adapter).device;

    WGPUAdapterProperties adapterProperties = {0};
    wgpuAdapterGetProperties(app_data.wgpu.adapter, &adapterProperties);

    printf("Adapter properties:\n");
    printf(" - vendorID: %d\n", adapterProperties.vendorID);
    if (adapterProperties.vendorName) {
        printf(" - vendorName: %s\n", adapterProperties.vendorName);
    }
    if (adapterProperties.architecture) {
        printf(" - architecture: %s\n", adapterProperties.architecture);
    }
    printf(" - deviceID: %d\n", adapterProperties.deviceID);
    if (adapterProperties.name) {
        printf(" - name: %s\n", adapterProperties.name);
    }
    if (adapterProperties.driverDescription) {
        printf(" - driverDescription: %s\n",
               adapterProperties.driverDescription);
    }

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
