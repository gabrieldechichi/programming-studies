#pragma once

#ifdef WIN32
#include "cube_instanced_vs_d3d11.h"
#define cube_instanced_vs cube_instanced_vs_d3d11
#define cube_instanced_vs_len cube_instanced_vs_d3d11_len
#else
#include "cube_instanced_vs_webgpu.h"
#define cube_instanced_vs cube_instanced_vs_webgpu
#define cube_instanced_vs_len cube_instanced_vs_webgpu_len
#endif
