#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

// Build configuration
#define CC "clang"
#define OUT_DIR "out"
#define MACOS_OUT_DIR "out/macos"
#define VENDOR_SRC "src/vendor/vendor.c"
#define MAIN_SRC "src/main.c"
#define VENDOR_OBJ "out/macos/vendor.o"
#define APP_TARGET "out/macos/app"

// Compiler flags
#define COMPILE_FLAGS "-x objective-c -Isrc -Isrc/vendor"
#define LINK_FLAGS "-x objective-c -Isrc -Isrc/vendor"
#define LINK_RESET_FLAGS "-x none"
#define FRAMEWORKS "-framework Cocoa -framework QuartzCore -framework Metal -framework MetalKit"

int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

long file_mtime(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return st.st_mtime;
    }
    return 0;
}

int create_dir(const char *path) {
    if (file_exists(path)) {
        return 0;
    }
    
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s", path);
    return system(cmd);
}

int main() {
    printf("Building macOS target...\n");
    
    // Create build directory
    if (create_dir(MACOS_OUT_DIR) != 0) {
        fprintf(stderr, "Failed to create build directory\n");
        return 1;
    }
    
    // Check if vendor.o needs rebuilding
    const char *vendor_src = VENDOR_SRC;
    const char *vendor_obj = VENDOR_OBJ;
    
    int need_vendor = 0;
    if (!file_exists(vendor_obj)) {
        need_vendor = 1;
        printf("vendor.o doesn't exist, need to compile\n");
    } else if (file_mtime(vendor_src) > file_mtime(vendor_obj)) {
        need_vendor = 1;
        printf("vendor.c is newer than vendor.o, need to recompile\n");
    }
    
    if (need_vendor) {
        printf("Compiling vendor.c...\n");
        char cmd[512];
        snprintf(cmd, sizeof(cmd), 
            "%s %s -c %s -o %s",
            CC, COMPILE_FLAGS, vendor_src, vendor_obj);
        
        if (system(cmd) != 0) {
            fprintf(stderr, "Failed to compile vendor.c\n");
            return 1;
        }
    }
    
    // Check if main app needs rebuilding
    const char *main_src = MAIN_SRC;
    const char *app_target = APP_TARGET;
    
    int need_main = 0;
    if (!file_exists(app_target)) {
        need_main = 1;
        printf("app doesn't exist, need to build\n");
    } else if (file_mtime(main_src) > file_mtime(app_target) || 
               file_mtime(vendor_obj) > file_mtime(app_target)) {
        need_main = 1;
        printf("Source files are newer than app, need to rebuild\n");
    }
    
    if (need_main) {
        printf("Linking main application...\n");
        char cmd[1024];
        snprintf(cmd, sizeof(cmd),
            "%s %s %s %s %s -o %s %s",
            CC, LINK_FLAGS, main_src, LINK_RESET_FLAGS, vendor_obj, app_target, FRAMEWORKS);
        
        if (system(cmd) != 0) {
            fprintf(stderr, "Failed to link main application\n");
            return 1;
        }
    }
    
    printf("Build complete: %s\n", app_target);
    return 0;
}