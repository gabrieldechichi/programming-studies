#include "imgui.h"
#include "platform.hpp"
#include <GLFW/glfw3.h>

int main() {
    auto platform = Platform::init();
    if (!platform) {
        return -1;
    }

    ImGui::StyleColorsDark();

    while (!platform->shouldClose()) {
        platform->beginFrame();

        ImGui::ShowDemoWindow();

        platform->render();
    }

    platform->destroy();

    return 0;
}
