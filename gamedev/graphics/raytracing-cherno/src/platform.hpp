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
    void render();
    void endFrame();
    void destroy();
};

typedef Platform_Glfw_OpenGL Platform;
