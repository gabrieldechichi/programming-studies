cmake_minimum_required(VERSION 3.10)

# Set to export compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(ImGuiWithGLFW)

set(CMAKE_CXX_STANDARD 14)

# GLFW
add_subdirectory(vendor/glfw)
include_directories(vendor/glfw/include)

# ImGui
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/vendor/imgui)
file(GLOB IMGUI_SOURCES
    ${IMGUI_DIR}/*.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
include_directories(${IMGUI_DIR} ${IMGUI_DIR}/backends)

# Source files
file(GLOB SRC_FILES src/*.cpp)

# Executable
add_executable(ImGuiWithGLFW ${SRC_FILES} ${IMGUI_SOURCES})

# Link Libraries
target_link_libraries(ImGuiWithGLFW glfw ${GLFW_LIBRARIES})

# OpenGL
find_package(OpenGL REQUIRED)
if (OPENGL_FOUND)
    target_include_directories(ImGuiWithGLFW PUBLIC ${OPENGL_INCLUDE_DIR})
    target_link_libraries(ImGuiWithGLFW ${OPENGL_gl_LIBRARY})
endif()
