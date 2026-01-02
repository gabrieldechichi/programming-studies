#pragma once

#ifdef WIN32
#include "triangle_transform_fs_d3d11.h"
#define triangle_transform_fs triangle_transform_fs_d3d11
#define triangle_transform_fs_len triangle_transform_fs_d3d11_len
#else
#include "triangle_transform_fs_webgpu.h"
#define triangle_transform_fs triangle_transform_fs_webgpu
#define triangle_transform_fs_len triangle_transform_fs_webgpu_len
#endif
