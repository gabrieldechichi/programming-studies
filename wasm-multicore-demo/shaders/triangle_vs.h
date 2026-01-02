#pragma once

#ifdef WIN32
#include "triangle_vs_d3d11.h"
#define triangle_vs triangle_vs_d3d11
#define triangle_vs_len triangle_vs_d3d11_len
#else
#include "triangle_vs_webgpu.h"
#define triangle_vs triangle_vs_webgpu
#define triangle_vs_len triangle_vs_webgpu_len
#endif
