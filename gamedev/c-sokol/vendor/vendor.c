//------------------------------------------------------------------------------
//  vendor.c - Sokol library implementations
//------------------------------------------------------------------------------
#define SOKOL_IMPL

#if defined(__APPLE__)
    #define SOKOL_METAL
#elif defined(_WIN32)
    #define SOKOL_D3D11
#else
    #define SOKOL_GLCORE
#endif

#include "sokol/sokol_app.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_log.h"
#include "sokol/sokol_glue.h"