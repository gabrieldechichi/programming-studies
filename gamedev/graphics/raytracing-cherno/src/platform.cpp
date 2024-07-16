#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "platform.hpp"

Platform_Glfw_OpenGL *Platform_Glfw_OpenGL::init() {
    // Initialize GLFW
    if (!glfwInit()) {
        return nullptr;
    }

    Platform_Glfw_OpenGL *platform = new Platform_Glfw_OpenGL();

    // Create a GLFW window
    platform->window =
        glfwCreateWindow(800, 600, "GLFW + ImGui", nullptr, nullptr);
    if (!platform->window) {
        glfwTerminate();
        return nullptr;
    }
    glfwMakeContextCurrent(platform->window);

    // Initialize ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;

    // Setup ImGui

    ImGui_ImplGlfw_InitForOpenGL(platform->window, true);
    ImGui_ImplOpenGL3_Init("#version 130");
    return platform;
}

void Platform_Glfw_OpenGL::beginFrame() {
    // Poll and handle events
    glfwPollEvents();

    // Start ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Platform_Glfw_OpenGL::render() {
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);
}

void Platform_Glfw_OpenGL::endFrame() {}

void Platform_Glfw_OpenGL::destroy() {

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
}

