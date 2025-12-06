#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "os.h"
#include "lib/common.h"
#include "lib/string.h"

#if defined (BUILD_SYSTEM) || defined (TOOLING) || defined (TESTS)
  #if defined(_WIN32)
    #include "os/os_win32.c"
  #else
    #include "os/os_darwin_time.c"
    #include "os/os_macos.c"
  #endif
#elif defined(MACOS)
  #include "os/audio_sokol.c"
  #include "os/os_darwin_time.c"
  #include "os/os_macos.c"
#elif defined(IOS)
  #include "os/audio_sokol.c"
  #include "os/os_darwin_time.c"
  #include "os/os_ios.c"
#elif defined(WIN32) || defined(WIN64)
  #include "os/audio_sokol.c"
  // #include "os/time_sokol.c"
  #include "os/os_win32.c"
#elif defined(WASM)
  #include "os/os_wasm.c"
#endif
