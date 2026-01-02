#pragma once

#ifdef WIN32
#include "cube_fs_d3d11.h"
#define cube_fs cube_fs_d3d11
#define cube_fs_len cube_fs_d3d11_len
#else
#include "cube_fs_webgpu.h"
#define cube_fs cube_fs_webgpu
#define cube_fs_len cube_fs_webgpu_len
#endif
