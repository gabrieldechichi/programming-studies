#pragma once

#ifdef WIN32
#include "blit_vs_d3d11.h"
#define blit_vs blit_vs_d3d11
#define blit_vs_len blit_vs_d3d11_len
#else
#include "blit_vs_webgpu.h"
#define blit_vs blit_vs_webgpu
#define blit_vs_len blit_vs_webgpu_len
#endif
