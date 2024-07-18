#include "glm/fwd.hpp"
#include "imgui.h"
#include "platform.hpp"
#include "texture.hpp"
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <cstdint>
#include <cstdio>

struct Pixel {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;

    Pixel(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
        : r(r), g(g), b(b), a(a) {}
    Pixel() = default;

    explicit Pixel(const glm::vec4 &v)
        : r(static_cast<unsigned char>(v.r * 255.0f)),
          g(static_cast<unsigned char>(v.g * 255.0f)),
          b(static_cast<unsigned char>(v.b * 255.0f)),
          a(static_cast<unsigned char>(v.a * 255.0f)) {}
};

class Renderer {
  public:
    Renderer(uint32_t width, uint32_t height)
        : width(width), height(height), pixelLen(width * height) {
        pixels = new Pixel[pixelLen];
        tex = Texture::create(width, height);
    }

    ~Renderer() {
        tex.destroy();
        delete[] pixels;
    }

    uint32_t width, height;
    Pixel *pixels;
    int pixelLen;
    Texture tex;
    glm::vec4 frag(glm::vec2 uv);
    void render();
};

glm::vec4 Renderer::frag(glm::vec2 uv) { return glm::vec4(uv.r, uv.g, 0, 1); }

void Renderer::render() {
    for (int y = 0; y < width; y++) {
        for (int x = 0; x < height; x++) {
            float u = y / (float)width;
            float v = x / (float)height;
            pixels[x * width + y] = (Pixel)frag(glm::vec2(u, v));
        }
    }
    tex.setPixels((unsigned char *)pixels);
    tex.draw();
}

int main() {
    auto platform = Platform::init();
    if (!platform) {
        return -1;
    }

    ImGui::StyleColorsDark();

    int width;
    int height;
    glfwGetWindowSize(platform->window, &width, &height);

    auto renderer = Renderer(width, height);

    while (!platform->shouldClose()) {
        platform->beginFrame();

        if (glfwGetKey(platform->window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(platform->window, true);
        }

        // ImGui::Begin("Demo Window");
        // ImGui::Text("hello world");
        // ImGui::End();
        //
        // ImGui::Begin("Texture window");
        // ImGui::Image((void *)(intptr_t)tex.id, ImVec2(width, height));
        // ImGui::End();

        platform->beginRender();
        renderer.render();
        platform->endRender();

        platform->endFrame();
    }

    platform->destroy();

    return 0;
}
