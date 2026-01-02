#pragma once

#ifdef WIN32
#include "cube_vs_d3d11.h"
#define cube_vs cube_vs_d3d11
#define cube_vs_len cube_vs_d3d11_len
#else
#include "cube_vs_webgpu.h"
#define cube_vs cube_vs_webgpu
#define cube_vs_len cube_vs_webgpu_len
#endif
