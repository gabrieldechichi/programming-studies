#include "glm/common.hpp"
#include "glm/fwd.hpp"
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

struct Light {
    glm::vec3 dir;
    glm::vec3 color;

    Light() = default;
    Light(glm::vec3 dir, glm::vec3 color)
        : dir(glm::normalize(dir)), color(color) {}
};

struct FrameData {
    glm::vec2 coord;
    Light light;
};

class Renderer {
  public:
    Renderer(uint32_t width, uint32_t height)
        : width(width), height(height), pixelLen(width * height) {
        pixels = new Pixel[pixelLen];
        tex = Texture::create(width, height);
        light = Light(glm::vec3(-1, -1, 1), glm::vec3(1));
    }

    ~Renderer() {
        tex.destroy();
        delete[] pixels;
    }

    uint32_t width, height;
    Pixel *pixels;
    int pixelLen;
    Texture tex;
    Light light;

    glm::vec4 frag(const FrameData &frame);
    void render();
};

glm::vec4 Renderer::frag(const FrameData &frame) {
    auto coord = frame.coord;
    auto light = frame.light;

    glm::vec3 rayOrigin(0.0f, 0.0f, -2.0f);
    glm::vec3 rayDirection(coord.x, coord.y, 1.0f);
    // rayDirection = glm::normalize(rayDirection);
    float radius = 0.5f;

    float a = glm::dot(rayDirection, rayDirection);
    float b = 2.0f * glm::dot(rayOrigin, rayDirection);
    float c = glm::dot(rayOrigin, rayOrigin) - radius * radius;

    float discriminant = b * b - 4.0f * a * c;
    if (discriminant < 0.0f) {
        return glm::vec4(0.2, 0.2, 0.2, 1);
    }

    float discriminantSqroot = std::sqrt(discriminant);
    // a is always positive so -b - discriminantSqroot is the closest t
    float t = (-b - discriminantSqroot) / (2 * a);

    auto hitPoint = rayOrigin + rayDirection * t;
    auto normal = glm::normalize(hitPoint);
    // normal = normal * 0.5f + 0.5f;
    auto diffuse = glm::max(0.0f, glm::dot(normal, -light.dir));
    auto color = glm::vec3(1, 0, 1);
    color *= diffuse;

    return glm::vec4(color, 1);
}

void Renderer::render() {
    FrameData frameData;
    frameData.light = light;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float u = x / (float)width;
            float v = y / (float)height;
            auto coord = glm::vec2(u, v) * 2.0f - 1.0f;
            coord.x *= width / (float)height;
            frameData.coord = coord;
            pixels[x + y * width] = (Pixel)frag(frameData);
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

    float lastTime = glfwGetTime();

    while (!platform->shouldClose()) {
        platform->beginFrame();

        if (glfwGetKey(platform->window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(platform->window, true);
        }

        float currentTime = glfwGetTime();
        float dt = currentTime - lastTime;
        lastTime = currentTime;

        {
            ImGui::SetNextWindowPos(ImVec2((float)width, (float)0),
                                    ImGuiCond_Always, ImVec2(1.0, 0.0));
            ImGui::SetNextWindowSize(ImVec2(250, height));

            ImGui::Begin("Property editor");
            ImGui::Text("Light direction");
            if (ImGui::DragFloat3("", &renderer.light.dir[0], 0.2f)) {
                renderer.light.dir = glm::normalize(renderer.light.dir);
            }
            ImGui::End();
        }
        // fps
        {
            ImGui::SetNextWindowPos(ImVec2((float)0, (float)0),
                                    ImGuiCond_Always);
            ImGui::Begin("Frame Time Info", NULL,
                         ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav);
            ImGui::Text("Frame Time: %.3f ms", dt * 1000);
            ImGui::End();
        }

        platform->beginRender();
        renderer.render();
        platform->endRender();

        platform->endFrame();
    }

    platform->destroy();

    return 0;
}
