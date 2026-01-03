#pragma once

#ifdef WIN32
#include "depth_only_fs_d3d11.h"
#define depth_only_fs depth_only_fs_d3d11
#define depth_only_fs_len depth_only_fs_d3d11_len
#else
#include "depth_only_fs_webgpu.h"
#define depth_only_fs depth_only_fs_webgpu
#define depth_only_fs_len depth_only_fs_webgpu_len
#endif
