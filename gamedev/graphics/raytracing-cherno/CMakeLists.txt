cmake_minimum_required(VERSION 3.10)

# Set to export compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(raytracing-cherno)

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

# GLM
include_directories(${CMAKE_SOURCE_DIR}/vendor/glm)

# Source files
file(GLOB SRC_FILES src/*.cpp)

# Executable
add_executable(raytracing-cherno ${SRC_FILES} ${IMGUI_SOURCES})

# Link Libraries
target_link_libraries(raytracing-cherno glfw ${GLFW_LIBRARIES})

# OpenGL

set(OpenGL_GL_PREFERENCE LEGACY)
find_package(OpenGL REQUIRED)
if (OPENGL_FOUND)
    target_include_directories(raytracing-cherno PUBLIC ${OPENGL_INCLUDE_DIR})
    target_link_libraries(raytracing-cherno ${OPENGL_gl_LIBRARY})
endif()
