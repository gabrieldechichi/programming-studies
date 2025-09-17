#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#include "gpu_backend.h"

// Internal structures
struct gpu_device {
    id<MTLDevice> device;
    id<MTLCommandQueue> command_queue;
};

struct gpu_texture {
    id<MTLTexture> texture;
    int width;
    int height;
};

struct gpu_readback_buffer {
    id<MTLBuffer> buffer;
    size_t size;
};

struct gpu_command_buffer {
    id<MTLCommandBuffer> cmd_buffer;
    bool completed;
};

gpu_device_t* gpu_init(void) {
    gpu_device_t* device = (gpu_device_t*)malloc(sizeof(gpu_device_t));

    // Create Metal device
    device->device = MTLCreateSystemDefaultDevice();
    if (!device->device) {
        free(device);
        return NULL;
    }

    // Create command queue
    device->command_queue = [device->device newCommandQueue];

    return device;
}

void* gpu_get_native_device(gpu_device_t* device) {
    return (__bridge void*)device->device;
}

gpu_texture_t* gpu_create_texture(gpu_device_t* device, int width, int height) {
    gpu_texture_t* texture = (gpu_texture_t*)malloc(sizeof(gpu_texture_t));
    texture->width = width;
    texture->height = height;

    // Create texture descriptor
    MTLTextureDescriptor *texDesc = [MTLTextureDescriptor
        texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
        width:width
        height:height
        mipmapped:NO];

    texDesc.storageMode = MTLStorageModePrivate;
    texDesc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;

    texture->texture = [device->device newTextureWithDescriptor:texDesc];

    return texture;
}

void* gpu_get_native_texture(gpu_texture_t* texture) {
    return (__bridge void*)texture->texture;
}

gpu_readback_buffer_t* gpu_create_readback_buffer(gpu_device_t* device, size_t size) {
    gpu_readback_buffer_t* buffer = (gpu_readback_buffer_t*)malloc(sizeof(gpu_readback_buffer_t));
    buffer->size = size;

    buffer->buffer = [device->device newBufferWithLength:size
                                     options:MTLResourceStorageModeShared];

    return buffer;
}

gpu_command_buffer_t* gpu_readback_texture_async(
    gpu_device_t* device,
    gpu_texture_t* texture,
    gpu_readback_buffer_t* buffer,
    int width,
    int height
) {
    gpu_command_buffer_t* cmd = (gpu_command_buffer_t*)malloc(sizeof(gpu_command_buffer_t));
    cmd->completed = false;

    // Create command buffer
    cmd->cmd_buffer = [device->command_queue commandBuffer];

    // Create blit encoder
    id<MTLBlitCommandEncoder> blitEncoder = [cmd->cmd_buffer blitCommandEncoder];

    // Setup copy operation
    [blitEncoder copyFromTexture:texture->texture
                     sourceSlice:0
                     sourceLevel:0
                    sourceOrigin:MTLOriginMake(0, 0, 0)
                      sourceSize:MTLSizeMake(width, height, 1)
                        toBuffer:buffer->buffer
               destinationOffset:0
          destinationBytesPerRow:width * 4
        destinationBytesPerImage:width * height * 4];

    [blitEncoder endEncoding];

    // Add completion handler to mark as complete
    [cmd->cmd_buffer addCompletedHandler:^(id<MTLCommandBuffer> cmdBuf) {
        (void)cmdBuf;
        cmd->completed = true;
    }];

    return cmd;
}

void gpu_submit_commands(gpu_command_buffer_t* cmd_buffer, bool wait) {
    [cmd_buffer->cmd_buffer commit];

    if (wait) {
        [cmd_buffer->cmd_buffer waitUntilCompleted];
        cmd_buffer->completed = true;
    }
}

bool gpu_is_readback_complete(gpu_command_buffer_t* cmd_buffer) {
    if (cmd_buffer->completed) return true;

    if (cmd_buffer->cmd_buffer.status == MTLCommandBufferStatusCompleted) {
        cmd_buffer->completed = true;
        return true;
    }
    return false;
}

void* gpu_get_readback_data(gpu_readback_buffer_t* buffer) {
    return buffer->buffer.contents;
}

void gpu_copy_readback_data(gpu_readback_buffer_t* buffer, void* dst, size_t size) {
    void* src = buffer->buffer.contents;
    memcpy(dst, src, size < buffer->size ? size : buffer->size);
}

void gpu_destroy_command_buffer(gpu_command_buffer_t* cmd_buffer) {
    if (cmd_buffer) {
        // ARC will handle the release
        cmd_buffer->cmd_buffer = nil;
        free(cmd_buffer);
    }
}

void gpu_destroy_texture(gpu_texture_t* texture) {
    if (texture) {
        // ARC will handle the release
        texture->texture = nil;
        free(texture);
    }
}

void gpu_destroy_readback_buffer(gpu_readback_buffer_t* buffer) {
    if (buffer) {
        // ARC will handle the release
        buffer->buffer = nil;
        free(buffer);
    }
}

void gpu_destroy(gpu_device_t* device) {
    if (device) {
        // ARC will handle the release
        device->command_queue = nil;
        device->device = nil;
        free(device);
    }
}