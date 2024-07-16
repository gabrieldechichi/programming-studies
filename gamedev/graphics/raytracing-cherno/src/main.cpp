#include "imgui.h"
#include "platform.hpp"
#include "random.hpp"
#include "texture.hpp"
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <cstdio>

int main() {
    auto platform = Platform::init();
    if (!platform) {
        return -1;
    }

    ImGui::StyleColorsDark();

    int width;
    int height;
    glfwGetWindowSize(platform->window, &width, &height);
    auto tex = Texture::create(width, height);

    int len = width * height * 4;
    unsigned char *pixels = new unsigned char[len];

    while (!platform->shouldClose()) {
        platform->beginFrame();

        if (glfwGetKey(platform->window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(platform->window, true);
        }

        for (int i = 0; i < len; i += 4) {
            pixels[i] = Random::UInt(0, 255);
            pixels[i + 1] = Random::UInt(0, 255);
            pixels[i + 2] = Random::UInt(0, 255);
            pixels[i + 3] = 255;
        }
        tex.setPixels(pixels);

        ImGui::ShowDemoWindow();

        platform->beginRender();

        tex.draw();
        platform->endRender();

        platform->endFrame();
    }

    tex.destroy();
    delete[] pixels;

    platform->destroy();

    return 0;
}
