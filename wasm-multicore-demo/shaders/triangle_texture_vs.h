#pragma once

#ifdef WIN32
#include "triangle_texture_vs_d3d11.h"
#define triangle_texture_vs triangle_texture_vs_d3d11
#define triangle_texture_vs_len triangle_texture_vs_d3d11_len
#else
#include "triangle_texture_vs_webgpu.h"
#define triangle_texture_vs triangle_texture_vs_webgpu
#define triangle_texture_vs_len triangle_texture_vs_webgpu_len
#endif
