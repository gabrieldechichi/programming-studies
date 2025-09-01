#ifndef XCODE_BUILD
// Only define SOKOL_IMPL when not building with Xcode
#define SOKOL_IMPL
#define SOKOL_METAL
#endif

#include "sokol/sokol_gfx.h"
#include "sokol/sokol_app.h"
#include "sokol/sokol_log.h"
#include "sokol/sokol_glue.h"
#include "sokol/sokol_gl.h"

#undef SOKOL_IMPL
#undef SOKOL_METAL
