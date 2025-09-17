#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

// FFmpeg headers
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

// Profiler
#include "profiler.h"

// GPU backend abstraction
#include "gpu_backend.h"

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

// Uniform buffer structure matching Metal shader
typedef struct {
    float model[16];
} uniforms_t;

// Application state
static struct {
  // GPU backend objects
  gpu_device_t *device;
  gpu_texture_t *render_textures[NUM_FRAMES];
  gpu_readback_buffer_t *readback_buffers[NUM_FRAMES];
  gpu_command_buffer_t *readback_commands[NUM_FRAMES];
  gpu_pipeline_t *pipeline;
  gpu_buffer_t *vertex_buffer;

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
     0.0f,  0.5f,  1.0f, 0.0f, 0.0f, 1.0f, // top (red)
    -0.5f, -0.5f,  0.0f, 1.0f, 0.0f, 1.0f, // bottom left (green)
     0.5f, -0.5f,  0.0f, 0.0f, 1.0f, 1.0f  // bottom right (blue)
};

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

// Timing utilities
static double get_time_diff(struct timeval *start, struct timeval *end) {
  return (double)(end->tv_sec - start->tv_sec) +
         (double)(end->tv_usec - start->tv_usec) / 1000000.0;
}

// Initialize FFmpeg encoder
static int init_ffmpeg_encoder(const char *filename) {
  int ret;

  // Allocate format context
  ret = avformat_alloc_output_context2(&app_state.format_ctx, NULL, NULL,
                                       filename);
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
  ret = avcodec_parameters_from_context(app_state.video_stream->codecpar,
                                        app_state.codec_ctx);
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
      FRAME_WIDTH, FRAME_HEIGHT, AV_PIX_FMT_BGRA, FRAME_WIDTH, FRAME_HEIGHT,
      AV_PIX_FMT_YUV420P, SWS_FAST_BILINEAR, NULL, NULL, NULL);

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

// Load shader from file
static char* load_shader_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Warning: Could not open shader file %s\n", filename);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* content = (char*)malloc((size_t)(size + 1));
    fread(content, 1, (size_t)size, file);
    content[size] = '\0';

    fclose(file);
    return content;
}

// Initialize GPU backend
static void gpu_backend_init(void) {
  PROFILE_BEGIN("gpu_backend_init");

  // Initialize GPU device
  app_state.device = gpu_init();
  if (!app_state.device) {
    fprintf(stderr, "Failed to create GPU device\n");
    exit(1);
  }

  // Load shader source - try multiple paths
  char* shader_source = load_shader_file("triangle.metal");
  if (!shader_source) {
    shader_source = load_shader_file("src/shaders/triangle.metal");
  }
  if (!shader_source) {
    shader_source = load_shader_file("../../src/shaders/triangle.metal");
  }

  // Create vertex layout
  gpu_vertex_attr_t attributes[] = {
    {.index = 0, .offset = 0, .format = 0},  // position (float2)
    {.index = 1, .offset = 8, .format = 2}   // color (float4)
  };

  gpu_vertex_layout_t vertex_layout = {
    .attributes = attributes,
    .num_attributes = 2,
    .stride = 24  // 2 floats + 4 floats = 6 floats = 24 bytes
  };

  // Create pipeline
  app_state.pipeline = gpu_create_pipeline(
    app_state.device,
    shader_source,
    "vertex_main",
    "fragment_main",
    &vertex_layout
  );

  if (shader_source) {
    free(shader_source);
  }

  if (!app_state.pipeline) {
    fprintf(stderr, "Failed to create render pipeline\n");
    exit(1);
  }

  // Create vertex buffer
  app_state.vertex_buffer = gpu_create_buffer(app_state.device, vertices, sizeof(vertices));

  // Create render textures and readback buffers for all frames
  for (int i = 0; i < NUM_FRAMES; i++) {
    // Create render texture
    app_state.render_textures[i] =
        gpu_create_texture(app_state.device, FRAME_WIDTH, FRAME_HEIGHT);

    // Create readback buffer for this frame
    app_state.readback_buffers[i] =
        gpu_create_readback_buffer(app_state.device, FRAME_SIZE_BYTES);

    // Allocate CPU memory for this frame
    app_state.frames[i].data = (uint8_t *)malloc(FRAME_SIZE_BYTES);
    app_state.frames[i].frame_number = i;
    atomic_store(&app_state.frames[i].ready, false);
  }

  PROFILE_END();
}

static void render_all_frames(void) {
  PROFILE_BEGIN("render_all_frames");
  printf("[Renderer] Submitting all %d frames to GPU...\n", NUM_FRAMES);

  const float dt = 1.0f / 24.0f;
  const float rotation_speed = 2.0f;

  // First pass: Submit all render commands
  PROFILE_BEGIN("render_submission");

  // Create command buffers for each frame
  gpu_command_buffer_t* cmd_buffers[NUM_FRAMES];

  for (int i = 0; i < NUM_FRAMES; i++) {
    // Calculate rotation for this frame
    float time = (float)i * dt;
    float angle = time * rotation_speed;
    uniforms_t uniforms;
    mat4_rotation_z(uniforms.model, angle);

    PROFILE_BEGIN("render_frame");

    // Create a command buffer for this frame
    cmd_buffers[i] = gpu_begin_commands(app_state.device);

    // Begin render pass for this frame
    gpu_render_encoder_t* encoder = gpu_begin_render_pass(
      cmd_buffers[i],
      app_state.render_textures[i],
      0.0f, 0.0f, 0.0f, 1.0f  // Black background
    );

    // Set pipeline and vertex buffer
    gpu_set_pipeline(encoder, app_state.pipeline);
    gpu_set_vertex_buffer(encoder, app_state.vertex_buffer, 0);

    // Set uniforms at buffer index 1 (index 0 is the vertex buffer)
    gpu_set_uniforms(encoder, 1, &uniforms, sizeof(uniforms));

    // Draw triangle
    gpu_draw(encoder, 3);

    // End render pass
    gpu_end_render_pass(encoder);

    // Commit and wait for this frame's commands
    gpu_commit_commands(cmd_buffers[i], true);  // Wait for each frame

    PROFILE_END();

    atomic_fetch_add(&app_state.frames_rendered, 1);
  }

  PROFILE_END(); // render_submission

  gettimeofday(&app_state.render_complete_time, NULL);
  printf("[Renderer] All frames submitted to GPU\n");

  // Second pass: Set up async readback for all frames
  PROFILE_BEGIN("readback all frames");
  // Now poll for completion in a separate loop
  for (int i = 0; i < NUM_FRAMES; i++) {

    PROFILE_BEGIN("read back single frame");

    PROFILE_BEGIN("read back cmd");
    app_state.readback_commands[i] = gpu_readback_texture_async(
        app_state.device, app_state.render_textures[i],
        app_state.readback_buffers[i], FRAME_WIDTH, FRAME_HEIGHT);
    PROFILE_END();

    PROFILE_BEGIN("submit read cmd");
    // Submit the command (blocking for simplicity)
    gpu_submit_commands(app_state.readback_commands[i], true);
    PROFILE_END();

    printf("frame %d ready\n", i);

    PROFILE_BEGIN("copy readback data");
    // Copy data from GPU buffer to CPU memory
    gpu_copy_readback_data(app_state.readback_buffers[i],
                           app_state.frames[i].data, FRAME_SIZE_BYTES);
    PROFILE_END();

    // Mark frame as ready for encoding
    atomic_store(&app_state.frames[i].ready, true);
    atomic_fetch_add(&app_state.frames_ready, 1);

    if (atomic_load(&app_state.frames_ready) == NUM_FRAMES) {
      gettimeofday(&app_state.readback_complete_time, NULL);
    }
    PROFILE_END();
  }
  PROFILE_END();

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

  // Release GPU backend objects
  for (int i = 0; i < NUM_FRAMES; i++) {
    if (app_state.render_textures[i]) {
      gpu_destroy_texture(app_state.render_textures[i]);
    }
    if (app_state.readback_buffers[i]) {
      gpu_destroy_readback_buffer(app_state.readback_buffers[i]);
    }
    if (app_state.readback_commands[i]) {
      gpu_destroy_command_buffer(app_state.readback_commands[i]);
    }
    free(app_state.frames[i].data);
  }

  if (app_state.pipeline) {
    gpu_destroy_pipeline(app_state.pipeline);
  }

  if (app_state.vertex_buffer) {
    gpu_destroy_buffer(app_state.vertex_buffer);
  }

  if (app_state.device) {
    gpu_destroy(app_state.device);
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

  // Initialize atomics and threading
  atomic_store(&app_state.frames_rendered, 0);
  atomic_store(&app_state.frames_ready, 0);
  atomic_store(&app_state.frames_encoded, 0);
  pthread_mutex_init(&app_state.queue_mutex, NULL);
  pthread_cond_init(&app_state.queue_cond, NULL);

  // Start timing
  gettimeofday(&app_state.start_time, NULL);

  // Initialize GPU backend
  printf("[Main] Initializing GPU backend...\n");
  gpu_backend_init();

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
  profiler_end_and_print_session();

  printf("\n✅ Video generated: output.mp4\n");
  return 0;
}

// Assert we haven't exceeded max profile points
PROFILE_ASSERT_END_OF_COMPILATION_UNIT
