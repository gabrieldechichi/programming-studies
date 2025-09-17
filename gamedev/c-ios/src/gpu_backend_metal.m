#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#include "gpu_backend.h"
#include <string.h>

// Internal structures
struct gpu_device {
    id<MTLDevice> device;
    id<MTLCommandQueue> command_queue;
    id<MTLLibrary> default_library;
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

struct gpu_pipeline {
    id<MTLRenderPipelineState> pipeline_state;
    id<MTLDepthStencilState> depth_stencil_state;
};

struct gpu_buffer {
    id<MTLBuffer> buffer;
    size_t size;
};

struct gpu_render_encoder {
    id<MTLRenderCommandEncoder> encoder;
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

    // Try to load shader library
    NSError* error = nil;
    device->default_library = nil;

    // Try multiple paths for the shader file
    NSArray* paths = @[@"triangle.metal",
                       @"src/shaders/triangle.metal",
                       @"../../src/shaders/triangle.metal"];

    for (NSString* path in paths) {
        if ([[NSFileManager defaultManager] fileExistsAtPath:path]) {
            NSString* shaderSource = [NSString stringWithContentsOfFile:path
                                                               encoding:NSUTF8StringEncoding
                                                                  error:&error];
            if (shaderSource) {
                device->default_library = [device->device newLibraryWithSource:shaderSource
                                                                       options:nil
                                                                         error:&error];
                if (device->default_library) {
                    NSLog(@"Loaded shader from: %@", path);
                    break;
                }
            }
        }
    }

    if (!device->default_library) {
        // Try to get default library
        device->default_library = [device->device newDefaultLibrary];
        if (!device->default_library) {
            NSLog(@"Warning: No Metal library found");
        }
    }

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

// === Rendering Implementation ===

gpu_pipeline_t* gpu_create_pipeline(
    gpu_device_t* device,
    const char* shader_source,
    const char* vertex_function,
    const char* fragment_function,
    gpu_vertex_layout_t* vertex_layout
) {
    gpu_pipeline_t* pipeline = (gpu_pipeline_t*)malloc(sizeof(gpu_pipeline_t));

    NSError* error = nil;
    id<MTLLibrary> library = device->default_library;

    // If shader source provided, compile it
    if (shader_source) {
        NSString* source = [NSString stringWithUTF8String:shader_source];
        library = [device->device newLibraryWithSource:source options:nil error:&error];
        if (!library) {
            NSLog(@"Failed to compile shader: %@", error);
            free(pipeline);
            return NULL;
        }
    }

    // Get shader functions
    id<MTLFunction> vertexFunc = [library newFunctionWithName:[NSString stringWithUTF8String:vertex_function]];
    id<MTLFunction> fragmentFunc = [library newFunctionWithName:[NSString stringWithUTF8String:fragment_function]];

    if (!vertexFunc) {
        NSLog(@"Failed to find vertex function: %s", vertex_function);
        // List available functions
        NSArray<NSString*>* functionNames = library.functionNames;
        NSLog(@"Available functions in library:");
        for (NSString* name in functionNames) {
            NSLog(@"  - %@", name);
        }
        free(pipeline);
        return NULL;
    }
    if (!fragmentFunc) {
        NSLog(@"Failed to find fragment function: %s", fragment_function);
        free(pipeline);
        return NULL;
    }
    NSLog(@"Found shader functions: %s, %s", vertex_function, fragment_function);

    // Create vertex descriptor
    MTLVertexDescriptor* vertexDesc = [[MTLVertexDescriptor alloc] init];

    for (int i = 0; i < vertex_layout->num_attributes; i++) {
        gpu_vertex_attr_t* attr = &vertex_layout->attributes[i];

        // Set attribute format
        MTLVertexFormat format;
        switch (attr->format) {
            case 0: format = MTLVertexFormatFloat2; break;
            case 1: format = MTLVertexFormatFloat3; break;
            case 2: format = MTLVertexFormatFloat4; break;
            default: format = MTLVertexFormatFloat4; break;
        }

        vertexDesc.attributes[attr->index].format = format;
        vertexDesc.attributes[attr->index].offset = attr->offset;
        vertexDesc.attributes[attr->index].bufferIndex = 0;
    }

    vertexDesc.layouts[0].stride = vertex_layout->stride;
    vertexDesc.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;

    // Create pipeline descriptor
    MTLRenderPipelineDescriptor* pipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineDesc.vertexFunction = vertexFunc;
    pipelineDesc.fragmentFunction = fragmentFunc;
    pipelineDesc.vertexDescriptor = vertexDesc;
    pipelineDesc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

    // Enable blending to ensure colors are written
    pipelineDesc.colorAttachments[0].blendingEnabled = YES;
    pipelineDesc.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
    pipelineDesc.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
    pipelineDesc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorOne;
    pipelineDesc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
    pipelineDesc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorZero;
    pipelineDesc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorZero;

    // Create pipeline state
    pipeline->pipeline_state = [device->device newRenderPipelineStateWithDescriptor:pipelineDesc error:&error];
    if (!pipeline->pipeline_state) {
        NSLog(@"Failed to create pipeline state: %@", error);
        free(pipeline);
        return NULL;
    }
    NSLog(@"Pipeline created successfully");

    // Create depth stencil state (no depth testing for now)
    MTLDepthStencilDescriptor* depthDesc = [[MTLDepthStencilDescriptor alloc] init];
    depthDesc.depthCompareFunction = MTLCompareFunctionAlways;
    depthDesc.depthWriteEnabled = NO;
    pipeline->depth_stencil_state = [device->device newDepthStencilStateWithDescriptor:depthDesc];

    return pipeline;
}

gpu_buffer_t* gpu_create_buffer(gpu_device_t* device, const void* data, size_t size) {
    gpu_buffer_t* buffer = (gpu_buffer_t*)malloc(sizeof(gpu_buffer_t));
    buffer->size = size;

    if (data) {
        buffer->buffer = [device->device newBufferWithBytes:data
                                                      length:size
                                                     options:MTLResourceStorageModeShared];
    } else {
        buffer->buffer = [device->device newBufferWithLength:size
                                                      options:MTLResourceStorageModeShared];
    }

    return buffer;
}

gpu_command_buffer_t* gpu_begin_commands(gpu_device_t* device) {
    gpu_command_buffer_t* cmd = (gpu_command_buffer_t*)malloc(sizeof(gpu_command_buffer_t));
    cmd->cmd_buffer = [device->command_queue commandBuffer];
    cmd->completed = false;
    return cmd;
}

gpu_render_encoder_t* gpu_begin_render_pass(
    gpu_command_buffer_t* cmd_buffer,
    gpu_texture_t* target,
    float clear_r, float clear_g, float clear_b, float clear_a
) {
    if (!cmd_buffer || !target || !target->texture) {
        NSLog(@"ERROR: Invalid parameters to gpu_begin_render_pass");
        return NULL;
    }

    gpu_render_encoder_t* encoder = (gpu_render_encoder_t*)malloc(sizeof(gpu_render_encoder_t));

    // Create render pass descriptor
    MTLRenderPassDescriptor* passDesc = [[MTLRenderPassDescriptor alloc] init];
    passDesc.colorAttachments[0].texture = target->texture;
    passDesc.colorAttachments[0].loadAction = MTLLoadActionClear;
    passDesc.colorAttachments[0].clearColor = MTLClearColorMake(clear_r, clear_g, clear_b, clear_a);
    passDesc.colorAttachments[0].storeAction = MTLStoreActionStore;

    // Keep the requested clear color for now (will be black)

    // Create render encoder
    encoder->encoder = [cmd_buffer->cmd_buffer renderCommandEncoderWithDescriptor:passDesc];
    if (!encoder->encoder) {
        NSLog(@"ERROR: Failed to create render command encoder");
        free(encoder);
        return NULL;
    }

    // Set viewport
    MTLViewport viewport = {
        .originX = 0.0,
        .originY = 0.0,
        .width = (double)target->width,
        .height = (double)target->height,
        .znear = 0.0,
        .zfar = 1.0
    };
    [encoder->encoder setViewport:viewport];

    return encoder;
}

void gpu_set_pipeline(gpu_render_encoder_t* encoder, gpu_pipeline_t* pipeline) {
    [encoder->encoder setRenderPipelineState:pipeline->pipeline_state];
    [encoder->encoder setDepthStencilState:pipeline->depth_stencil_state];
    [encoder->encoder setCullMode:MTLCullModeNone];  // Disable culling
    [encoder->encoder setFrontFacingWinding:MTLWindingCounterClockwise];
}

void gpu_set_vertex_buffer(gpu_render_encoder_t* encoder, gpu_buffer_t* buffer, int index) {
    if (!buffer || !buffer->buffer) {
        NSLog(@"ERROR: Invalid buffer in gpu_set_vertex_buffer");
        return;
    }
    [encoder->encoder setVertexBuffer:buffer->buffer offset:0 atIndex:index];
}

void gpu_set_uniforms(gpu_render_encoder_t* encoder, int index, const void* data, size_t size) {
    [encoder->encoder setVertexBytes:data length:size atIndex:index];
}

void gpu_draw(gpu_render_encoder_t* encoder, int vertex_count) {
    if (!encoder || !encoder->encoder) {
        NSLog(@"ERROR: Invalid encoder in gpu_draw");
        return;
    }
    [encoder->encoder drawPrimitives:MTLPrimitiveTypeTriangle
                          vertexStart:0
                          vertexCount:vertex_count];
}

void gpu_end_render_pass(gpu_render_encoder_t* encoder) {
    [encoder->encoder endEncoding];
    encoder->encoder = nil;
    free(encoder);
}

void gpu_commit_commands(gpu_command_buffer_t* cmd_buffer, bool wait) {
    // Add completion handler to know when GPU work is done
    __block bool* completed_ptr = &cmd_buffer->completed;
    [cmd_buffer->cmd_buffer addCompletedHandler:^(id<MTLCommandBuffer> buffer) {
        *completed_ptr = true;
    }];

    [cmd_buffer->cmd_buffer commit];
    if (wait) {
        [cmd_buffer->cmd_buffer waitUntilCompleted];
        cmd_buffer->completed = true;
    }
}

void gpu_wait_for_command_buffer(gpu_command_buffer_t* cmd_buffer) {
    if (!cmd_buffer || cmd_buffer->completed) {
        return;
    }

    // Wait for the command buffer to complete
    [cmd_buffer->cmd_buffer waitUntilCompleted];
    cmd_buffer->completed = true;
}

void gpu_destroy_pipeline(gpu_pipeline_t* pipeline) {
    if (pipeline) {
        pipeline->pipeline_state = nil;
        pipeline->depth_stencil_state = nil;
        free(pipeline);
    }
}

void gpu_destroy_buffer(gpu_buffer_t* buffer) {
    if (buffer) {
        buffer->buffer = nil;
        free(buffer);
    }
}

void gpu_destroy(gpu_device_t* device) {
    if (device) {
        // ARC will handle the release
        device->default_library = nil;
        device->command_queue = nil;
        device->device = nil;
        free(device);
    }
}
