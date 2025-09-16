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

// FFmpeg headers
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

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

  // FFmpeg encoding context
  AVFormatContext *format_ctx;
  AVCodecContext *codec_ctx;
  AVStream *video_stream;
  AVPacket *packet;
  AVFrame *frame;
  struct SwsContext *sws_ctx;
  int64_t pts_counter;

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

// Initialize FFmpeg encoder
static int init_ffmpeg_encoder(const char *filename) {
  int ret;

  // Allocate format context
  ret = avformat_alloc_output_context2(&app_state.format_ctx, NULL, NULL, filename);
  if (ret < 0) {
    fprintf(stderr, "Failed to allocate output context\n");
    return -1;
  }

  // Find H.264 encoder (use VideoToolbox on macOS)
  const AVCodec *codec = avcodec_find_encoder_by_name("h264_videotoolbox");
  if (!codec) {
    // Fallback to software encoder
    codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
      fprintf(stderr, "H.264 encoder not found\n");
      return -1;
    }
  }

  // Create new video stream
  app_state.video_stream = avformat_new_stream(app_state.format_ctx, NULL);
  if (!app_state.video_stream) {
    fprintf(stderr, "Failed to create video stream\n");
    return -1;
  }

  // Allocate codec context
  app_state.codec_ctx = avcodec_alloc_context3(codec);
  if (!app_state.codec_ctx) {
    fprintf(stderr, "Failed to allocate codec context\n");
    return -1;
  }

  // Set codec parameters
  app_state.codec_ctx->width = FRAME_WIDTH;
  app_state.codec_ctx->height = FRAME_HEIGHT;
  app_state.codec_ctx->time_base = (AVRational){1, 24};
  app_state.codec_ctx->framerate = (AVRational){24, 1};
  app_state.codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
  app_state.codec_ctx->bit_rate = 2000000; // 2 Mbps

  // Set H.264 specific options
  av_opt_set(app_state.codec_ctx->priv_data, "profile", "high", 0);
  av_opt_set(app_state.codec_ctx->priv_data, "level", "4.0", 0);
  if (codec->name && strstr(codec->name, "videotoolbox")) {
    // VideoToolbox specific options for better performance
    av_opt_set(app_state.codec_ctx->priv_data, "realtime", "1", 0);
  }

  // Open codec
  ret = avcodec_open2(app_state.codec_ctx, codec, NULL);
  if (ret < 0) {
    fprintf(stderr, "Failed to open codec\n");
    return -1;
  }

  // Copy codec parameters to stream
  ret = avcodec_parameters_from_context(app_state.video_stream->codecpar, app_state.codec_ctx);
  if (ret < 0) {
    fprintf(stderr, "Failed to copy codec parameters\n");
    return -1;
  }

  app_state.video_stream->time_base = app_state.codec_ctx->time_base;

  // Open output file
  if (!(app_state.format_ctx->oformat->flags & AVFMT_NOFILE)) {
    ret = avio_open(&app_state.format_ctx->pb, filename, AVIO_FLAG_WRITE);
    if (ret < 0) {
      fprintf(stderr, "Failed to open output file\n");
      return -1;
    }
  }

  // Write file header
  ret = avformat_write_header(app_state.format_ctx, NULL);
  if (ret < 0) {
    fprintf(stderr, "Failed to write header\n");
    return -1;
  }

  // Allocate frame and packet
  app_state.frame = av_frame_alloc();
  app_state.frame->format = app_state.codec_ctx->pix_fmt;
  app_state.frame->width = app_state.codec_ctx->width;
  app_state.frame->height = app_state.codec_ctx->height;
  ret = av_frame_get_buffer(app_state.frame, 0);
  if (ret < 0) {
    fprintf(stderr, "Failed to allocate frame buffer\n");
    return -1;
  }

  app_state.packet = av_packet_alloc();

  // Initialize SWS context for BGRA to YUV420P conversion
  app_state.sws_ctx = sws_getContext(
    FRAME_WIDTH, FRAME_HEIGHT, AV_PIX_FMT_BGRA,
    FRAME_WIDTH, FRAME_HEIGHT, AV_PIX_FMT_YUV420P,
    SWS_FAST_BILINEAR, NULL, NULL, NULL);

  if (!app_state.sws_ctx) {
    fprintf(stderr, "Failed to create SWS context\n");
    return -1;
  }

  app_state.pts_counter = 0;

  printf("[FFmpeg] Encoder initialized (using %s)\n", codec->name);
  return 0;
}

// Close FFmpeg encoder
static void close_ffmpeg_encoder(void) {
  // Write trailer
  if (app_state.format_ctx) {
    av_write_trailer(app_state.format_ctx);
  }

  // Clean up
  if (app_state.sws_ctx) {
    sws_freeContext(app_state.sws_ctx);
  }
  if (app_state.frame) {
    av_frame_free(&app_state.frame);
  }
  if (app_state.packet) {
    av_packet_free(&app_state.packet);
  }
  if (app_state.codec_ctx) {
    avcodec_free_context(&app_state.codec_ctx);
  }
  if (app_state.format_ctx) {
    if (!(app_state.format_ctx->oformat->flags & AVFMT_NOFILE)) {
      avio_closep(&app_state.format_ctx->pb);
    }
    avformat_free_context(app_state.format_ctx);
  }
}

// Encode a single frame
static int encode_frame(uint8_t *bgra_data) {
  int ret;

  // Convert BGRA to YUV420P
  const uint8_t *src_data[4] = {bgra_data, NULL, NULL, NULL};
  int src_linesize[4] = {FRAME_WIDTH * 4, 0, 0, 0};

  ret = av_frame_make_writable(app_state.frame);
  if (ret < 0) {
    return ret;
  }

  sws_scale(app_state.sws_ctx, src_data, src_linesize, 0, FRAME_HEIGHT,
            app_state.frame->data, app_state.frame->linesize);

  app_state.frame->pts = app_state.pts_counter++;

  // Send frame to encoder
  ret = avcodec_send_frame(app_state.codec_ctx, app_state.frame);
  if (ret < 0) {
    fprintf(stderr, "Error sending frame to encoder\n");
    return ret;
  }

  // Receive encoded packets
  while (ret >= 0) {
    ret = avcodec_receive_packet(app_state.codec_ctx, app_state.packet);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      break;
    } else if (ret < 0) {
      fprintf(stderr, "Error receiving packet from encoder\n");
      return ret;
    }

    // Rescale timestamps
    av_packet_rescale_ts(app_state.packet, app_state.codec_ctx->time_base,
                         app_state.video_stream->time_base);
    app_state.packet->stream_index = app_state.video_stream->index;

    // Write packet to file
    ret = av_interleaved_write_frame(app_state.format_ctx, app_state.packet);
    av_packet_unref(app_state.packet);
    if (ret < 0) {
      fprintf(stderr, "Error writing packet\n");
      return ret;
    }
  }

  return 0;
}

// Encoder thread function
static void *encoder_thread_func(void *arg) {
  (void)arg;
  printf("[Encoder] Thread started\n");

  int next_frame_to_encode = 0;

  while (next_frame_to_encode < NUM_FRAMES) {
    PROFILE_BEGIN("ffmpeg wait for frame");
    // Wait for the next frame to be ready
    while (!atomic_load(&app_state.frames[next_frame_to_encode].ready)) {
      usleep(100); // Small sleep to avoid busy waiting
    }
    PROFILE_END();

    PROFILE_BEGIN("ffmpeg encode frame");
    // Encode frame directly in memory
    frame_data_t *frame = &app_state.frames[next_frame_to_encode];

    if (encode_frame(frame->data) < 0) {
      fprintf(stderr, "[Encoder] Failed to encode frame %d\n",
              next_frame_to_encode);
    }
    PROFILE_END();

    atomic_fetch_add(&app_state.frames_encoded, 1);
    printf("[Encoder] Encoded frame %d/%d\n", next_frame_to_encode + 1,
           NUM_FRAMES);
    next_frame_to_encode++;
  }

  // Flush encoder
  avcodec_send_frame(app_state.codec_ctx, NULL);
  AVPacket *flush_pkt = av_packet_alloc();
  while (avcodec_receive_packet(app_state.codec_ctx, flush_pkt) == 0) {
    av_packet_rescale_ts(flush_pkt, app_state.codec_ctx->time_base,
                         app_state.video_stream->time_base);
    flush_pkt->stream_index = app_state.video_stream->index;
    av_interleaved_write_frame(app_state.format_ctx, flush_pkt);
    av_packet_unref(flush_pkt);
  }
  av_packet_free(&flush_pkt);

  gettimeofday(&app_state.encode_complete_time, NULL);

  printf("[Encoder] Thread finished - all frames encoded\n");
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

  // Initialize FFmpeg encoder
  if (init_ffmpeg_encoder("output.mp4") < 0) {
    fprintf(stderr, "Failed to initialize FFmpeg encoder\n");
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
  // Close FFmpeg encoder
  close_ffmpeg_encoder();

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
