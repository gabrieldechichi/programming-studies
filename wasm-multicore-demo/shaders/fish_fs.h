#pragma once

#ifdef WIN32
#include "fish_fs_d3d11.h"
#define fish_fs fish_fs_d3d11
#define fish_fs_len fish_fs_d3d11_len
#else
#include "fish_fs_webgpu.h"
#define fish_fs fish_fs_webgpu
#define fish_fs_len fish_fs_webgpu_len
#endif
