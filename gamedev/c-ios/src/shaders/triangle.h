// Auto-generated platform-specific shader include
#pragma once

#if defined(MACOS)
#include "triangle_macos.h"
#elif defined(IOS)
#include "triangle_ios.h"
#elif defined(WIN64)
#include "triangle_windows.h"
#else
#error "Unsupported platform for shader: triangle"
#endif
