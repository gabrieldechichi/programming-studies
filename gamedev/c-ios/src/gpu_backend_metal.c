#include "gpu_backend.h"
#include <stdlib.h>
#include <string.h>
#include <Block.h>

// Metal and Foundation headers
#include <objc/objc.h>
#include <objc/runtime.h>
#include <objc/message.h>
#include <Metal/Metal.h>

// Metal forward declarations
typedef struct objc_object *id;
typedef struct objc_selector *SEL;
#ifndef nil
#define nil ((id)0)
#endif

// Metal selectors we'll use
static SEL sel_alloc;
static SEL sel_init;
static SEL sel_release;
static SEL sel_retain;
static SEL sel_newCommandQueue;
static SEL sel_commandBuffer;
static SEL sel_commit;
static SEL sel_waitUntilCompleted;
static SEL sel_blitCommandEncoder;
static SEL sel_endEncoding;
static SEL sel_texture2DDescriptorWithPixelFormat_width_height_mipmapped;
static SEL sel_newTextureWithDescriptor;
static SEL sel_setStorageMode;
static SEL sel_setUsage;
static SEL sel_newBufferWithLength_options;
static SEL sel_contents;
static SEL sel_copyFromTexture_sourceSlice_sourceLevel_sourceOrigin_sourceSize_toBuffer_destinationOffset_destinationBytesPerRow_destinationBytesPerImage;
static SEL sel_addCompletedHandler;
static SEL sel_status;

// Metal classes
static Class MTLTextureDescriptor_class;

// Metal constants
#define MTLPixelFormatBGRA8Unorm 80
#define MTLTextureUsageRenderTarget 0x0004
#define MTLTextureUsageShaderRead 0x0001
#define MTLStorageModePrivate 2
#define MTLStorageModeShared 1
#define MTLResourceStorageModeShared 1
#define MTLCommandBufferStatusCompleted 4

// Internal structures
struct gpu_device {
    id device;
    id command_queue;
};

struct gpu_texture {
    id texture;
    int width;
    int height;
};

struct gpu_readback_buffer {
    id buffer;
    size_t size;
};

struct gpu_command_buffer {
    id cmd_buffer;
    bool completed;
};

// Completion handler for command buffer
typedef void (^CommandBufferHandler)(id);

// Helper to call ObjC methods without arguments
static id msgSend0(id obj, SEL sel) {
    return ((id (*)(id, SEL))objc_msgSend)(obj, sel);
}

// Helper to call ObjC methods with one argument
static id msgSend1(id obj, SEL sel, id arg) {
    return ((id (*)(id, SEL, id))objc_msgSend)(obj, sel, arg);
}

// Helper to call ObjC methods with integer argument
static void msgSendInt(id obj, SEL sel, int arg) {
    ((void (*)(id, SEL, int))objc_msgSend)(obj, sel, arg);
}

// Initialize selectors (called once)
static void init_selectors(void) {
    static bool initialized = false;
    if (initialized) return;

    sel_alloc = sel_registerName("alloc");
    sel_init = sel_registerName("init");
    sel_release = sel_registerName("release");
    sel_retain = sel_registerName("retain");
    sel_newCommandQueue = sel_registerName("newCommandQueue");
    sel_commandBuffer = sel_registerName("commandBuffer");
    sel_commit = sel_registerName("commit");
    sel_waitUntilCompleted = sel_registerName("waitUntilCompleted");
    sel_blitCommandEncoder = sel_registerName("blitCommandEncoder");
    sel_endEncoding = sel_registerName("endEncoding");
    sel_texture2DDescriptorWithPixelFormat_width_height_mipmapped =
        sel_registerName("texture2DDescriptorWithPixelFormat:width:height:mipmapped:");
    sel_newTextureWithDescriptor = sel_registerName("newTextureWithDescriptor:");
    sel_setStorageMode = sel_registerName("setStorageMode:");
    sel_setUsage = sel_registerName("setUsage:");
    sel_newBufferWithLength_options = sel_registerName("newBufferWithLength:options:");
    sel_contents = sel_registerName("contents");
    sel_copyFromTexture_sourceSlice_sourceLevel_sourceOrigin_sourceSize_toBuffer_destinationOffset_destinationBytesPerRow_destinationBytesPerImage =
        sel_registerName("copyFromTexture:sourceSlice:sourceLevel:sourceOrigin:sourceSize:toBuffer:destinationOffset:destinationBytesPerRow:destinationBytesPerImage:");
    sel_addCompletedHandler = sel_registerName("addCompletedHandler:");
    sel_status = sel_registerName("status");

    MTLTextureDescriptor_class = objc_getClass("MTLTextureDescriptor");

    initialized = true;
}

gpu_device_t* gpu_init(void) {
    init_selectors();

    gpu_device_t* device = (gpu_device_t*)malloc(sizeof(gpu_device_t));

    // Create Metal device
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-function-type-mismatch"
    device->device = ((id (*)(void))MTLCreateSystemDefaultDevice)();
#pragma clang diagnostic pop

    if (!device->device) {
        free(device);
        return NULL;
    }
    msgSend0(device->device, sel_retain);

    // Create command queue
    device->command_queue = msgSend0(device->device, sel_newCommandQueue);
    msgSend0(device->command_queue, sel_retain);

    return device;
}

void* gpu_get_native_device(gpu_device_t* device) {
    return device->device;
}

gpu_texture_t* gpu_create_texture(gpu_device_t* device, int width, int height) {
    gpu_texture_t* texture = (gpu_texture_t*)malloc(sizeof(gpu_texture_t));
    texture->width = width;
    texture->height = height;

    // Create texture descriptor
    id tex_desc = ((id (*)(id, SEL, int, int, int, BOOL))objc_msgSend)(
        MTLTextureDescriptor_class,
        sel_texture2DDescriptorWithPixelFormat_width_height_mipmapped,
        MTLPixelFormatBGRA8Unorm, width, height, NO);

    msgSendInt(tex_desc, sel_setStorageMode, MTLStorageModePrivate);
    msgSendInt(tex_desc, sel_setUsage, MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead);

    texture->texture = msgSend1(device->device, sel_newTextureWithDescriptor, tex_desc);
    msgSend0(texture->texture, sel_retain);

    return texture;
}

void* gpu_get_native_texture(gpu_texture_t* texture) {
    return texture->texture;
}

gpu_readback_buffer_t* gpu_create_readback_buffer(gpu_device_t* device, size_t size) {
    gpu_readback_buffer_t* buffer = (gpu_readback_buffer_t*)malloc(sizeof(gpu_readback_buffer_t));
    buffer->size = size;

    buffer->buffer = ((id (*)(id, SEL, size_t, int))objc_msgSend)(
        device->device, sel_newBufferWithLength_options, size, MTLResourceStorageModeShared);
    msgSend0(buffer->buffer, sel_retain);

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
    cmd->cmd_buffer = msgSend0(device->command_queue, sel_commandBuffer);
    msgSend0(cmd->cmd_buffer, sel_retain);

    // Create blit encoder
    id blit_encoder = msgSend0(cmd->cmd_buffer, sel_blitCommandEncoder);

    // Setup copy operation
    typedef struct { NSUInteger x, y, z; } MyMTLOrigin;
    typedef struct { NSUInteger width, height, depth; } MyMTLSize;

    MyMTLOrigin origin = {0, 0, 0};
    MyMTLSize size = {width, height, 1};

    ((void (*)(id, SEL, id, NSUInteger, NSUInteger, MyMTLOrigin, MyMTLSize, id,
               NSUInteger, NSUInteger, NSUInteger))objc_msgSend)(
        blit_encoder,
        sel_copyFromTexture_sourceSlice_sourceLevel_sourceOrigin_sourceSize_toBuffer_destinationOffset_destinationBytesPerRow_destinationBytesPerImage,
        texture->texture, 0, 0, origin, size,
        buffer->buffer, 0, width * 4, width * height * 4);

    msgSend0(blit_encoder, sel_endEncoding);

    // Add completion handler to mark as complete
    CommandBufferHandler handler = Block_copy(^(id cmd_buf) {
        (void)cmd_buf;
        cmd->completed = true;
    });

    ((void (*)(id, SEL, CommandBufferHandler))objc_msgSend)(
        cmd->cmd_buffer, sel_addCompletedHandler, handler);
    Block_release(handler);

    return cmd;
}

void gpu_submit_commands(gpu_command_buffer_t* cmd_buffer, bool wait) {
    msgSend0(cmd_buffer->cmd_buffer, sel_commit);

    if (wait) {
        msgSend0(cmd_buffer->cmd_buffer, sel_waitUntilCompleted);
        cmd_buffer->completed = true;
    }
}

bool gpu_is_readback_complete(gpu_command_buffer_t* cmd_buffer) {
    if (cmd_buffer->completed) return true;

    int status = ((int (*)(id, SEL))objc_msgSend)(cmd_buffer->cmd_buffer, sel_status);
    if (status == MTLCommandBufferStatusCompleted) {
        cmd_buffer->completed = true;
        return true;
    }
    return false;
}

void* gpu_get_readback_data(gpu_readback_buffer_t* buffer) {
    return msgSend0(buffer->buffer, sel_contents);
}

void gpu_copy_readback_data(gpu_readback_buffer_t* buffer, void* dst, size_t size) {
    void* src = msgSend0(buffer->buffer, sel_contents);
    memcpy(dst, src, size < buffer->size ? size : buffer->size);
}

void gpu_destroy_command_buffer(gpu_command_buffer_t* cmd_buffer) {
    if (cmd_buffer) {
        msgSend0(cmd_buffer->cmd_buffer, sel_release);
        free(cmd_buffer);
    }
}

void gpu_destroy_texture(gpu_texture_t* texture) {
    if (texture) {
        msgSend0(texture->texture, sel_release);
        free(texture);
    }
}

void gpu_destroy_readback_buffer(gpu_readback_buffer_t* buffer) {
    if (buffer) {
        msgSend0(buffer->buffer, sel_release);
        free(buffer);
    }
}

void gpu_destroy(gpu_device_t* device) {
    if (device) {
        msgSend0(device->command_queue, sel_release);
        msgSend0(device->device, sel_release);
        free(device);
    }
}