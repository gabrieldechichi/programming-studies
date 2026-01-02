#pragma once

#ifdef WIN32
#include "blit_fs_d3d11.h"
#define blit_fs blit_fs_d3d11
#define blit_fs_len blit_fs_d3d11_len
#else
#include "blit_fs_webgpu.h"
#define blit_fs blit_fs_webgpu
#define blit_fs_len blit_fs_webgpu_len
#endif
