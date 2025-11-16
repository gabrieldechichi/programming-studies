// Common libraries needed by both entrypoint and game

// These are only needed when building as separate executables
// In static build, they come from main.c
#ifdef HOT_RELOAD
#include "lib/common.c"
#include "lib/handle.c"
#include "lib/string.c"
#include "lib/string_builder.c"
#include "lib/profiler.c"
#include "os/hotreload.c"
#endif

#include "os/os.c"
#include "lib/memory.c"
#include "renderer/renderer.c"

#ifndef WASM
#include "ui/ui.c"
#include "entrypoint_sokol.c"
#endif
