#ifndef GPU_BACKEND_H
#define GPU_BACKEND_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "sokol/sokol_gfx.h"

// Opaque handle types
typedef struct gpu_device gpu_device_t;
typedef struct gpu_texture gpu_texture_t;
typedef struct gpu_readback_buffer gpu_readback_buffer_t;
typedef struct gpu_command_buffer gpu_command_buffer_t;

// Initialize the GPU backend
gpu_device_t* gpu_init(void);

// Get the native device handle for Sokol initialization
void* gpu_get_native_device(gpu_device_t* device);

// Create a texture for rendering
gpu_texture_t* gpu_create_texture(gpu_device_t* device, int width, int height);

// Get native texture handle for Sokol wrapping
void* gpu_get_native_texture(gpu_texture_t* texture);

// Create a readback buffer for async GPU->CPU transfer
gpu_readback_buffer_t* gpu_create_readback_buffer(gpu_device_t* device, size_t size);

// Start async readback from texture to buffer
// Returns a command buffer that must be submitted
gpu_command_buffer_t* gpu_readback_texture_async(
    gpu_device_t* device,
    gpu_texture_t* texture,
    gpu_readback_buffer_t* buffer,
    int width,
    int height
);

// Submit command buffer and optionally wait for completion
void gpu_submit_commands(gpu_command_buffer_t* cmd_buffer, bool wait);

// Check if a readback operation is complete
bool gpu_is_readback_complete(gpu_command_buffer_t* cmd_buffer);

// Get the data from a readback buffer (only valid after readback is complete)
void* gpu_get_readback_data(gpu_readback_buffer_t* buffer);

// Copy readback buffer data to CPU memory
void gpu_copy_readback_data(gpu_readback_buffer_t* buffer, void* dst, size_t size);

// Cleanup functions
void gpu_destroy_command_buffer(gpu_command_buffer_t* cmd_buffer);
void gpu_destroy_texture(gpu_texture_t* texture);
void gpu_destroy_readback_buffer(gpu_readback_buffer_t* buffer);
void gpu_destroy(gpu_device_t* device);

#endif // GPU_BACKEND_H