#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Metal and Foundation headers
#include <objc/objc.h>
#include <objc/runtime.h>
#include <objc/message.h>
#include <CoreGraphics/CoreGraphics.h>
#include <Metal/Metal.h>

// Sokol headers
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_log.h"
#include "shaders/triangle.h"

// Metal forward declarations
typedef struct objc_object* id;
typedef struct objc_selector* SEL;
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
static SEL sel_renderCommandEncoderWithDescriptor;
static SEL sel_blitCommandEncoder;
static SEL sel_endEncoding;
static SEL sel_texture2DDescriptorWithPixelFormat_width_height_mipmapped;
static SEL sel_newTextureWithDescriptor;
static SEL sel_setStorageMode;
static SEL sel_setUsage;
static SEL sel_newBufferWithLength_options;
static SEL sel_contents;
static SEL sel_copyFromTexture_sourceSlice_sourceLevel_sourceOrigin_sourceSize_toBuffer_destinationOffset_destinationBytesPerRow_destinationBytesPerImage;
static SEL sel_colorAttachments;
static SEL sel_objectAtIndexedSubscript;
static SEL sel_setTexture;
static SEL sel_setLoadAction;
static SEL sel_setStoreAction;
static SEL sel_setClearColor;

// Metal classes
static Class MTLCreateSystemDefaultDevice_class;
static Class MTLRenderPassDescriptor_class;
static Class MTLTextureDescriptor_class;

// Metal constants
#define MTLPixelFormatBGRA8Unorm 80
#define MTLTextureUsageRenderTarget 0x0004
#define MTLTextureUsageShaderRead 0x0001
#define MTLStorageModePrivate 2
#define MTLStorageModeShared 1
#define MTLResourceStorageModeShared 1
#define MTLLoadActionClear 2
#define MTLStoreActionStore 1

// Application state
#define NUM_FRAMES 10
#define FRAME_WIDTH 800
#define FRAME_HEIGHT 600

static struct {
    id device;
    id command_queue;
    id render_textures[NUM_FRAMES];
    id readback_buffer;
    sg_pass_action pass_action;
    sg_pipeline pip;
    sg_bindings bind;
    uint8_t* frame_data;
} app_state;

// Triangle vertex data
static float vertices[] = {
    // positions      colors
    0.0f,  0.5f,  1.0f, 0.0f, 0.0f, 1.0f, // top vertex (red)
    0.5f,  -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, // bottom right (green)
    -0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 1.0f  // bottom left (blue)
};

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

// Initialize Metal
static void metal_init(void) {
    // Get selectors
    sel_alloc = sel_registerName("alloc");
    sel_init = sel_registerName("init");
    sel_release = sel_registerName("release");
    sel_retain = sel_registerName("retain");
    sel_newCommandQueue = sel_registerName("newCommandQueue");
    sel_commandBuffer = sel_registerName("commandBuffer");
    sel_commit = sel_registerName("commit");
    sel_waitUntilCompleted = sel_registerName("waitUntilCompleted");
    sel_renderCommandEncoderWithDescriptor = sel_registerName("renderCommandEncoderWithDescriptor:");
    sel_blitCommandEncoder = sel_registerName("blitCommandEncoder");
    sel_endEncoding = sel_registerName("endEncoding");
    sel_texture2DDescriptorWithPixelFormat_width_height_mipmapped = sel_registerName("texture2DDescriptorWithPixelFormat:width:height:mipmapped:");
    sel_newTextureWithDescriptor = sel_registerName("newTextureWithDescriptor:");
    sel_setStorageMode = sel_registerName("setStorageMode:");
    sel_setUsage = sel_registerName("setUsage:");
    sel_newBufferWithLength_options = sel_registerName("newBufferWithLength:options:");
    sel_contents = sel_registerName("contents");
    sel_copyFromTexture_sourceSlice_sourceLevel_sourceOrigin_sourceSize_toBuffer_destinationOffset_destinationBytesPerRow_destinationBytesPerImage =
        sel_registerName("copyFromTexture:sourceSlice:sourceLevel:sourceOrigin:sourceSize:toBuffer:destinationOffset:destinationBytesPerRow:destinationBytesPerImage:");
    sel_colorAttachments = sel_registerName("colorAttachments");
    sel_objectAtIndexedSubscript = sel_registerName("objectAtIndexedSubscript:");
    sel_setTexture = sel_registerName("setTexture:");
    sel_setLoadAction = sel_registerName("setLoadAction:");
    sel_setStoreAction = sel_registerName("setStoreAction:");
    sel_setClearColor = sel_registerName("setClearColor:");

    // Get classes
    MTLTextureDescriptor_class = objc_getClass("MTLTextureDescriptor");
    MTLRenderPassDescriptor_class = objc_getClass("MTLRenderPassDescriptor");

    // Create device using proper casting
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wcast-function-type-mismatch"
    app_state.device = ((id (*)(void))MTLCreateSystemDefaultDevice)();
    #pragma clang diagnostic pop
    if (!app_state.device) {
        fprintf(stderr, "Failed to create Metal device\n");
        exit(1);
    }
    msgSend0(app_state.device, sel_retain);

    // Create command queue
    app_state.command_queue = msgSend0(app_state.device, sel_newCommandQueue);
    msgSend0(app_state.command_queue, sel_retain);

    // Create render target textures
    for (int i = 0; i < NUM_FRAMES; i++) {
        // Create texture descriptor using class method
        id tex_desc = ((id (*)(id, SEL, int, int, int, BOOL))objc_msgSend)(
            MTLTextureDescriptor_class,
            sel_texture2DDescriptorWithPixelFormat_width_height_mipmapped,
            MTLPixelFormatBGRA8Unorm, FRAME_WIDTH, FRAME_HEIGHT, NO);

        msgSendInt(tex_desc, sel_setStorageMode, MTLStorageModePrivate);
        msgSendInt(tex_desc, sel_setUsage, MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead);

        app_state.render_textures[i] = msgSend1(app_state.device, sel_newTextureWithDescriptor, tex_desc);
        msgSend0(app_state.render_textures[i], sel_retain);
    }

    // Create readback buffer for all frames
    size_t buffer_size = FRAME_WIDTH * FRAME_HEIGHT * 4 * NUM_FRAMES;
    app_state.readback_buffer = ((id (*)(id, SEL, size_t, int))objc_msgSend)(
        app_state.device, sel_newBufferWithLength_options,
        buffer_size, MTLResourceStorageModeShared);
    msgSend0(app_state.readback_buffer, sel_retain);

    // Allocate CPU memory for frame data
    app_state.frame_data = (uint8_t*)malloc(buffer_size);
}

static void sokol_init(void) {
    // Initialize Sokol GFX with Metal backend
    sg_setup(&(sg_desc){
        .environment = {
            .metal = {
                .device = app_state.device,
            },
        },
        .logger.func = slog_func,
    });

    // Create vertex buffer
    app_state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .data = {
            .ptr = vertices,
            .size = sizeof(vertices)
        },
        .label = "triangle-vertices"
    });

    // Create shader and pipeline
    sg_shader shd = sg_make_shader(triangle_shader_desc(sg_query_backend()));

    app_state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .layout = {
            .attrs = {
                [ATTR_triangle_position].format = SG_VERTEXFORMAT_FLOAT2,
                [ATTR_triangle_color].format = SG_VERTEXFORMAT_FLOAT4
            }
        },
        .label = "triangle-pipeline"
    });

    // Set up clear action
    app_state.pass_action = (sg_pass_action){
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = {0.0f, 0.0f, 0.0f, 1.0f}  // Black background
        }
    };
}

static void render_frames(void) {
    printf("Rendering %d frames...\n", NUM_FRAMES);

    for (int i = 0; i < NUM_FRAMES; i++) {
        // Create color attachment view for current render texture
        sg_view color_view = sg_make_view(&(sg_view_desc){
            .color_attachment = {
                .image = {
                    .id = (uint32_t)(uintptr_t)app_state.render_textures[i]
                },
            }
        });

        // Begin offscreen pass
        sg_begin_pass(&(sg_pass){
            .action = app_state.pass_action,
            .attachments = {
                .colors[0] = color_view,
            }
        });

        // Draw triangle
        sg_apply_pipeline(app_state.pip);
        sg_apply_bindings(&app_state.bind);
        sg_draw(0, 3, 1);

        sg_end_pass();
        sg_commit();

        // Clean up view
        sg_destroy_view(color_view);

        printf("  Frame %d rendered\n", i);
    }
}

static void readback_frames(void) {
    printf("Reading back frames from GPU...\n");

    // Create command buffer for blit operations
    id cmd_buffer = msgSend0(app_state.command_queue, sel_commandBuffer);
    id blit_encoder = msgSend0(cmd_buffer, sel_blitCommandEncoder);

    // Copy each texture to the readback buffer
    for (int i = 0; i < NUM_FRAMES; i++) {
        size_t bytes_per_row = FRAME_WIDTH * 4;
        size_t bytes_per_image = bytes_per_row * FRAME_HEIGHT;
        size_t offset = (size_t)i * bytes_per_image;

        // sourceOrigin and sourceSize structs
        typedef struct {
            NSUInteger x, y, z;
        } MyMTLOrigin;

        typedef struct {
            NSUInteger width, height, depth;
        } MyMTLSize;

        MyMTLOrigin origin = {0, 0, 0};
        MyMTLSize size = {FRAME_WIDTH, FRAME_HEIGHT, 1};

        // Copy texture to buffer
        ((void (*)(id, SEL, id, NSUInteger, NSUInteger, MyMTLOrigin, MyMTLSize, id, NSUInteger, NSUInteger, NSUInteger))objc_msgSend)(
            blit_encoder,
            sel_copyFromTexture_sourceSlice_sourceLevel_sourceOrigin_sourceSize_toBuffer_destinationOffset_destinationBytesPerRow_destinationBytesPerImage,
            app_state.render_textures[i],  // source texture
            0,                              // sourceSlice
            0,                              // sourceLevel
            origin,                         // sourceOrigin
            size,                           // sourceSize
            app_state.readback_buffer,     // destination buffer
            offset,                         // destinationOffset
            bytes_per_row,                  // destinationBytesPerRow
            bytes_per_image                 // destinationBytesPerImage
        );
    }

    msgSend0(blit_encoder, sel_endEncoding);
    msgSend0(cmd_buffer, sel_commit);
    msgSend0(cmd_buffer, sel_waitUntilCompleted);

    // Copy data from Metal buffer to CPU memory
    void* buffer_contents = msgSend0(app_state.readback_buffer, sel_contents);
    memcpy(app_state.frame_data, buffer_contents, FRAME_WIDTH * FRAME_HEIGHT * 4 * NUM_FRAMES);
}

static void save_frames(void) {
    printf("Saving frames to disk...\n");

    for (int i = 0; i < NUM_FRAMES; i++) {
        char filename[256];
        snprintf(filename, sizeof(filename), "frame_%02d.raw", i);

        size_t frame_size = FRAME_WIDTH * FRAME_HEIGHT * 4;
        size_t offset = (size_t)i * frame_size;

        FILE* f = fopen(filename, "wb");
        if (f) {
            // Convert BGRA to RGBA
            uint8_t* frame_ptr = app_state.frame_data + offset;
            for (int j = 0; j < FRAME_WIDTH * FRAME_HEIGHT; j++) {
                uint8_t b = frame_ptr[j * 4 + 0];
                uint8_t g = frame_ptr[j * 4 + 1];
                uint8_t r = frame_ptr[j * 4 + 2];
                uint8_t a = frame_ptr[j * 4 + 3];

                // Write as RGBA
                fputc(r, f);
                fputc(g, f);
                fputc(b, f);
                fputc(a, f);
            }
            fclose(f);
            printf("  Saved %s (%dx%d RGBA)\n", filename, FRAME_WIDTH, FRAME_HEIGHT);
        } else {
            fprintf(stderr, "Failed to open %s for writing\n", filename);
        }
    }
}

static void cleanup(void) {
    // Cleanup Sokol
    sg_shutdown();

    // Release Metal resources
    for (int i = 0; i < NUM_FRAMES; i++) {
        if (app_state.render_textures[i]) {
            msgSend0(app_state.render_textures[i], sel_release);
        }
    }

    if (app_state.readback_buffer) {
        msgSend0(app_state.readback_buffer, sel_release);
    }

    if (app_state.command_queue) {
        msgSend0(app_state.command_queue, sel_release);
    }

    if (app_state.device) {
        msgSend0(app_state.device, sel_release);
    }

    // Free CPU memory
    if (app_state.frame_data) {
        free(app_state.frame_data);
    }
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    printf("Initializing headless Metal renderer...\n");

    // Initialize Metal
    metal_init();

    // Initialize Sokol
    sokol_init();

    // Render all frames
    render_frames();

    // Readback frames from GPU
    readback_frames();

    // Save frames to disk (in final use case, this would be omitted)
    save_frames();

    // Cleanup
    cleanup();

    printf("Done!\n");
    return 0;
}
