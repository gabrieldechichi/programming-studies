#pragma once

#include <GLFW/glfw3.h>

class Platform_Glfw_OpenGL {
  public:
    GLFWwindow *window;
    static Platform_Glfw_OpenGL *init();

    bool shouldClose() {
        return window == nullptr || glfwWindowShouldClose(window);
    }

    void beginFrame();
    void beginRender();
    void endRender();
    void endFrame();
    void destroy();
};

typedef Platform_Glfw_OpenGL Platform;

void checkGLError(const char *file, int line);
void clearGLErrors();

#define CHECK_GL_ERROR() checkGLError(__FILE__, __LINE__)
