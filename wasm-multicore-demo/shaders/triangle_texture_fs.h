#pragma once

#ifdef WIN32
#include "triangle_texture_fs_d3d11.h"
#define triangle_texture_fs triangle_texture_fs_d3d11
#define triangle_texture_fs_len triangle_texture_fs_d3d11_len
#else
#include "triangle_texture_fs_webgpu.h"
#define triangle_texture_fs triangle_texture_fs_webgpu
#define triangle_texture_fs_len triangle_texture_fs_webgpu_len
#endif
