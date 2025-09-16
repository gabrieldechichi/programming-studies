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
    id render_textures[NUM_FRAMES];  // Metal textures
    sg_image render_images[NUM_FRAMES];  // Sokol image wrappers
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

    // Create Metal textures for render targets
    for (int i = 0; i < NUM_FRAMES; i++) {
        // Create texture descriptor
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

    // Create Sokol image wrappers for our Metal textures
    for (int i = 0; i < NUM_FRAMES; i++) {
        app_state.render_images[i] = sg_make_image(&(sg_image_desc){
            .usage = {
                .color_attachment = true,
            },
            .width = FRAME_WIDTH,
            .height = FRAME_HEIGHT,
            .pixel_format = SG_PIXELFORMAT_BGRA8,
            .sample_count = 1,
            .mtl_textures[0] = app_state.render_textures[i],  // Wrap our Metal texture
            .label = "render-target"
        });
    }

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
        // Create color attachment view for current render image
        sg_view color_view = sg_make_view(&(sg_view_desc){
            .color_attachment = {
                .image = app_state.render_images[i],
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

static void generate_mp4(void) {
    printf("Generating MP4 video...\n");

    // FFmpeg command to read raw RGB24 frames from stdin and create MP4
    // -f rawvideo: input format is raw video
    // -pixel_format rgb24: input pixel format (we'll convert from BGRA to RGB)
    // -video_size: dimensions of input frames
    // -framerate: output framerate (30 fps for smooth playback)
    // -i -: read from stdin
    // -c:v libx264: use H.264 codec
    // -pix_fmt yuv420p: output pixel format for compatibility
    // -y: overwrite output file
    const char* ffmpeg_cmd =
        "ffmpeg -loglevel error -f rawvideo -pixel_format rgb24 -video_size 800x600 "
        "-framerate 30 -i - -c:v libx264 -pix_fmt yuv420p -y output.mp4";

    FILE* ffmpeg = popen(ffmpeg_cmd, "w");
    if (!ffmpeg) {
        fprintf(stderr, "Failed to launch ffmpeg\n");
        return;
    }

    // Write frames to ffmpeg
    for (int i = 0; i < NUM_FRAMES; i++) {
        size_t frame_size = FRAME_WIDTH * FRAME_HEIGHT * 4;
        size_t offset = (size_t)i * frame_size;
        uint8_t* frame_ptr = app_state.frame_data + offset;

        // Convert BGRA to RGB and write to ffmpeg
        for (int j = 0; j < FRAME_WIDTH * FRAME_HEIGHT; j++) {
            uint8_t b = frame_ptr[j * 4 + 0];
            uint8_t g = frame_ptr[j * 4 + 1];
            uint8_t r = frame_ptr[j * 4 + 2];
            // Skip alpha channel for RGB24 format

            // Write RGB
            fputc(r, ffmpeg);
            fputc(g, ffmpeg);
            fputc(b, ffmpeg);
        }

        printf("  Wrote frame %d/%d\n", i + 1, NUM_FRAMES);
    }

    // Close pipe and wait for ffmpeg to finish
    int result = pclose(ffmpeg);
    if (result == 0) {
        printf("✅ Successfully generated output.mp4\n");
    } else {
        fprintf(stderr, "FFmpeg exited with error code: %d\n", result);
    }
}

static void cleanup(void) {
    // Destroy Sokol images BEFORE shutting down Sokol
    for (int i = 0; i < NUM_FRAMES; i++) {
        if (app_state.render_images[i].id != 0) {
            sg_destroy_image(app_state.render_images[i]);
        }
    }

    // Cleanup Sokol
    sg_shutdown();

    // Release Metal textures
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
    fflush(stdout);

    // Initialize Metal
    printf("Initializing Metal...\n");
    fflush(stdout);
    metal_init();

    // Initialize Sokol
    printf("Initializing Sokol...\n");
    fflush(stdout);
    sokol_init();

    // Render all frames
    printf("Starting render...\n");
    fflush(stdout);
    render_frames();

    // Readback frames from GPU
    readback_frames();

    // Generate MP4 video using ffmpeg
    generate_mp4();

    // Cleanup
    cleanup();

    printf("Done!\n");
    return 0;
}
