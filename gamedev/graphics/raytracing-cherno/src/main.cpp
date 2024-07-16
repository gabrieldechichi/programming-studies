#include "imgui.h"
#include "platform.hpp"
#include "random.hpp"
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <cstdio>

GLuint createTexture(int width, int height) {
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    {

        unsigned char *redPixels = new unsigned char[width * height * 4];
        for (int i = 0; i < width * height * 4; i += 4) {
            redPixels[i] = 0;       // Red channel
            redPixels[i + 1] = 255; // Green channel
            redPixels[i + 2] = 0;   // Blue channel
            redPixels[i + 3] = 255; // Alpha channel
        }

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, redPixels);

        delete[] redPixels;
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
}

void deleteTexture(GLuint *tex) { glDeleteTextures(1, tex); }

void updateTexture(GLuint texture, int width, int height,
                   unsigned char *pixels) {
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA,
                    GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void drawQuadTex(GLuint texture) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glBegin(GL_QUADS);

    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(-1.0f, -1.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(-1.0f, 1.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(1.0f, 1.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(1.0f, -1.0f);

    glEnd();
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
}

#define ARRAY_LEN(a) sizeof(a) / sizeof(a[0])

int main() {
    auto platform = Platform::init();
    if (!platform) {
        return -1;
    }

    ImGui::StyleColorsDark();

    int width;
    int height;
    glfwGetWindowSize(platform->window, &width, &height);
    auto tex = createTexture(width, height);

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
        updateTexture(tex, width, height, pixels);

        ImGui::ShowDemoWindow();

        platform->beginRender();

        drawQuadTex(tex);
        platform->endRender();

        platform->endFrame();
    }

    deleteTexture(&tex);

    platform->destroy();

    return 0;
}
