#pragma once

#ifdef WIN32
#include "fish_vs_d3d11.h"
#define fish_vs fish_vs_d3d11
#define fish_vs_len fish_vs_d3d11_len
#else
#include "fish_vs_webgpu.h"
#define fish_vs fish_vs_webgpu
#define fish_vs_len fish_vs_webgpu_len
#endif
