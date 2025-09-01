// sokol_impl.m - Objective-C implementation file for Xcode builds
// This file compiles the sokol implementations for iOS/macOS

#define SOKOL_IMPL
#define SOKOL_METAL

// When building with Xcode, we need Objective-C compilation
#import "sokol_gfx.h"
#import "sokol_app.h"
#import "sokol_log.h"
#import "sokol_glue.h"
#import "sokol_gl.h"