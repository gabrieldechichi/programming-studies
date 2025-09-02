#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// Build configuration
#define CC "clang"
#define ZIG_CC "zig cc"
#define OUT_DIR "out"
#define MACOS_OUT_DIR "out/macos"
#define IOS_OUT_DIR "out/ios"
#define WINDOWS_OUT_DIR "out/windows"
#define VENDOR_SRC "src/vendor/vendor.c"
#define MAIN_SRC "src/main.c"

// macOS configuration
#define MACOS_VENDOR_OBJ "out/macos/vendor.o"
#define MACOS_APP_TARGET "out/macos/app"
#define MACOS_COMPILE_FLAGS "-x objective-c -Isrc -Isrc/vendor"
#define MACOS_LINK_FLAGS "-x objective-c -Isrc -Isrc/vendor"
#define MACOS_FRAMEWORKS                                                       \
  "-framework Cocoa -framework QuartzCore -framework Metal -framework "        \
  "MetalKit"

// iOS configuration
#define IOS_VENDOR_OBJ "out/ios/vendor.o"
#define IOS_APP_TARGET "out/ios/app-ios"
#define IOS_APP_BUNDLE "out/ios/ClearSapp.app"
#define IOS_COMPILE_FLAGS                                                      \
  "-x objective-c -miphoneos-version-min=12.0 -Isrc -Isrc/vendor"
#define IOS_LINK_FLAGS "-x objective-c -arch arm64 -Isrc -Isrc/vendor"
#define IOS_FRAMEWORKS                                                         \
  "-framework Foundation -framework UIKit -framework QuartzCore -framework "   \
  "Metal -framework MetalKit"
#define IOS_SDK "xcrun -sdk iphoneos"

// iOS signing configuration
#define BUNDLE_ID "com.example.clearsapp.Test"
#define SIGNING_IDENTITY                                                       \
  "Apple Development: gabriel.dechichi@portola.ai (8Y3X5XDMMD)"
#define PROVISIONING_PROFILE                                                   \
  "/Users/gabrieldechichi/Library/Developer/Xcode/UserData/Provisioning\\ "    \
  "Profiles/4d20f01c-5581-46d3-a2ad-7a07adcf0c84.mobileprovision"

// Windows configuration (using zig cc for cross-compilation)
#define WINDOWS_VENDOR_OBJ "out/windows/vendor.o"
#define WINDOWS_APP_TARGET "out/windows/app.exe"
#define WINDOWS_TARGET "x86_64-windows-gnu"
#define WINDOWS_COMPILE_FLAGS "-Isrc -Isrc/vendor -target " WINDOWS_TARGET
#define WINDOWS_LINK_FLAGS "-target " WINDOWS_TARGET
// Windows libraries needed by Sokol
#define WINDOWS_LIBS "-ld3d11 -ldxgi -lgdi32 -lole32 -lkernel32 -luser32 -lshell32"

// Common flags
#define LINK_RESET_FLAGS "-x none"

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


int build_macos() {
  printf("Building macOS target...\n");

  // Create build directory
  if (create_dir(MACOS_OUT_DIR) != 0) {
    fprintf(stderr, "Failed to create macOS build directory\n");
    return 1;
  }

  // Check if vendor.o needs rebuilding
  const char *vendor_src = VENDOR_SRC;
  const char *vendor_obj = MACOS_VENDOR_OBJ;

  int need_vendor = 0;
  if (!file_exists(vendor_obj)) {
    need_vendor = 1;
    printf("vendor.o doesn't exist, need to compile\n");
  } else if (file_mtime(vendor_src) > file_mtime(vendor_obj)) {
    need_vendor = 1;
    printf("vendor.c is newer than vendor.o, need to recompile\n");
  }

  if (need_vendor) {
    printf("Compiling vendor.c for macOS...\n");
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "%s %s -c %s -o %s", CC, MACOS_COMPILE_FLAGS,
             vendor_src, vendor_obj);

    if (system(cmd) != 0) {
      fprintf(stderr, "Failed to compile vendor.c\n");
      return 1;
    }
  }

  // Check if main app needs rebuilding
  const char *main_src = MAIN_SRC;
  const char *app_target = MACOS_APP_TARGET;

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
    printf("Linking macOS application...\n");
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "%s %s %s %s %s -o %s %s", CC, MACOS_LINK_FLAGS,
             main_src, LINK_RESET_FLAGS, vendor_obj, app_target,
             MACOS_FRAMEWORKS);

    if (system(cmd) != 0) {
      fprintf(stderr, "Failed to link macOS application\n");
      return 1;
    }
  }

  printf("macOS build complete: %s\n", app_target);
  return 0;
}

int deploy_ios();

int build_ios() {
  printf("Building iOS target...\n");

  // Create build directory
  if (create_dir(IOS_OUT_DIR) != 0) {
    fprintf(stderr, "Failed to create iOS build directory\n");
    return 1;
  }

  // Check if vendor.o needs rebuilding
  const char *vendor_src = VENDOR_SRC;
  const char *vendor_obj = IOS_VENDOR_OBJ;

  int need_vendor = 0;
  if (!file_exists(vendor_obj)) {
    need_vendor = 1;
    printf("iOS vendor.o doesn't exist, need to compile\n");
  } else if (file_mtime(vendor_src) > file_mtime(vendor_obj)) {
    need_vendor = 1;
    printf("vendor.c is newer than iOS vendor.o, need to recompile\n");
  }

  if (need_vendor) {
    printf("Compiling vendor.c for iOS...\n");
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "%s %s %s -arch arm64 -c %s -o %s", IOS_SDK, CC,
             IOS_COMPILE_FLAGS, vendor_src, vendor_obj);

    if (system(cmd) != 0) {
      fprintf(stderr, "Failed to compile vendor.c for iOS\n");
      return 1;
    }
  }

  // Check if main app needs rebuilding
  const char *main_src = MAIN_SRC;
  const char *app_target = IOS_APP_TARGET;

  int need_main = 0;
  if (!file_exists(app_target)) {
    need_main = 1;
    printf("iOS app doesn't exist, need to build\n");
  } else if (file_mtime(main_src) > file_mtime(app_target) ||
             file_mtime(vendor_obj) > file_mtime(app_target)) {
    need_main = 1;
    printf("Source files are newer than iOS app, need to rebuild\n");
  }

  if (need_main) {
    printf("Linking iOS application...\n");
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "%s %s %s %s %s %s -o %s %s", IOS_SDK, CC,
             IOS_LINK_FLAGS, main_src, LINK_RESET_FLAGS, vendor_obj, app_target,
             IOS_FRAMEWORKS);

    if (system(cmd) != 0) {
      fprintf(stderr, "Failed to link iOS application\n");
      return 1;
    }
  }

  // Create app bundle
  printf("Creating iOS app bundle...\n");
  char cmd[1024];

  // Remove existing bundle
  snprintf(cmd, sizeof(cmd), "rm -rf %s", IOS_APP_BUNDLE);
  system(cmd);

  // Create bundle directory
  snprintf(cmd, sizeof(cmd), "mkdir -p %s", IOS_APP_BUNDLE);
  if (system(cmd) != 0) {
    fprintf(stderr, "Failed to create app bundle directory\n");
    return 1;
  }

  // Copy executable
  snprintf(cmd, sizeof(cmd), "cp %s %s/app", app_target, IOS_APP_BUNDLE);
  if (system(cmd) != 0) {
    fprintf(stderr, "Failed to copy executable to bundle\n");
    return 1;
  }

  // Copy Info.plist
  snprintf(cmd, sizeof(cmd), "cp Info.plist %s/Info.plist", IOS_APP_BUNDLE);
  if (system(cmd) != 0) {
    fprintf(stderr, "Failed to copy Info.plist to bundle\n");
    return 1;
  }

  // Copy provisioning profile
  snprintf(cmd, sizeof(cmd), "cp %s %s/embedded.mobileprovision",
           PROVISIONING_PROFILE, IOS_APP_BUNDLE);
  if (system(cmd) != 0) {
    fprintf(stderr, "Failed to copy provisioning profile to bundle\n");
    return 1;
  }

  // Code sign
  printf("Code signing iOS app...\n");
  snprintf(
      cmd, sizeof(cmd),
      "codesign -s \"%s\" --timestamp -f --entitlements Entitlements.plist %s",
      SIGNING_IDENTITY, IOS_APP_BUNDLE);
  if (system(cmd) != 0) {
    fprintf(stderr, "Failed to code sign app bundle\n");
    return 1;
  }

  printf("iOS build complete: %s\n", IOS_APP_BUNDLE);
  return 0;
}

int build_windows() {
  printf("Building Windows target (cross-compilation with zig cc)...\n");
  
  // Check if zig is available
  if (system("which zig > /dev/null 2>&1") != 0) {
    fprintf(stderr, "❌ zig not found! Install it from https://ziglang.org/download/\n");
    return 1;
  }
  
  // Create build directory
  if (create_dir(WINDOWS_OUT_DIR) != 0) {
    fprintf(stderr, "Failed to create Windows build directory\n");
    return 1;
  }
  
  // Check if vendor.o needs rebuilding
  const char *vendor_src = VENDOR_SRC;
  const char *vendor_obj = WINDOWS_VENDOR_OBJ;
  
  int need_vendor = 0;
  if (!file_exists(vendor_obj)) {
    need_vendor = 1;
    printf("Windows vendor.o doesn't exist, need to compile\n");
  } else if (file_mtime(vendor_src) > file_mtime(vendor_obj)) {
    need_vendor = 1;
    printf("vendor.c is newer than Windows vendor.o, need to recompile\n");
  }
  
  if (need_vendor) {
    printf("Compiling vendor.c for Windows...\n");
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "%s %s -c %s -o %s",
             ZIG_CC, WINDOWS_COMPILE_FLAGS, vendor_src, vendor_obj);
    
    printf("Running: %s\n", cmd);
    if (system(cmd) != 0) {
      fprintf(stderr, "Failed to compile vendor.c for Windows\n");
      return 1;
    }
  }
  
  // Check if main app needs rebuilding
  const char *main_src = MAIN_SRC;
  const char *app_target = WINDOWS_APP_TARGET;
  
  int need_main = 0;
  if (!file_exists(app_target)) {
    need_main = 1;
    printf("Windows exe doesn't exist, need to build\n");
  } else if (file_mtime(main_src) > file_mtime(app_target) ||
             file_mtime(vendor_obj) > file_mtime(app_target)) {
    need_main = 1;
    printf("Source files are newer than Windows exe, need to rebuild\n");
  }
  
  if (need_main) {
    printf("Linking Windows application...\n");
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "%s %s -Isrc -Isrc/vendor %s %s -o %s %s",
             ZIG_CC, WINDOWS_LINK_FLAGS, main_src, vendor_obj, app_target, WINDOWS_LIBS);
    
    printf("Running: %s\n", cmd);
    if (system(cmd) != 0) {
      fprintf(stderr, "Failed to link Windows application\n");
      return 1;
    }
  }
  
  printf("Windows build complete: %s\n", app_target);
  printf("💡 Copy %s to a Windows machine to test\n", app_target);
  return 0;
}

int deploy_ios() {
  printf("🚀 iOS Device Deployment\n");

  // First build iOS
  if (build_ios() != 0) {
    fprintf(stderr, "Failed to build iOS app\n");
    return 1;
  }

  // List available devices
  printf("📱 Looking for connected iOS devices...\n");

  // Auto-select first connected iOS device
  char device_cmd[512];
  snprintf(device_cmd, sizeof(device_cmd),
           "xcrun devicectl list devices | grep -E '(iPhone|iPad)' | "
           "grep -v 'unavailable' | grep -E '(available|connected)' | "
           "head -1 | grep -o '[A-F0-9-]\\{36\\}'");

  FILE *fp = popen(device_cmd, "r");
  if (!fp) {
    fprintf(stderr, "Failed to list devices\n");
    return 1;
  }

  char device_id[128] = {0};
  if (fgets(device_id, sizeof(device_id), fp) == NULL) {
    pclose(fp);
    fprintf(stderr, "❌ No connected iOS devices found\n");
    fprintf(stderr, "💡 Make sure your device is:\n");
    fprintf(stderr, "   - Connected via USB\n");
    fprintf(stderr, "   - Unlocked and trusted this computer\n");
    fprintf(stderr, "   - In Developer Mode (iOS 16+)\n");
    return 1;
  }
  pclose(fp);

  // Remove newline
  device_id[strcspn(device_id, "\n")] = 0;

  printf("📲 Found device: %s\n", device_id);

  // Install on device
  printf("📲 Installing on device...\n");
  char install_cmd[512];
  snprintf(install_cmd, sizeof(install_cmd),
           "xcrun devicectl device install app --device %s %s", device_id,
           IOS_APP_BUNDLE);

  if (system(install_cmd) != 0) {
    fprintf(stderr, "Failed to install app on device\n");
    return 1;
  }

  printf("✅ iOS deployment complete!\n");
  return 0;
}

int main(int argc, char *argv[]) {
  if (argc > 1) {
    if (strcmp(argv[1], "ios") == 0) {
      return build_ios();
    } else if (strcmp(argv[1], "macos") == 0) {
      return build_macos();
    } else if (strcmp(argv[1], "windows") == 0) {
      return build_windows();
    } else if (strcmp(argv[1], "ios-deploy") == 0) {
      return deploy_ios();
    } else {
      fprintf(stderr, "Unknown target: %s\n", argv[1]);
      fprintf(stderr, "Usage: %s [macos|ios|windows|ios-deploy]\n", argv[0]);
      return 1;
    }
  }

  // Default to macOS
  return build_macos();
}