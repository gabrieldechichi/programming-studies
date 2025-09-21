#include "../lib/json_parser.h"
#include "../os/os.h"
#include "memory.h"
#include "typedefs.h"
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>

// FFmpeg headers
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>

// Profiler
#include "profiler.h"

// GPU backend abstraction
#include "gpu_backend.h"

// Application constants
#define MAX_FRAMES 1440 // Maximum frames for longest video (60 seconds at 24fps)
#define NUM_TEXTURE_POOLS                                                      \
  1 // Use single texture set, process frames sequentially
#define FRAME_WIDTH 1080
#define FRAME_HEIGHT 1920
#define FRAME_SIZE_BYTES (FRAME_WIDTH * FRAME_HEIGHT * 4)
#define YUV_Y_SIZE_BYTES                                                       \
  (FRAME_WIDTH * FRAME_HEIGHT) // Y plane: full resolution
#define YUV_UV_SIZE_BYTES                                                      \
  (FRAME_WIDTH * FRAME_HEIGHT / 4) // U,V planes: quarter resolution (4:2:0)
#define YUV_TOTAL_SIZE_BYTES                                                   \
  (YUV_Y_SIZE_BYTES + 2 * YUV_UV_SIZE_BYTES) // Y + U + V
#define INPUT_BUFFER_SIZE MB(1)
#define SOCKET_PATH "/tmp/video_renderer.sock"

// Memory allocation sizes
#define PERMANENT_MEMORY_SIZE MB(200)  // 200MB for permanent allocations (GPU objects, no frames)
#define TEMPORARY_MEMORY_SIZE GB(5)    // 5GB for temporary allocations (frames + profiler + other temp data)

// Frame data structure for queue
typedef struct {
  uint8_t *data;
  int frame_number;
  atomic_bool ready;
} frame_data_t;

// Request structure
typedef struct {
  double seconds;
  int num_frames;
} render_request_t;

// Uniform buffer structure matching Metal shader
typedef struct {
  float model[16];
} uniforms_t;

// Application context with memory allocators
typedef struct {
  // Memory allocators
  ArenaAllocator permanent_arena;
  ArenaAllocator temporary_arena;
  Allocator permanent_allocator;
  Allocator temporary_allocator;

  // Raw memory blocks (to be freed on exit)
  uint8_t *permanent_memory;
  uint8_t *temporary_memory;

  // GPU backend objects
  gpu_device_t *device;
  gpu_texture_t *render_texture_pool[NUM_TEXTURE_POOLS];
  gpu_readback_buffer_t *readback_buffer_pool[NUM_TEXTURE_POOLS];
  gpu_command_buffer_t *readback_commands[MAX_FRAMES];
  gpu_pipeline_t *pipeline;
  gpu_buffer_t *vertex_buffer;

  // GPU color conversion objects
  gpu_compute_pipeline_t *compute_pipeline;
  gpu_texture_t *yuv_y_texture_pool[NUM_TEXTURE_POOLS];
  gpu_texture_t *yuv_u_texture_pool[NUM_TEXTURE_POOLS];
  gpu_texture_t *yuv_v_texture_pool[NUM_TEXTURE_POOLS];
  gpu_readback_buffer_t
      *yuv_readback_buffer_pool[NUM_TEXTURE_POOLS]; // Pool of readback buffers
  gpu_command_buffer_t
      *yuv_readback_commands[MAX_FRAMES]; // Single command per frame

  // Frame management
  frame_data_t frames[MAX_FRAMES];
  atomic_int frames_rendered;
  atomic_int frames_ready;
  atomic_int frames_encoded;
  int current_num_frames;

  // Pool slot synchronization - track which frame is using each pool
  atomic_int pool_slot_in_use[NUM_TEXTURE_POOLS]; // -1 = free, >= 0 = frame
                                                  // number using this slot

  // Initialization state
  bool initialized;

  // Threading
  pthread_t encoder_thread;
  pthread_mutex_t queue_mutex;
  pthread_cond_t queue_cond;

  // FFmpeg encoding context (per-request)
  AVFormatContext *format_ctx;
  AVCodecContext *codec_ctx;
  AVStream *video_stream;
  int64_t pts_counter;

  // FFmpeg cached objects (initialized once)
  const AVCodec *cached_codec;
  struct SwsContext *cached_sws_ctx;
  AVFrame *cached_frame;
  AVPacket *cached_packet;

  // Timing
  struct timeval start_time;
  struct timeval render_complete_time;
  struct timeval readback_complete_time;
  struct timeval encode_complete_time;
} AppContext;

// Global application context
static AppContext g_ctx;

// Triangle vertex data
static float vertices[] = {
    // positions      colors
    0.0f,  0.5f,  1.0f, 0.0f, 0.0f, 1.0f, // top (red)
    -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, // bottom left (green)
    0.5f,  -0.5f, 0.0f, 0.0f, 1.0f, 1.0f  // bottom right (blue)
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

// Initialize application context with memory allocators
static int init_context(AppContext *ctx) {
  // Zero out the context
  memset(ctx, 0, sizeof(AppContext));

  // Allocate permanent memory block (only malloc call #1)
  ctx->permanent_memory = (uint8_t *)malloc(PERMANENT_MEMORY_SIZE);
  if (!ctx->permanent_memory) {
    fprintf(stderr, "Failed to allocate permanent memory (%d MB)\n", PERMANENT_MEMORY_SIZE / MB(1));
    return -1;
  }

  // Allocate temporary memory block (only malloc call #2)
  ctx->temporary_memory = (uint8_t *)malloc(TEMPORARY_MEMORY_SIZE);
  if (!ctx->temporary_memory) {
    fprintf(stderr, "Failed to allocate temporary memory (%d MB)\n", TEMPORARY_MEMORY_SIZE / MB(1));
    free(ctx->permanent_memory);
    return -1;
  }

  // Initialize arena allocators
  ctx->permanent_arena = arena_from_buffer(ctx->permanent_memory, PERMANENT_MEMORY_SIZE);
  ctx->temporary_arena = arena_from_buffer(ctx->temporary_memory, TEMPORARY_MEMORY_SIZE);

  // Wrap arenas with allocator interface
  ctx->permanent_allocator = make_arena_allocator(&ctx->permanent_arena);
  ctx->temporary_allocator = make_arena_allocator(&ctx->temporary_arena);

  printf("[Memory] Initialized allocators: Permanent=%dMB, Temporary=%dMB\n",
         PERMANENT_MEMORY_SIZE / MB(1), TEMPORARY_MEMORY_SIZE / MB(1));

  return 0;
}

// Clean up application context
static void cleanup_context(AppContext *ctx) {
  // if (ctx->permanent_memory) {
  //   free(ctx->permanent_memory);
  //   ctx->permanent_memory = NULL;
  // }
  // if (ctx->temporary_memory) {
  //   free(ctx->temporary_memory);
  //   ctx->temporary_memory = NULL;
  // }

  printf("[Memory] Context cleaned up\n");
}

// Allocate frame data for current request (from temporary allocator)
static int allocate_frame_data_for_request(int num_frames) {
  printf("[Memory] Allocating frame data for request: %d frames x %d bytes = %zu MB\n",
         num_frames, YUV_TOTAL_SIZE_BYTES, ((size_t)num_frames * YUV_TOTAL_SIZE_BYTES) / MB(1));

  for (int i = 0; i < num_frames; i++) {
    g_ctx.frames[i].data = ALLOC_ARRAY(&g_ctx.temporary_allocator, uint8_t, YUV_TOTAL_SIZE_BYTES);
    if (!g_ctx.frames[i].data) {
      fprintf(stderr, "Failed to allocate frame data for frame %d (need %d bytes)\n", i, YUV_TOTAL_SIZE_BYTES);
      fprintf(stderr, "Available: %zu MB, Requested: %zu MB total\n",
              ALLOC_FREE_SIZE(&g_ctx.temporary_allocator) / MB(1),
              ((size_t)num_frames * YUV_TOTAL_SIZE_BYTES) / MB(1));
      return -1;
    }
    atomic_store(&g_ctx.frames[i].ready, false);
  }

  // Clear data pointers for unused frames
  for (int i = num_frames; i < MAX_FRAMES; i++) {
    g_ctx.frames[i].data = NULL;
  }

  printf("[Memory] Frame allocation complete for request. Temporary allocator usage: %zu/%zu MB\n",
         ALLOC_COMMITED_SIZE(&g_ctx.temporary_allocator) / MB(1),
         ALLOC_CAPACITY(&g_ctx.temporary_allocator) / MB(1));

  return 0;
}

// Initialize FFmpeg cached objects (call once at startup)
static int init_ffmpeg_cache(void) {
  printf("[FFmpeg] Initializing cached objects...\n");

  // Find H.264 encoder - prioritize hardware encoders
  const AVCodec *codec = NULL;

  // Try NVENC first (NVIDIA hardware encoding)
  codec = avcodec_find_encoder_by_name("h264_nvenc");
  if (codec) {
    printf("[FFmpeg] Using NVENC hardware encoder\n");
  } else {
    // Try VideoToolbox (macOS hardware encoding)
    codec = avcodec_find_encoder_by_name("h264_videotoolbox");
    if (codec) {
      printf("[FFmpeg] Using VideoToolbox hardware encoder\n");
    } else {
      // Try Intel QuickSync
      codec = avcodec_find_encoder_by_name("h264_qsv");
      if (codec) {
        printf("[FFmpeg] Using Intel QuickSync hardware encoder\n");
      } else {
        // Fallback to software encoder
        codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (codec) {
          printf("[FFmpeg] Using software encoder (libx264)\n");
        } else {
          fprintf(stderr, "No H.264 encoder found\n");
          return -1;
        }
      }
    }
  }
  g_ctx.cached_codec = codec;

  // Initialize SWS context for BGRA to YUV420P conversion
  g_ctx.cached_sws_ctx = sws_getContext(
      FRAME_WIDTH, FRAME_HEIGHT, AV_PIX_FMT_BGRA, FRAME_WIDTH, FRAME_HEIGHT,
      AV_PIX_FMT_YUV420P, SWS_FAST_BILINEAR, NULL, NULL, NULL);

  if (!g_ctx.cached_sws_ctx) {
    fprintf(stderr, "Failed to create SWS context\n");
    return -1;
  }

  // Allocate frame and packet (reused across requests)
  g_ctx.cached_frame = av_frame_alloc();
  g_ctx.cached_frame->format = AV_PIX_FMT_YUV420P;
  g_ctx.cached_frame->width = FRAME_WIDTH;
  g_ctx.cached_frame->height = FRAME_HEIGHT;
  int ret = av_frame_get_buffer(g_ctx.cached_frame, 0);
  if (ret < 0) {
    fprintf(stderr, "Failed to allocate frame buffer\n");
    return -1;
  }

  g_ctx.cached_packet = av_packet_alloc();

  printf("[FFmpeg] Cached objects initialized (using %s)\n", codec->name);
  return 0;
}

// Open FFmpeg encoder for a specific request (uses cached objects)
static int open_ffmpeg_encoder(const char *filename) {
  int ret;

  // Allocate format context
  ret = avformat_alloc_output_context2(&g_ctx.format_ctx, NULL, NULL,
                                       filename);
  if (ret < 0) {
    fprintf(stderr, "Failed to allocate output context\n");
    return -1;
  }

  // Create new video stream
  g_ctx.video_stream = avformat_new_stream(g_ctx.format_ctx, NULL);
  if (!g_ctx.video_stream) {
    fprintf(stderr, "Failed to create video stream\n");
    return -1;
  }

  // Allocate codec context using cached codec
  g_ctx.codec_ctx = avcodec_alloc_context3(g_ctx.cached_codec);
  if (!g_ctx.codec_ctx) {
    fprintf(stderr, "Failed to allocate codec context\n");
    return -1;
  }

  // Set codec parameters
  g_ctx.codec_ctx->width = FRAME_WIDTH;
  g_ctx.codec_ctx->height = FRAME_HEIGHT;
  g_ctx.codec_ctx->time_base = (AVRational){1, 24};
  g_ctx.codec_ctx->framerate = (AVRational){24, 1};
  g_ctx.codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
  g_ctx.codec_ctx->bit_rate = 2000000; // 2 Mbps

  // Set encoder-specific options using cached codec
  if (g_ctx.cached_codec->name && strstr(g_ctx.cached_codec->name, "nvenc")) {
    // NVENC specific options for best performance
    av_opt_set(g_ctx.codec_ctx->priv_data, "preset", "p1",
               0); // Fastest preset
    av_opt_set(g_ctx.codec_ctx->priv_data, "tune", "ll", 0); // Low latency
    av_opt_set(g_ctx.codec_ctx->priv_data, "rc", "cbr",
               0); // Constant bitrate
    av_opt_set(g_ctx.codec_ctx->priv_data, "gpu", "0", 0); // Use GPU 0
    av_opt_set(g_ctx.codec_ctx->priv_data, "delay", "0",
               0); // No B-frame delay
  } else if (g_ctx.cached_codec->name && strstr(g_ctx.cached_codec->name, "videotoolbox")) {
    // VideoToolbox specific options for better performance
    av_opt_set(g_ctx.codec_ctx->priv_data, "realtime", "1", 0);
  } else if (g_ctx.cached_codec->name && strstr(g_ctx.cached_codec->name, "qsv")) {
    // Intel QuickSync specific options
    av_opt_set(g_ctx.codec_ctx->priv_data, "preset", "veryfast", 0);
  } else {
    // Software encoder (libx264) options
    av_opt_set(g_ctx.codec_ctx->priv_data, "profile", "high", 0);
    av_opt_set(g_ctx.codec_ctx->priv_data, "level", "4.0", 0);
  }

  // Open codec
  ret = avcodec_open2(g_ctx.codec_ctx, g_ctx.cached_codec, NULL);
  if (ret < 0) {
    fprintf(stderr, "Failed to open codec\n");
    return -1;
  }

  // Copy codec parameters to stream
  ret = avcodec_parameters_from_context(g_ctx.video_stream->codecpar,
                                        g_ctx.codec_ctx);
  if (ret < 0) {
    fprintf(stderr, "Failed to copy codec parameters\n");
    return -1;
  }

  g_ctx.video_stream->time_base = g_ctx.codec_ctx->time_base;

  // Open output file
  if (!(g_ctx.format_ctx->oformat->flags & AVFMT_NOFILE)) {
    ret = avio_open(&g_ctx.format_ctx->pb, filename, AVIO_FLAG_WRITE);
    if (ret < 0) {
      fprintf(stderr, "Failed to open output file\n");
      return -1;
    }
  }

  // Write file header
  ret = avformat_write_header(g_ctx.format_ctx, NULL);
  if (ret < 0) {
    fprintf(stderr, "Failed to write header\n");
    return -1;
  }

  // Reset PTS counter for this request
  g_ctx.pts_counter = 0;

  printf("[FFmpeg] Encoder opened for file: %s\n", filename);
  return 0;
}

// Close FFmpeg encoder (only closes per-request objects)
static void close_ffmpeg_encoder(void) {
  // Write trailer
  if (g_ctx.format_ctx) {
    av_write_trailer(g_ctx.format_ctx);
  }

  // Clean up per-request objects only (cached objects remain)
  if (g_ctx.codec_ctx) {
    avcodec_free_context(&g_ctx.codec_ctx);
    g_ctx.codec_ctx = NULL;
  }
  if (g_ctx.format_ctx) {
    if (!(g_ctx.format_ctx->oformat->flags & AVFMT_NOFILE)) {
      avio_closep(&g_ctx.format_ctx->pb);
    }
    avformat_free_context(g_ctx.format_ctx);
    g_ctx.format_ctx = NULL;
  }
  g_ctx.video_stream = NULL; // Part of format_ctx, so just NULL it

  printf("[FFmpeg] Encoder closed for current request\n");
}

// Clean up FFmpeg cached objects (call once at shutdown)
static void cleanup_ffmpeg_cache(void) {
  // Clean up cached objects
  if (g_ctx.cached_sws_ctx) {
    sws_freeContext(g_ctx.cached_sws_ctx);
    g_ctx.cached_sws_ctx = NULL;
  }
  if (g_ctx.cached_frame) {
    av_frame_free(&g_ctx.cached_frame);
  }
  if (g_ctx.cached_packet) {
    av_packet_free(&g_ctx.cached_packet);
  }

  printf("[FFmpeg] Cached objects cleaned up\n");
}

// Encode a single frame
static int encode_frame(uint8_t *yuv_data) {
  int ret;

  ret = av_frame_make_writable(g_ctx.cached_frame);
  if (ret < 0) {
    return ret;
  }

  // YUV data layout in memory: [Y_PLANE][U_PLANE][V_PLANE]
  uint8_t *y_src = yuv_data;
  uint8_t *u_src = yuv_data + YUV_Y_SIZE_BYTES;
  uint8_t *v_src = yuv_data + YUV_Y_SIZE_BYTES + YUV_UV_SIZE_BYTES;

  // Debug: Print YUV data sizes on first frame
  static int first_frame = 1;
  if (first_frame) {
    printf("[Debug] YUV data sizes: Y=%d, U=%d, V=%d, Total=%d\n",
           YUV_Y_SIZE_BYTES, YUV_UV_SIZE_BYTES, YUV_UV_SIZE_BYTES,
           YUV_TOTAL_SIZE_BYTES);
    first_frame = 0;
  }

  // Copy Y plane data with proper linesize alignment
  uint8_t *dst_y = g_ctx.cached_frame->data[0];
  for (int y = 0; y < FRAME_HEIGHT; y++) {
    memcpy(dst_y, y_src, FRAME_WIDTH);
    y_src += FRAME_WIDTH;                  // Source has no padding
    dst_y += g_ctx.cached_frame->linesize[0]; // FFmpeg frame might have padding
  }

  // Copy U plane data with proper linesize alignment
  uint8_t *dst_u = g_ctx.cached_frame->data[1];
  for (int y = 0; y < FRAME_HEIGHT / 2; y++) {
    memcpy(dst_u, u_src, FRAME_WIDTH / 2);
    u_src += FRAME_WIDTH / 2;              // Source has no padding
    dst_u += g_ctx.cached_frame->linesize[1]; // FFmpeg frame might have padding
  }

  // Copy V plane data with proper linesize alignment
  uint8_t *dst_v = g_ctx.cached_frame->data[2];
  for (int y = 0; y < FRAME_HEIGHT / 2; y++) {
    memcpy(dst_v, v_src, FRAME_WIDTH / 2);
    v_src += FRAME_WIDTH / 2;              // Source has no padding
    dst_v += g_ctx.cached_frame->linesize[2]; // FFmpeg frame might have padding
  }

  g_ctx.cached_frame->pts = g_ctx.pts_counter++;

  // Send frame to encoder
  ret = avcodec_send_frame(g_ctx.codec_ctx, g_ctx.cached_frame);
  if (ret < 0) {
    fprintf(stderr, "Error sending frame to encoder\n");
    return ret;
  }

  // Receive encoded packets
  while (ret >= 0) {
    ret = avcodec_receive_packet(g_ctx.codec_ctx, g_ctx.cached_packet);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      break;
    } else if (ret < 0) {
      fprintf(stderr, "Error receiving packet from encoder\n");
      return ret;
    }

    // Rescale timestamps
    av_packet_rescale_ts(g_ctx.cached_packet, g_ctx.codec_ctx->time_base,
                         g_ctx.video_stream->time_base);
    g_ctx.cached_packet->stream_index = g_ctx.video_stream->index;

    // Write packet to file
    ret = av_interleaved_write_frame(g_ctx.format_ctx, g_ctx.cached_packet);
    av_packet_unref(g_ctx.cached_packet);
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

  while (next_frame_to_encode < g_ctx.current_num_frames) {
    PROFILE_BEGIN("ffmpeg wait for frame");
    // Wait for the next frame to be ready
    while (!atomic_load(&g_ctx.frames[next_frame_to_encode].ready)) {
      os_sleep_us(100); // Small sleep to avoid busy waiting
    }
    PROFILE_END();

    PROFILE_BEGIN("ffmpeg encode frame");
    // Encode frame directly in memory
    frame_data_t *frame = &g_ctx.frames[next_frame_to_encode];

    if (encode_frame(frame->data) < 0) {
      fprintf(stderr, "[Encoder] Failed to encode frame %d\n",
              next_frame_to_encode);
    }
    PROFILE_END();

    atomic_fetch_add(&g_ctx.frames_encoded, 1);
    next_frame_to_encode++;
  }

  // Flush encoder
  avcodec_send_frame(g_ctx.codec_ctx, NULL);
  AVPacket *flush_pkt = av_packet_alloc();
  while (avcodec_receive_packet(g_ctx.codec_ctx, flush_pkt) == 0) {
    av_packet_rescale_ts(flush_pkt, g_ctx.codec_ctx->time_base,
                         g_ctx.video_stream->time_base);
    flush_pkt->stream_index = g_ctx.video_stream->index;
    av_interleaved_write_frame(g_ctx.format_ctx, flush_pkt);
    av_packet_unref(flush_pkt);
  }
  av_packet_free(&flush_pkt);

  gettimeofday(&g_ctx.encode_complete_time, NULL);

  printf("[Encoder] Thread finished - all frames encoded\n");
  return NULL;
}

// Load shader from file
static char *load_shader_file(const char *filename) {
  FILE *file = fopen(filename, "r");
  if (!file) {
    printf("Warning: Could not open shader file %s\n", filename);
    return NULL;
  }

  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fseek(file, 0, SEEK_SET);

  char *content = ALLOC_ARRAY(&g_ctx.temporary_allocator, char, size + 1);
  if (!content) {
    printf("Warning: Failed to allocate memory for shader file %s\n", filename);
    fclose(file);
    return NULL;
  }

  fread(content, 1, (size_t)size, file);
  content[size] = '\0';

  fclose(file);
  return content;
}

// Initialize GPU backend
static int initialize_system(void) {
  if (g_ctx.initialized) {
    return 0;
  }

  PROFILE_BEGIN("initialize_system");
  printf("[System] Initializing GPU backend and FFmpeg...\n");

  // Initialize GPU device
  g_ctx.device = gpu_init(&g_ctx.permanent_allocator, &g_ctx.temporary_allocator);
  if (!g_ctx.device) {
    fprintf(stderr, "Failed to create GPU device\n");
    exit(1);
  }

  // Load shader source - try multiple paths
  char *shader_source = load_shader_file("triangle.metal");
  if (!shader_source) {
    shader_source = load_shader_file("src/shaders/triangle.metal");
  }
  if (!shader_source) {
    shader_source = load_shader_file("../../src/shaders/triangle.metal");
  }

  // Create vertex layout
  gpu_vertex_attr_t attributes[] = {
      {.index = 0, .offset = 0, .format = 0}, // position (float2)
      {.index = 1, .offset = 8, .format = 2}  // color (float4)
  };

  gpu_vertex_layout_t vertex_layout = {
      .attributes = attributes,
      .num_attributes = 2,
      .stride = 24 // 2 floats + 4 floats = 6 floats = 24 bytes
  };

  // Create pipeline
  g_ctx.pipeline =
      gpu_create_pipeline(g_ctx.device, shader_source, "vertex_main",
                          "fragment_main", &vertex_layout);

  // No need to free shader_source - allocated from temporary allocator

  if (!g_ctx.pipeline) {
    fprintf(stderr, "Failed to create render pipeline\n");
    exit(1);
  }

  // Create vertex buffer
  g_ctx.vertex_buffer =
      gpu_create_buffer(g_ctx.device, vertices, sizeof(vertices));

  // Create compute pipeline for BGRA to YUV conversion
  g_ctx.compute_pipeline = gpu_create_compute_pipeline(
      g_ctx.device, "out/linux/bgra_to_yuv.comp.spv", MAX_FRAMES);

  if (!g_ctx.compute_pipeline) {
    // Try alternate path
    g_ctx.compute_pipeline =
        gpu_create_compute_pipeline(g_ctx.device, "bgra_to_yuv.comp.spv", MAX_FRAMES);
  }

  // Create texture pools (4 pools instead of 200 unique textures)
  printf("[GPU] Creating %d texture pools (instead of per-frame textures)\n",
         NUM_TEXTURE_POOLS);
  for (int i = 0; i < NUM_TEXTURE_POOLS; i++) {
    // Create render texture pool (BGRA)
    g_ctx.render_texture_pool[i] =
        gpu_create_texture(g_ctx.device, FRAME_WIDTH, FRAME_HEIGHT);

    // Create YUV storage texture pools for compute output
    g_ctx.yuv_y_texture_pool[i] = gpu_create_storage_texture(
        g_ctx.device, FRAME_WIDTH, FRAME_HEIGHT, 1); // R8 format
    g_ctx.yuv_u_texture_pool[i] = gpu_create_storage_texture(
        g_ctx.device, FRAME_WIDTH / 2, FRAME_HEIGHT / 2, 1); // R8 format
    g_ctx.yuv_v_texture_pool[i] = gpu_create_storage_texture(
        g_ctx.device, FRAME_WIDTH / 2, FRAME_HEIGHT / 2, 1); // R8 format

    // Create readback buffer pool for packed YUV data (Y+U+V)
    g_ctx.yuv_readback_buffer_pool[i] =
        gpu_create_readback_buffer(g_ctx.device, YUV_TOTAL_SIZE_BYTES);
  }

  // Frame data will be allocated per-request, not pre-allocated
  // Initialize frame metadata only
  for (int i = 0; i < MAX_FRAMES; i++) {
    g_ctx.frames[i].data = NULL; // Will be allocated per request
    g_ctx.frames[i].frame_number = i;
    atomic_store(&g_ctx.frames[i].ready, false);
  }

  printf("[Memory] Frame metadata initialized. Permanent allocator usage: %zu/%zu MB\n",
         ALLOC_COMMITED_SIZE(&g_ctx.permanent_allocator) / MB(1),
         ALLOC_CAPACITY(&g_ctx.permanent_allocator) / MB(1));

  // Initialize FFmpeg cached objects
  if (init_ffmpeg_cache() < 0) {
    fprintf(stderr, "Failed to initialize FFmpeg cache\n");
    exit(1);
  }

  g_ctx.initialized = true;
  PROFILE_END();
  return 0;
}

static void render_all_frames(void) {
  PROFILE_BEGIN("render_all_frames");
  printf("[Renderer] Processing %d frames sequentially using single texture "
         "set...\n",
         g_ctx.current_num_frames);

  const float dt = 1.0f / 24.0f;
  const float rotation_speed = 2.0f;
  const int pool_index = 0; // Always use pool slot 0 since we only have 1

  // Process frames one by one to ensure correct sequence
  for (int i = 0; i < g_ctx.current_num_frames; i++) {
    // Calculate rotation for this frame
    float time = (float)i * dt;
    float angle = time * rotation_speed;
    uniforms_t uniforms;
    mat4_rotation_z(uniforms.model, angle);

    PROFILE_BEGIN("render_frame");

    // Create a command buffer for this frame
    gpu_command_buffer_t *cmd_buffer = gpu_begin_commands(g_ctx.device);

    // Begin render pass for this frame using the single texture set
    gpu_render_encoder_t *encoder = gpu_begin_render_pass(
        cmd_buffer, g_ctx.render_texture_pool[pool_index], 0.0f, 0.0f, 0.0f,
        1.0f // Black background
    );

    // Set pipeline and vertex buffer
    gpu_set_pipeline(encoder, g_ctx.pipeline);
    gpu_set_vertex_buffer(encoder, g_ctx.vertex_buffer, 0);

    // Set uniforms at buffer index 1 (index 0 is the vertex buffer)
    gpu_set_uniforms(encoder, 1, &uniforms, sizeof(uniforms));

    // Draw triangle
    gpu_draw(encoder, 3);

    // End render pass
    gpu_end_render_pass(encoder);

    // Commit and wait for completion
    gpu_commit_commands(cmd_buffer, true); // Blocking wait

    PROFILE_END();

    // Now do compute conversion and readback for this frame
    PROFILE_BEGIN("compute and readback");

    // Create command buffer for compute dispatch
    gpu_command_buffer_t *compute_cmd = gpu_begin_commands(g_ctx.device);

    // Dispatch compute shader to convert BGRA -> YUV using the single texture
    // set
    gpu_texture_t *compute_textures[] = {
        g_ctx.render_texture_pool[pool_index], // Input BGRA
        g_ctx.yuv_y_texture_pool[pool_index],  // Output Y
        g_ctx.yuv_u_texture_pool[pool_index],  // Output U
        g_ctx.yuv_v_texture_pool[pool_index]   // Output V
    };

    // Dispatch compute: 16x16 workgroups for 1080x1920 image
    int groups_x = (FRAME_WIDTH + 15) / 16;  // ceil(1080/16) = 68
    int groups_y = (FRAME_HEIGHT + 15) / 16; // ceil(1920/16) = 120

    gpu_dispatch_compute(compute_cmd, g_ctx.compute_pipeline,
                         compute_textures, 4, groups_x, groups_y, 1);

    // Commit compute work and wait
    gpu_commit_commands(compute_cmd, true);

    // Create single readback command for all YUV planes
    g_ctx.yuv_readback_commands[i] = gpu_readback_yuv_textures_async(
        g_ctx.device, g_ctx.yuv_y_texture_pool[pool_index],
        g_ctx.yuv_u_texture_pool[pool_index],
        g_ctx.yuv_v_texture_pool[pool_index],
        g_ctx.yuv_readback_buffer_pool[pool_index], FRAME_WIDTH,
        FRAME_HEIGHT);

    // Submit readback command and wait
    gpu_submit_commands(g_ctx.yuv_readback_commands[i], true);

    // Copy data to CPU memory
    gpu_copy_readback_data(g_ctx.yuv_readback_buffer_pool[pool_index],
                           g_ctx.frames[i].data, YUV_TOTAL_SIZE_BYTES);

    // Mark frame as ready for encoding
    atomic_store(&g_ctx.frames[i].ready, true);
    atomic_fetch_add(&g_ctx.frames_ready, 1);
    atomic_fetch_add(&g_ctx.frames_rendered, 1);

    PROFILE_END();
  }

  gettimeofday(&g_ctx.render_complete_time, NULL);
  gettimeofday(&g_ctx.readback_complete_time, NULL);
  printf("[Renderer] All %d frames completed\n", g_ctx.current_num_frames);

  PROFILE_END(); // render_all_frames
}

static int start_ffmpeg_encoder(const char *filename) {
  PROFILE_BEGIN("start_ffmpeg_encoder");

  // Open FFmpeg encoder for this request
  if (open_ffmpeg_encoder(filename) < 0) {
    fprintf(stderr, "Failed to open FFmpeg encoder\n");
    PROFILE_END();
    return -1;
  }

  // Start encoder thread
  pthread_create(&g_ctx.encoder_thread, NULL, encoder_thread_func, NULL);

  PROFILE_END();
  return 0;
}

static void wait_for_completion(void) {
  PROFILE_BEGIN("wait_for_completion");

  // Wait for encoder thread to finish
  pthread_join(g_ctx.encoder_thread, NULL);

  PROFILE_END();

  // Print timing results
  double render_time =
      get_time_diff(&g_ctx.start_time, &g_ctx.render_complete_time);
  double readback_time =
      get_time_diff(&g_ctx.start_time, &g_ctx.readback_complete_time);
  double total_time =
      get_time_diff(&g_ctx.start_time, &g_ctx.encode_complete_time);

  printf("\n=== Performance Metrics ===\n");
  printf("Render submission: %.3f seconds\n", render_time);
  printf("All frames ready:  %.3f seconds\n", readback_time);
  printf("Total time:        %.3f seconds\n", total_time);
  printf("Speedup:           %.2fx (vs 1.045s baseline)\n", 1.045 / total_time);
  printf("FPS achieved:      %.1f fps\n",
         g_ctx.current_num_frames / total_time);
  printf("===========================\n");
}

static void cleanup(void) {
  // Clean up FFmpeg cached objects
  cleanup_ffmpeg_cache();

  // Release GPU backend objects - pools
  for (int i = 0; i < NUM_TEXTURE_POOLS; i++) {
    if (g_ctx.render_texture_pool[i]) {
      gpu_destroy_texture(g_ctx.render_texture_pool[i]);
    }
    if (g_ctx.yuv_y_texture_pool[i]) {
      gpu_destroy_texture(g_ctx.yuv_y_texture_pool[i]);
    }
    if (g_ctx.yuv_u_texture_pool[i]) {
      gpu_destroy_texture(g_ctx.yuv_u_texture_pool[i]);
    }
    if (g_ctx.yuv_v_texture_pool[i]) {
      gpu_destroy_texture(g_ctx.yuv_v_texture_pool[i]);
    }
    if (g_ctx.yuv_readback_buffer_pool[i]) {
      gpu_destroy_readback_buffer(g_ctx.yuv_readback_buffer_pool[i]);
    }
  }

  // Command buffers are already freed by gpu_reset_command_pools() after each request
  // No need to destroy them individually here

  if (g_ctx.pipeline) {
    gpu_destroy_pipeline(g_ctx.pipeline);
  }

  if (g_ctx.vertex_buffer) {
    gpu_destroy_buffer(g_ctx.vertex_buffer);
  }

  if (g_ctx.device) {
    gpu_destroy(g_ctx.device);
  }

  pthread_mutex_destroy(&g_ctx.queue_mutex);
  pthread_cond_destroy(&g_ctx.queue_cond);

  // Clean up context memory
  cleanup_context(&g_ctx);
}

// Parse JSON request and extract seconds
static int parse_request(const char *json_str, render_request_t *request) {
  JsonParser parser = json_parser_init(json_str, &g_ctx.temporary_allocator);

  if (!json_expect_object_start(&parser)) {
    fprintf(stderr, "Expected '{' at start of JSON object\n");
    return -1;
  }
  char *key = json_parse_string_value(&parser);
  if (!key || strcmp(key, "seconds") != 0) {
    fprintf(stderr, "Expected 'seconds' key in JSON, got: %s\n",
            key ? key : "null");
    return -1;
  }
  if (!json_expect_colon(&parser)) {
    fprintf(stderr, "Expected ':' after 'seconds' key\n");
    return -1;
  }

  request->seconds = json_parse_number_value(&parser);
  request->num_frames = (int)(request->seconds * 24.0); // 24 fps

  if (request->num_frames <= 0 || request->num_frames > MAX_FRAMES) {
    fprintf(stderr, "Invalid frame count: %d (max: %d)\n", request->num_frames,
            MAX_FRAMES);
    return -1;
  }

  if (!json_expect_object_end(&parser)) {
    fprintf(stderr, "Expected '}' at end of JSON object\n");
    return -1;
  }
  return 0;
}

// Render video and return base64 encoded result
static int render_video(const render_request_t *request) {
  g_ctx.current_num_frames = request->num_frames;

  // Allocate frame data for this request
  if (allocate_frame_data_for_request(request->num_frames) < 0) {
    return -1;
  }

  // Reset frame states
  atomic_store(&g_ctx.frames_rendered, 0);
  atomic_store(&g_ctx.frames_ready, 0);
  atomic_store(&g_ctx.frames_encoded, 0);

  // Start timing
  gettimeofday(&g_ctx.start_time, NULL);

  // Start encoder for this request
  if (start_ffmpeg_encoder("output.mp4") < 0) {
    return -1;
  }

  // Render frames
  render_all_frames();

  // Wait for completion
  wait_for_completion();

  // Close the encoder to finalize the output file
  close_ffmpeg_encoder();

  // Reset GPU command pools to free all command buffers for next request
  gpu_reset_command_pools(g_ctx.device);

  // Reset compute descriptor pool to free all descriptor sets for next request
  gpu_reset_compute_descriptor_pool(g_ctx.compute_pipeline);

  return 0;
}

// Base64 encoding table
static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Base64 encode function
static char* base64_encode(const unsigned char *data, size_t input_length, size_t *output_length) {
    *output_length = 4 * ((input_length + 2) / 3);

    char *encoded_data = ALLOC_ARRAY(&g_ctx.temporary_allocator, char, *output_length + 1);
    if (!encoded_data) return NULL;

    size_t i, j;
    for (i = 0, j = 0; i < input_length;) {
        uint32_t octet_a = i < input_length ? data[i++] : 0;
        uint32_t octet_b = i < input_length ? data[i++] : 0;
        uint32_t octet_c = i < input_length ? data[i++] : 0;

        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;

        encoded_data[j++] = base64_table[(triple >> 18) & 0x3F];
        encoded_data[j++] = base64_table[(triple >> 12) & 0x3F];
        encoded_data[j++] = base64_table[(triple >> 6) & 0x3F];
        encoded_data[j++] = base64_table[triple & 0x3F];
    }

    // Add padding
    size_t mod = input_length % 3;
    if (mod == 1) {
        encoded_data[*output_length - 2] = '=';
        encoded_data[*output_length - 1] = '=';
    } else if (mod == 2) {
        encoded_data[*output_length - 1] = '=';
    }

    encoded_data[*output_length] = '\0';
    return encoded_data;
}

// Send JSON response with base64 video
static void send_response(int client_fd, bool success, const char *error_msg) {
  if (success) {
    // Read video file and encode as base64
    FILE *video_file = fopen("output.mp4", "rb");
    if (!video_file) {
      const char *error_response = "{\"success\": false, \"error\": \"Failed to open output video\"}\n";
      write(client_fd, error_response, strlen(error_response));
      return;
    }

    fseek(video_file, 0, SEEK_END);
    long file_size = ftell(video_file);
    fseek(video_file, 0, SEEK_SET);

    unsigned char *video_data = ALLOC_ARRAY(&g_ctx.temporary_allocator, unsigned char, file_size);
    if (!video_data) {
      const char *error_response = "{\"success\": false, \"error\": \"Failed to allocate memory for video data\"}\n";
      write(client_fd, error_response, strlen(error_response));
      fclose(video_file);
      return;
    }

    fread(video_data, 1, file_size, video_file);
    fclose(video_file);

    // Encode video to base64
    size_t base64_length;
    char *base64_video = base64_encode(video_data, file_size, &base64_length);
    if (!base64_video) {
      const char *error_response = "{\"success\": false, \"error\": \"Failed to encode video to base64\"}\n";
      write(client_fd, error_response, strlen(error_response));
      return;
    }

    // Build JSON response with actual base64 data
    // Allocate buffer for JSON response (needs to be large enough for base64 + JSON structure)
    size_t response_size = base64_length + 256; // Extra space for JSON structure
    char *response = ALLOC_ARRAY(&g_ctx.temporary_allocator, char, response_size);
    if (!response) {
      const char *error_response = "{\"success\": false, \"error\": \"Failed to allocate memory for response\"}\n";
      write(client_fd, error_response, strlen(error_response));
      return;
    }

    int written = snprintf(response, response_size,
                          "{\"success\": true, \"file_size\": %ld, \"video\": \"%s\"}\n",
                          file_size, base64_video);

    // Send the complete response
    write(client_fd, response, written);
  } else {
    char response[512];
    snprintf(response, sizeof(response),
             "{\"success\": false, \"error\": \"%s\"}\n",
             error_msg ? error_msg : "Unknown error");
    write(client_fd, response, strlen(response));
  }
}

void process_request(int client_fd, char *input_buffer) {
  profiler_begin_session();

  render_request_t request;
  if (parse_request(input_buffer, &request) < 0) {
    send_response(client_fd, false, "Invalid JSON request");
    return;
  }

  printf("Rendering %.2f seconds (%d frames)...\n", request.seconds,
         request.num_frames);
  fflush(stdout);

  if (render_video(&request) < 0) {
    send_response(client_fd, false, "Rendering failed");
    return;
  }

  send_response(client_fd, true, NULL);

  profiler_end_and_print_session(&g_ctx.temporary_allocator);
}

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  printf("=== Video Renderer Daemon (Unix Socket) ===\n");
  printf("Resolution: %dx%d, Max frames: %d\n", FRAME_WIDTH, FRAME_HEIGHT,
         MAX_FRAMES);
  printf("Socket path: %s\n", SOCKET_PATH);
  fflush(stdout);

  profiler_begin_session();
  // Initialize context and memory allocators first
  if (init_context(&g_ctx) < 0) {
    fprintf(stderr, "Failed to initialize context\n");
    return 1;
  }

  // Initialize system once
  if (initialize_system() < 0) {
    fprintf(stderr, "Failed to initialize system\n");
    return 1;
  }

  pthread_mutex_init(&g_ctx.queue_mutex, NULL);
  pthread_cond_init(&g_ctx.queue_cond, NULL);

  for (int i = 0; i < NUM_TEXTURE_POOLS; i++) {
    atomic_store(&g_ctx.pool_slot_in_use[i], -1);
  }

  profiler_end_and_print_session(&g_ctx.temporary_allocator);

  // Create Unix socket
  int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (server_fd < 0) {
    fprintf(stderr, "Failed to create socket: %s\n", strerror(errno));
    cleanup();
    return 1;
  }

  // Remove existing socket file if it exists
  unlink(SOCKET_PATH);

  // Bind socket
  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

  if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    fprintf(stderr, "Failed to bind socket: %s\n", strerror(errno));
    close(server_fd);
    cleanup();
    return 1;
  }

  // Listen for connections
  if (listen(server_fd, 1) < 0) {
    fprintf(stderr, "Failed to listen on socket: %s\n", strerror(errno));
    close(server_fd);
    cleanup();
    return 1;
  }

  printf("Listening for connections on Unix socket...\n");
  fflush(stdout);

  char input_buffer[INPUT_BUFFER_SIZE];

  // Main daemon loop
  while (1) {
    // Accept client connection
    int client_fd = accept(server_fd, NULL, NULL);
    if (client_fd < 0) {
      fprintf(stderr, "Failed to accept connection: %s\n", strerror(errno));
      continue;
    }

    printf("Client connected\n");
    fflush(stdout);

    // Read request from client
    ssize_t bytes_read = read(client_fd, input_buffer, sizeof(input_buffer) - 1);
    if (bytes_read > 0) {
      input_buffer[bytes_read] = '\0';

      // Remove trailing newline if present
      if (input_buffer[bytes_read - 1] == '\n') {
        input_buffer[bytes_read - 1] = '\0';
      }

      printf("Received request: %s\n", input_buffer);
      fflush(stdout);

      // Process the request
      process_request(client_fd, input_buffer);

      // Reset temporary allocator after each request
      ALLOC_RESET(&g_ctx.temporary_allocator);
    }

    // Close client connection
    close(client_fd);
    printf("Client disconnected\n");
    fflush(stdout);
  }

  // Cleanup (unreachable in normal operation)
  close(server_fd);
  unlink(SOCKET_PATH);
  cleanup();
  return 0;
}

// Assert we haven't exceeded max profile points
PROFILE_ASSERT_END_OF_COMPILATION_UNIT;
