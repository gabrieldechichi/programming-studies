#pragma once

#ifdef WIN32
#include "triangle_transform_vs_d3d11.h"
#define triangle_transform_vs triangle_transform_vs_d3d11
#define triangle_transform_vs_len triangle_transform_vs_d3d11_len
#else
#include "triangle_transform_vs_webgpu.h"
#define triangle_transform_vs triangle_transform_vs_webgpu
#define triangle_transform_vs_len triangle_transform_vs_webgpu_len
#endif
