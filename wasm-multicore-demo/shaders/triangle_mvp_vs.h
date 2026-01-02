#pragma once

#ifdef WIN32
#include "triangle_mvp_vs_d3d11.h"
#define triangle_mvp_vs triangle_mvp_vs_d3d11
#define triangle_mvp_vs_len triangle_mvp_vs_d3d11_len
#else
#include "triangle_mvp_vs_webgpu.h"
#define triangle_mvp_vs triangle_mvp_vs_webgpu
#define triangle_mvp_vs_len triangle_mvp_vs_webgpu_len
#endif
