#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <Block.h>

// Metal and Foundation headers
#include <objc/objc.h>
#include <objc/runtime.h>
#include <objc/message.h>
#include <CoreGraphics/CoreGraphics.h>
#include <Metal/Metal.h>

// Sokol headers
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_log.h"
#include "sokol/sokol_time.h"
#include "shaders/triangle.h"

// Profiler
#include "profiler.h"

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
static SEL sel_renderCommandEncoderWithDescriptor;
static SEL sel_blitCommandEncoder;
static SEL sel_endEncoding;
static SEL sel_texture2DDescriptorWithPixelFormat_width_height_mipmapped;
static SEL sel_newTextureWithDescriptor;
static SEL sel_setStorageMode;
static SEL sel_setUsage;
static SEL sel_newBufferWithLength_options;
static SEL sel_contents;
static SEL
    sel_copyFromTexture_sourceSlice_sourceLevel_sourceOrigin_sourceSize_toBuffer_destinationOffset_destinationBytesPerRow_destinationBytesPerImage;
static SEL sel_colorAttachments;
static SEL sel_objectAtIndexedSubscript;
static SEL sel_setTexture;
static SEL sel_setLoadAction;
static SEL sel_setStoreAction;
static SEL sel_setClearColor;
static SEL sel_addCompletedHandler;
static SEL sel_status;
static SEL sel_presentDrawable;
static SEL sel_nextDrawable;

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
#define MTLCommandBufferStatusCompleted 4

// Application constants
#define NUM_FRAMES 200 // 2.5 seconds at 24fps
#define FRAME_WIDTH 1080
#define FRAME_HEIGHT 1920
#define FRAME_SIZE_BYTES (FRAME_WIDTH * FRAME_HEIGHT * 4)

// Frame data structure for queue
typedef struct {
  uint8_t *data;
  int frame_number;
  atomic_bool ready;
} frame_data_t;

// Application state
static struct {
  // Metal objects
  id device;
  id command_queue;
  id render_textures[NUM_FRAMES];
  id readback_buffers[NUM_FRAMES]; // Individual readback buffer per frame
  id command_buffers[NUM_FRAMES];  // Command buffer per frame

  // Sokol objects
  sg_image render_images[NUM_FRAMES];
  sg_pass_action pass_action;
  sg_pipeline pip;
  sg_bindings bind;

  // Frame management
  frame_data_t frames[NUM_FRAMES];
  atomic_int frames_rendered;
  atomic_int frames_ready;
  atomic_int frames_encoded;

  // Threading
  pthread_t encoder_thread;
  pthread_mutex_t queue_mutex;
  pthread_cond_t queue_cond;
  FILE *ffmpeg_pipe;

  // Timing
  struct timeval start_time;
  struct timeval render_complete_time;
  struct timeval readback_complete_time;
  struct timeval encode_complete_time;
} app_state;

// Triangle vertex data
static float vertices[] = {
    // positions      colors
    0.0f,  0.5f,  1.0f, 0.0f, 0.0f, 1.0f, // top vertex (red)
    0.5f,  -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, // bottom right (green)
    -0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 1.0f  // bottom left (blue)
};

// vs_params_t is defined in the shader header (triangle.h)

// Create a 4x4 rotation matrix around Z axis
static void mat4_rotation_z(float *m, float angle_rad) {
  float c = cosf(angle_rad);
  float s = sinf(angle_rad);

  memset(m, 0, sizeof(float) * 16);
  m[0] = c;
  m[1] = s;
  m[4] = -s;
  m[5] = c;
  m[10] = 1.0f;
  m[15] = 1.0f;
}

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

// Timing utilities
static double get_time_diff(struct timeval *start, struct timeval *end) {
  return (double)(end->tv_sec - start->tv_sec) +
         (double)(end->tv_usec - start->tv_usec) / 1000000.0;
}

// BGRA to RGB conversion removed - we now send BGRA directly to FFmpeg

// Encoder thread function
static void *encoder_thread_func(void *arg) {
  PROFILE_BEGIN("ffmpeg encoder thread");
  (void)arg;
  printf("[Encoder] Thread started\n");

  int next_frame_to_encode = 0;

  while (next_frame_to_encode < NUM_FRAMES) {
    PROFILE_BEGIN("ffmpeg do frame");
    // Wait for the next frame to be ready
    while (!atomic_load(&app_state.frames[next_frame_to_encode].ready)) {
      // usleep(100); // Small sleep to avoid busy waiting
    }

    // Write BGRA frame directly - no conversion needed!
    frame_data_t *frame = &app_state.frames[next_frame_to_encode];

    size_t written =
        fwrite(frame->data, 1, FRAME_SIZE_BYTES, app_state.ffmpeg_pipe);
    if (written != FRAME_SIZE_BYTES) {
      fprintf(stderr, "[Encoder] Failed to write frame %d\n",
              next_frame_to_encode);
    }

    atomic_fetch_add(&app_state.frames_encoded, 1);
    printf("[Encoder] Encoded frame %d/%d\n", next_frame_to_encode + 1,
           NUM_FRAMES);
    next_frame_to_encode++;
    PROFILE_END();
  }

  // Close FFmpeg pipe
  pclose(app_state.ffmpeg_pipe);
  gettimeofday(&app_state.encode_complete_time, NULL);

  printf("[Encoder] Thread finished - all frames encoded\n");
  PROFILE_END();
  return NULL;
}

// Completion handler for command buffer
typedef void (^CommandBufferHandler)(id);

// Initialize Metal
static void metal_init(void) {
  PROFILE_BEGIN("metal_init");

  // Get selectors
  sel_alloc = sel_registerName("alloc");
  sel_init = sel_registerName("init");
  sel_release = sel_registerName("release");
  sel_retain = sel_registerName("retain");
  sel_newCommandQueue = sel_registerName("newCommandQueue");
  sel_commandBuffer = sel_registerName("commandBuffer");
  sel_commit = sel_registerName("commit");
  sel_waitUntilCompleted = sel_registerName("waitUntilCompleted");
  sel_renderCommandEncoderWithDescriptor =
      sel_registerName("renderCommandEncoderWithDescriptor:");
  sel_blitCommandEncoder = sel_registerName("blitCommandEncoder");
  sel_endEncoding = sel_registerName("endEncoding");
  sel_texture2DDescriptorWithPixelFormat_width_height_mipmapped =
      sel_registerName(
          "texture2DDescriptorWithPixelFormat:width:height:mipmapped:");
  sel_newTextureWithDescriptor = sel_registerName("newTextureWithDescriptor:");
  sel_setStorageMode = sel_registerName("setStorageMode:");
  sel_setUsage = sel_registerName("setUsage:");
  sel_newBufferWithLength_options =
      sel_registerName("newBufferWithLength:options:");
  sel_contents = sel_registerName("contents");
  sel_copyFromTexture_sourceSlice_sourceLevel_sourceOrigin_sourceSize_toBuffer_destinationOffset_destinationBytesPerRow_destinationBytesPerImage =
      sel_registerName("copyFromTexture:sourceSlice:sourceLevel:sourceOrigin:"
                       "sourceSize:toBuffer:destinationOffset:"
                       "destinationBytesPerRow:destinationBytesPerImage:");
  sel_colorAttachments = sel_registerName("colorAttachments");
  sel_objectAtIndexedSubscript = sel_registerName("objectAtIndexedSubscript:");
  sel_setTexture = sel_registerName("setTexture:");
  sel_setLoadAction = sel_registerName("setLoadAction:");
  sel_setStoreAction = sel_registerName("setStoreAction:");
  sel_setClearColor = sel_registerName("setClearColor:");
  sel_addCompletedHandler = sel_registerName("addCompletedHandler:");
  sel_status = sel_registerName("status");

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

  // Create render textures and readback buffers for all frames
  for (int i = 0; i < NUM_FRAMES; i++) {
    // Create texture descriptor
    id tex_desc = ((id (*)(id, SEL, int, int, int, BOOL))objc_msgSend)(
        MTLTextureDescriptor_class,
        sel_texture2DDescriptorWithPixelFormat_width_height_mipmapped,
        MTLPixelFormatBGRA8Unorm, FRAME_WIDTH, FRAME_HEIGHT, NO);

    msgSendInt(tex_desc, sel_setStorageMode, MTLStorageModePrivate);
    msgSendInt(tex_desc, sel_setUsage,
               MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead);

    app_state.render_textures[i] =
        msgSend1(app_state.device, sel_newTextureWithDescriptor, tex_desc);
    msgSend0(app_state.render_textures[i], sel_retain);

    // Create individual readback buffer for this frame
    app_state.readback_buffers[i] =
        ((id (*)(id, SEL, size_t, int))objc_msgSend)(
            app_state.device, sel_newBufferWithLength_options, FRAME_SIZE_BYTES,
            MTLResourceStorageModeShared);
    msgSend0(app_state.readback_buffers[i], sel_retain);

    // Allocate CPU memory for this frame
    app_state.frames[i].data = (uint8_t *)malloc(FRAME_SIZE_BYTES);
    app_state.frames[i].frame_number = i;
    atomic_store(&app_state.frames[i].ready, false);
  }

  PROFILE_END();
}

static void sokol_init(void) {
  PROFILE_BEGIN("sokol_init");

  // Initialize Sokol GFX with Metal backend
  sg_setup(&(sg_desc){
      .environment =
          {
              .metal =
                  {
                      .device = app_state.device,
                  },
          },
      .image_pool_size =
          NUM_FRAMES + 10, // Need one image per frame plus some overhead
      .view_pool_size =
          NUM_FRAMES + 10, // Need one view per frame plus some overhead
      .logger.func = slog_func,
  });

  // Create Sokol image wrappers for our Metal textures
  for (int i = 0; i < NUM_FRAMES; i++) {
    app_state.render_images[i] = sg_make_image(&(sg_image_desc){
        .usage =
            {
                .color_attachment = true,
            },
        .width = FRAME_WIDTH,
        .height = FRAME_HEIGHT,
        .pixel_format = SG_PIXELFORMAT_BGRA8,
        .sample_count = 1,
        .mtl_textures[0] =
            app_state.render_textures[i], // Wrap our Metal texture
        .label = "render-target"});
  }

  // Create vertex buffer
  app_state.bind.vertex_buffers[0] = sg_make_buffer(
      &(sg_buffer_desc){.data = {.ptr = vertices, .size = sizeof(vertices)},
                        .label = "triangle-vertices"});

  // Create shader and pipeline
  sg_shader shd = sg_make_shader(triangle_shader_desc(sg_query_backend()));

  app_state.pip = sg_make_pipeline(
      &(sg_pipeline_desc){.shader = shd,
                          .layout = {.attrs = {[ATTR_triangle_position].format =
                                                   SG_VERTEXFORMAT_FLOAT2,
                                               [ATTR_triangle_color].format =
                                                   SG_VERTEXFORMAT_FLOAT4}},
                          .label = "triangle-pipeline"});

  // Set up clear action
  app_state.pass_action = (sg_pass_action){
      .colors[0] = {
          .load_action = SG_LOADACTION_CLEAR,
          .clear_value = {0.0f, 0.0f, 0.0f, 1.0f} // Black background
      }};

  PROFILE_END();
}

static void render_all_frames(void) {
  PROFILE_BEGIN("render_all_frames");
  printf("[Renderer] Submitting all %d frames to GPU...\n", NUM_FRAMES);

  const float dt = 1.0f / 24.0f;
  const float rotation_speed = 2.0f;

  // First pass: Submit all render commands
  PROFILE_BEGIN("render_submission");
  for (int i = 0; i < NUM_FRAMES; i++) {
    // Calculate rotation for this frame
    float time = (float)i * dt;
    float angle = time * rotation_speed;
    vs_params_t vs_params;
    mat4_rotation_z(vs_params.model, angle);

    PROFILE_BEGIN("sg make view");
    // Create view for this frame's render target
    sg_view color_view =
        sg_make_view(&(sg_view_desc){.color_attachment = {
                                         .image = app_state.render_images[i],
                                     }});
    PROFILE_END();

    PROFILE_BEGIN("sg begin pass");
    // Begin render pass for this frame
    sg_begin_pass(&(sg_pass){.action = app_state.pass_action,
                             .attachments = {
                                 .colors[0] = color_view,
                             }});
    PROFILE_END();

    PROFILE_BEGIN("sg apply pipeline");
    // Draw triangle
    sg_apply_pipeline(app_state.pip);
    PROFILE_END();

    PROFILE_BEGIN("sg apply bindings");
    sg_apply_bindings(&app_state.bind);
    PROFILE_END();

    PROFILE_BEGIN("sg apply uniforms");
    sg_apply_uniforms(0, &(sg_range){&vs_params, sizeof(vs_params)});
    PROFILE_END();

    PROFILE_BEGIN("sg draw");
    sg_draw(0, 3, 1);
    PROFILE_END();

    PROFILE_BEGIN("sg end pass");
    sg_end_pass();
    PROFILE_END();

    PROFILE_BEGIN("destroy view");
    // Clean up view
    sg_destroy_view(color_view);
    PROFILE_END();

    atomic_fetch_add(&app_state.frames_rendered, 1);
  }

  // commit all frames at once
  sg_commit();
  PROFILE_END(); // render_submission

  gettimeofday(&app_state.render_complete_time, NULL);
  printf("[Renderer] All frames submitted to GPU\n");

  // Second pass: Set up async readback for all frames
  PROFILE_BEGIN("readback_setup");
  for (int i = 0; i < NUM_FRAMES; i++) {
    PROFILE_BEGIN("frame_readback_setup");
    // Capture frame index by value
    const int frame_idx = i;

    // Create command buffer with readback operations
    id cmd_buffer = msgSend0(app_state.command_queue, sel_commandBuffer);
    id blit_encoder = msgSend0(cmd_buffer, sel_blitCommandEncoder);

    // Copy texture to readback buffer
    typedef struct {
      NSUInteger x, y, z;
    } MyMTLOrigin;
    typedef struct {
      NSUInteger width, height, depth;
    } MyMTLSize;

    MyMTLOrigin origin = {0, 0, 0};
    MyMTLSize size = {FRAME_WIDTH, FRAME_HEIGHT, 1};

    ((void (*)(id, SEL, id, NSUInteger, NSUInteger, MyMTLOrigin, MyMTLSize, id,
               NSUInteger, NSUInteger, NSUInteger))objc_msgSend)(
        blit_encoder,
        sel_copyFromTexture_sourceSlice_sourceLevel_sourceOrigin_sourceSize_toBuffer_destinationOffset_destinationBytesPerRow_destinationBytesPerImage,
        app_state.render_textures[frame_idx], 0, 0, origin, size,
        app_state.readback_buffers[frame_idx], 0, FRAME_WIDTH * 4,
        FRAME_SIZE_BYTES);

    msgSend0(blit_encoder, sel_endEncoding);

    // Add completion handler
    CommandBufferHandler handler = Block_copy(^(id cmd) {
      (void)cmd;
      // Copy data from Metal buffer to CPU memory
      void *buffer_contents =
          msgSend0(app_state.readback_buffers[frame_idx], sel_contents);
      memcpy(app_state.frames[frame_idx].data, buffer_contents,
             FRAME_SIZE_BYTES);

      // Mark frame as ready for encoding
      atomic_store(&app_state.frames[frame_idx].ready, true);
      atomic_fetch_add(&app_state.frames_ready, 1);

      if (atomic_load(&app_state.frames_ready) == NUM_FRAMES) {
        gettimeofday(&app_state.readback_complete_time, NULL);
      }
    });

    ((void (*)(id, SEL, CommandBufferHandler))objc_msgSend)(
        cmd_buffer, sel_addCompletedHandler, handler);
    msgSend0(cmd_buffer, sel_commit);
    Block_release(handler);

    PROFILE_END(); // frame_readback_setup
  }
  PROFILE_END(); // readback_setup

  PROFILE_END(); // render_all_frames
}

static void start_ffmpeg_encoder(void) {
  PROFILE_BEGIN("start_ffmpeg_encoder");

  // Start FFmpeg process with hardware encoding (VideoToolbox on macOS)
  // Using BGRA input directly - no color conversion needed!
  // Added compression settings: CRF for quality, preset for speed/compression
  // tradeoff
  const char *ffmpeg_cmd =
      "ffmpeg -loglevel error -f rawvideo -pixel_format bgra -video_size "
      "1080x1920 "
      "-framerate 24 -i - -c:v h264_videotoolbox -pix_fmt yuv420p "
      "-b:v 2M -profile:v high -level 4.0 -y output.mp4";

  app_state.ffmpeg_pipe = popen(ffmpeg_cmd, "w");
  if (!app_state.ffmpeg_pipe) {
    fprintf(stderr, "Failed to launch ffmpeg\n");
    exit(1);
  }

  // Start encoder thread
  pthread_create(&app_state.encoder_thread, NULL, encoder_thread_func, NULL);

  PROFILE_END();
}

static void wait_for_completion(void) {
  PROFILE_BEGIN("wait_for_completion");

  // Wait for encoder thread to finish
  pthread_join(app_state.encoder_thread, NULL);

  PROFILE_END();

  // Print timing results
  double render_time =
      get_time_diff(&app_state.start_time, &app_state.render_complete_time);
  double readback_time =
      get_time_diff(&app_state.start_time, &app_state.readback_complete_time);
  double total_time =
      get_time_diff(&app_state.start_time, &app_state.encode_complete_time);

  printf("\n=== Performance Metrics ===\n");
  printf("Render submission: %.3f seconds\n", render_time);
  printf("All frames ready:  %.3f seconds\n", readback_time);
  printf("Total time:        %.3f seconds\n", total_time);
  printf("Speedup:           %.2fx (vs 1.045s baseline)\n", 1.045 / total_time);
  printf("FPS achieved:      %.1f fps\n", NUM_FRAMES / total_time);
  printf("===========================\n");
}

static void cleanup(void) {
  // Destroy Sokol images
  for (int i = 0; i < NUM_FRAMES; i++) {
    if (app_state.render_images[i].id != 0) {
      sg_destroy_image(app_state.render_images[i]);
    }
  }

  // Cleanup Sokol
  sg_shutdown();

  // Release Metal objects
  for (int i = 0; i < NUM_FRAMES; i++) {
    if (app_state.render_textures[i]) {
      msgSend0(app_state.render_textures[i], sel_release);
    }
    if (app_state.readback_buffers[i]) {
      msgSend0(app_state.readback_buffers[i], sel_release);
    }
    if (app_state.command_buffers[i]) {
      msgSend0(app_state.command_buffers[i], sel_release);
    }
    free(app_state.frames[i].data);
  }

  if (app_state.command_queue) {
    msgSend0(app_state.command_queue, sel_release);
  }

  if (app_state.device) {
    msgSend0(app_state.device, sel_release);
  }

  pthread_mutex_destroy(&app_state.queue_mutex);
  pthread_cond_destroy(&app_state.queue_cond);
}

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  printf("=== Fast Parallel Video Renderer ===\n");
  printf("Frames: %d, Resolution: %dx%d\n", NUM_FRAMES, FRAME_WIDTH,
         FRAME_HEIGHT);
  printf("=====================================\n\n");
  stm_setup();

  // Initialize atomics and threading
  atomic_store(&app_state.frames_rendered, 0);
  atomic_store(&app_state.frames_ready, 0);
  atomic_store(&app_state.frames_encoded, 0);
  pthread_mutex_init(&app_state.queue_mutex, NULL);
  pthread_cond_init(&app_state.queue_cond, NULL);

  // Start timing
  gettimeofday(&app_state.start_time, NULL);

  // Initialize Metal and Sokol
  printf("[Main] Initializing Metal...\n");
  metal_init();

  printf("[Main] Initializing Sokol...\n");
  sokol_init();

  // Start FFmpeg encoder thread
  printf("[Main] Starting FFmpeg encoder thread...\n");
  start_ffmpeg_encoder();

  // Render all frames (non-blocking)
  render_all_frames();

  // Wait for everything to complete
  wait_for_completion();

  // Cleanup
  cleanup();

  // Print profiling results
  printf("\n");
  profiler_end_and_print_session(NULL);

  printf("\n✅ Video generated: output.mp4\n");
  return 0;
}

// Assert we haven't exceeded max profile points
PROFILE_ASSERT_END_OF_COMPILATION_UNIT;
