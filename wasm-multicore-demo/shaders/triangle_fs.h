#pragma once

#ifdef WIN32
#include "triangle_fs_d3d11.h"
#define triangle_fs triangle_fs_d3d11
#define triangle_fs_len triangle_fs_d3d11_len
#else
#include "triangle_fs_webgpu.h"
#define triangle_fs triangle_fs_webgpu
#define triangle_fs_len triangle_fs_webgpu_len
#endif
