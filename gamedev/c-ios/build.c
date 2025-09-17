#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// Build configuration
#define CC "clang"
#define OUT_DIR "out"
#define MACOS_OUT_DIR "out/macos"
#define LINUX_OUT_DIR "out/linux"

// Source files
#define VIDEO_RENDERER_SRC "src/video_renderer.c"
#define GPU_BACKEND_METAL_SRC "src/gpu_backend_metal.m"
#define GPU_BACKEND_VULKAN_SRC "src/gpu_backend_vulkan.c"
#define PROFILER_SRC "src/profiler.c"

// Object files - macOS
#define MACOS_VIDEO_OBJ "out/macos/video_renderer.o"
#define MACOS_GPU_OBJ "out/macos/gpu_backend_metal.o"
#define MACOS_PROFILER_OBJ "out/macos/profiler.o"

// Object files - Linux
#define LINUX_VIDEO_OBJ "out/linux/video_renderer.o"
#define LINUX_GPU_OBJ "out/linux/gpu_backend_vulkan.o"
#define LINUX_PROFILER_OBJ "out/linux/profiler.o"

// Targets
#define MACOS_APP_TARGET "out/macos/video_renderer"
#define LINUX_APP_TARGET "out/linux/video_renderer"

// Common strict warning flags for main code
#define MAIN_STRICT_FLAGS                                                      \
  "-std=c11 "                                                                  \
  "-Wall -Wextra "                                                             \
  "-Wpedantic -Wcast-align -Wcast-qual "                                       \
  "-Wconversion -Wenum-compare -Wfloat-equal "                                 \
  "-Wredundant-decls -Wsign-conversion "                                       \
  "-Wstrict-prototypes -Wmissing-prototypes "                                  \
  "-Wold-style-definition -Wmissing-declarations "                             \
  "-Wformat=2 -Wformat-security "                                              \
  "-Wundef -Wshadow"

// Debug and release build flags
#define DEBUG_FLAGS "-g -O0 -DDEBUG"
#define RELEASE_FLAGS "-O2 -DNDEBUG"

// macOS configuration
#define MACOS_C_COMPILE_FLAGS                                                  \
  "-Isrc -DMACOS=1 " MAIN_STRICT_FLAGS
#define MACOS_OBJC_COMPILE_FLAGS                                               \
  "-x objective-c -fobjc-arc -Isrc -DMACOS=1 " MAIN_STRICT_FLAGS
#define MACOS_FRAMEWORKS                                                       \
  "-framework Cocoa -framework QuartzCore -framework Metal -framework "        \
  "MetalKit -framework Foundation -framework CoreGraphics"
// FFmpeg libraries for video encoding
#define MACOS_FFMPEG_FLAGS                                                     \
  "-I/opt/homebrew/include "                                                   \
  "-L/opt/homebrew/lib "                                                       \
  "-lavformat -lavcodec -lavutil -lswscale"

// Linux configuration
#define LINUX_COMPILE_FLAGS                                                    \
  "-Isrc -DLINUX=1 " MAIN_STRICT_FLAGS
#define LINUX_VULKAN_FLAGS                                                     \
  "-lvulkan -lm"
// FFmpeg libraries for Linux (standard package manager paths)
#define LINUX_FFMPEG_FLAGS                                                     \
  "-lavformat -lavcodec -lavutil -lswscale"

static int file_exists(const char *path) {
  struct stat st;
  return stat(path, &st) == 0;
}

static int create_dir(const char *path) {
  if (file_exists(path)) {
    return 0;
  }

  char cmd[256];
  snprintf(cmd, sizeof(cmd), "mkdir -p %s", path);
  return system(cmd);
}

static int build_macos(const char *build_type) {
  const char *build_flags =
      strcmp(build_type, "release") == 0 ? RELEASE_FLAGS : DEBUG_FLAGS;

  printf("Building macOS video renderer (%s)...\n", build_type);

  // Create build directory
  if (create_dir(MACOS_OUT_DIR) != 0) {
    fprintf(stderr, "Failed to create macOS build directory\n");
    return 1;
  }

  // Copy Metal shader to output directory
  char cmd[1024];
  snprintf(cmd, sizeof(cmd), "cp src/shaders/triangle.metal %s/", MACOS_OUT_DIR);
  system(cmd);

  // Compile video_renderer.c
  printf("Compiling video_renderer.c...\n");
  snprintf(cmd, sizeof(cmd), "%s %s %s %s -c %s -o %s",
           CC, MACOS_C_COMPILE_FLAGS, build_flags,
           MACOS_FFMPEG_FLAGS,
           VIDEO_RENDERER_SRC, MACOS_VIDEO_OBJ);

  if (system(cmd) != 0) {
    fprintf(stderr, "Failed to compile video_renderer.c\n");
    return 1;
  }

  // Compile gpu_backend_metal.m
  printf("Compiling gpu_backend_metal.m...\n");
  snprintf(cmd, sizeof(cmd), "%s %s %s -c %s -o %s",
           CC, MACOS_OBJC_COMPILE_FLAGS, build_flags,
           GPU_BACKEND_METAL_SRC, MACOS_GPU_OBJ);

  if (system(cmd) != 0) {
    fprintf(stderr, "Failed to compile gpu_backend_metal.m\n");
    return 1;
  }

  // Compile profiler.c
  printf("Compiling profiler.c...\n");
  snprintf(cmd, sizeof(cmd), "%s %s %s -c %s -o %s",
           CC, MACOS_C_COMPILE_FLAGS, build_flags,
           PROFILER_SRC, MACOS_PROFILER_OBJ);

  if (system(cmd) != 0) {
    fprintf(stderr, "Failed to compile profiler.c\n");
    return 1;
  }

  // Link everything together
  printf("Linking macOS application...\n");
  snprintf(cmd, sizeof(cmd), "%s %s %s %s -o %s %s %s",
           CC,
           MACOS_VIDEO_OBJ, MACOS_GPU_OBJ, MACOS_PROFILER_OBJ,
           MACOS_APP_TARGET,
           MACOS_FRAMEWORKS, MACOS_FFMPEG_FLAGS);

  if (system(cmd) != 0) {
    fprintf(stderr, "Failed to link macOS application\n");
    return 1;
  }

  printf("macOS build complete: %s\n", MACOS_APP_TARGET);
  printf("To run: cd %s && ./video_renderer\n", MACOS_OUT_DIR);
  return 0;
}

static int build_linux(const char *build_type) {
  const char *build_flags =
      strcmp(build_type, "release") == 0 ? RELEASE_FLAGS : DEBUG_FLAGS;

  printf("Building Linux video renderer with Vulkan (%s)...\n", build_type);

  // Create build directory
  if (create_dir(LINUX_OUT_DIR) != 0) {
    fprintf(stderr, "Failed to create Linux build directory\n");
    return 1;
  }

  // Compile GLSL shaders to SPIR-V
  char cmd[1024];
  printf("Compiling shaders to SPIR-V...\n");

  // Compile vertex shader
  snprintf(cmd, sizeof(cmd), "glslangValidator -V src/shaders/triangle.vert -o %s/triangle.vert.spv 2>/dev/null || glslc src/shaders/triangle.vert -o %s/triangle.vert.spv",
           LINUX_OUT_DIR, LINUX_OUT_DIR);
  if (system(cmd) != 0) {
    fprintf(stderr, "Failed to compile vertex shader. Make sure glslangValidator or glslc is installed.\n");
    fprintf(stderr, "Install with: sudo apt install glslang-tools or vulkan-sdk\n");
    return 1;
  }

  // Compile fragment shader
  snprintf(cmd, sizeof(cmd), "glslangValidator -V src/shaders/triangle.frag -o %s/triangle.frag.spv 2>/dev/null || glslc src/shaders/triangle.frag -o %s/triangle.frag.spv",
           LINUX_OUT_DIR, LINUX_OUT_DIR);
  if (system(cmd) != 0) {
    fprintf(stderr, "Failed to compile fragment shader\n");
    return 1;
  }

  // Compile video_renderer.c
  printf("Compiling video_renderer.c...\n");
  snprintf(cmd, sizeof(cmd), "%s %s %s -c %s -o %s",
           CC, LINUX_COMPILE_FLAGS, build_flags,
           VIDEO_RENDERER_SRC, LINUX_VIDEO_OBJ);

  if (system(cmd) != 0) {
    fprintf(stderr, "Failed to compile video_renderer.c\n");
    return 1;
  }

  // Compile gpu_backend_vulkan.c
  printf("Compiling gpu_backend_vulkan.c...\n");
  snprintf(cmd, sizeof(cmd), "%s %s %s -c %s -o %s",
           CC, LINUX_COMPILE_FLAGS, build_flags,
           GPU_BACKEND_VULKAN_SRC, LINUX_GPU_OBJ);

  if (system(cmd) != 0) {
    fprintf(stderr, "Failed to compile gpu_backend_vulkan.c\n");
    return 1;
  }

  // Compile profiler.c
  printf("Compiling profiler.c...\n");
  snprintf(cmd, sizeof(cmd), "%s %s %s -c %s -o %s",
           CC, LINUX_COMPILE_FLAGS, build_flags,
           PROFILER_SRC, LINUX_PROFILER_OBJ);

  if (system(cmd) != 0) {
    fprintf(stderr, "Failed to compile profiler.c\n");
    return 1;
  }

  // Link everything together
  printf("Linking Linux application...\n");
  snprintf(cmd, sizeof(cmd), "%s %s %s %s -o %s %s %s -lpthread",
           CC,
           LINUX_VIDEO_OBJ, LINUX_GPU_OBJ, LINUX_PROFILER_OBJ,
           LINUX_APP_TARGET,
           LINUX_VULKAN_FLAGS, LINUX_FFMPEG_FLAGS);

  if (system(cmd) != 0) {
    fprintf(stderr, "Failed to link Linux application\n");
    return 1;
  }

  printf("Linux build complete: %s\n", LINUX_APP_TARGET);
  printf("To run: cd %s && ./video_renderer\n", LINUX_OUT_DIR);
  return 0;
}

int main(int argc, char *argv[]) {
  // Parse build type (debug/release), default to debug
  const char *build_type = "debug";
  if (argc > 2) {
    if (strcmp(argv[2], "debug") == 0 || strcmp(argv[2], "release") == 0) {
      build_type = argv[2];
    } else {
      fprintf(stderr, "Unknown build type: %s\n", argv[2]);
      fprintf(stderr, "Build type must be 'debug' or 'release'\n");
      return 1;
    }
  }

  if (argc > 1) {
    if (strcmp(argv[1], "macos") == 0) {
      return build_macos(build_type);
    } else if (strcmp(argv[1], "linux") == 0) {
      return build_linux(build_type);
    } else {
      fprintf(stderr, "Unknown target: %s\n", argv[1]);
      fprintf(stderr, "Usage: %s [macos|linux] [debug|release]\n", argv[0]);
      fprintf(stderr, "Build type defaults to 'debug' if not specified\n");
      return 1;
    }
  }

  // Default to Linux debug on Linux systems
  return build_linux(build_type);
}
