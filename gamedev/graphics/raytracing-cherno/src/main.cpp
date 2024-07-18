#include "glm/common.hpp"
#include "glm/geometric.hpp"
#include "glm/glm.hpp"
#include "imgui.h"
#include "platform.hpp"
#include "texture.hpp"
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <cmath>
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
        : r(static_cast<unsigned char>(std::fmax(0, v.r * 255.0f))),
          g(static_cast<unsigned char>(std::fmax(0, v.g * 255.0f))),
          b(static_cast<unsigned char>(std::fmax(0, v.b * 255.0f))),
          a(static_cast<unsigned char>(std::fmax(0, v.a * 255.0f))) {}
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

glm::vec4 Renderer::frag(glm::vec2 coord) {
    glm::vec3 lightDir(-1,-1,1);
    lightDir = glm::normalize(lightDir);

    glm::vec3 rayOrigin(0.0f, 0.0f, -2.0f);
    glm::vec3 rayDirection(coord.x, coord.y, 1.0f);
    //rayDirection = glm::normalize(rayDirection);
    float radius = 0.5f;

    float a = glm::dot(rayDirection, rayDirection);
    float b = 2.0f * glm::dot(rayOrigin, rayDirection);
    float c = glm::dot(rayOrigin, rayOrigin) - radius * radius;

    float discriminant = b * b - 4.0f * a * c;
    if (discriminant < 0.0f) {
        return glm::vec4(0.2, 0.2, 0.2, 1);
    }

    float discriminantSqroot = std::sqrt(discriminant);
    float t1 = (-b + discriminantSqroot) / (2 * a);
    float t2 = (-b - discriminantSqroot) / (2 * a);

    float t = t1 < t2 ? t1 : t2;

    auto hitPoint = rayOrigin + rayDirection * t;
    auto normal = glm::normalize(hitPoint);
    // normal = normal * 0.5f + 0.5f;
    auto diffuse = glm::dot(normal, -lightDir);
    auto color = glm::vec3(1,1,1);
    color *= diffuse;

    return glm::vec4(color, 1);
}

void Renderer::render() {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float u = x / (float)width;
            float v = y / (float)height;
            auto coord = glm::vec2(u, v) * 2.0f - 1.0f;
            coord.x *= width / (float)height;
            pixels[x + y * width] = (Pixel)frag(coord);
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
