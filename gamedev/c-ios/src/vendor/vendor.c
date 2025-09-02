#ifndef XCODE_BUILD
// Only define SOKOL_IMPL when not building with Xcode
#define SOKOL_IMPL
#endif

// Platform-specific backend selection
#if defined(__APPLE__)
    #define SOKOL_METAL
#elif defined(_WIN32) || defined(_WIN64)
    #define SOKOL_D3D11
#elif defined(__linux__)
    #define SOKOL_GLCORE33
#endif

#include "sokol/sokol_gfx.h"
#include "sokol/sokol_app.h"
#include "sokol/sokol_log.h"
#include "sokol/sokol_glue.h"
#include "sokol/sokol_gl.h"

#undef SOKOL_IMPL
#ifdef SOKOL_METAL
#undef SOKOL_METAL
#endif
#ifdef SOKOL_D3D11
#undef SOKOL_D3D11
#endif
#ifdef SOKOL_GLCORE33
#undef SOKOL_GLCORE33
#endif
