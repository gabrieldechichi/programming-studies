#ifndef H_LIPSYNC
#define H_LIPSYNC

#include "context.h"
#include "lipsync_algs.h"
#include "memory.h"
#include "typedefs.h"

// Core lipsync context - manages internal state and buffers
typedef struct {
  // Audio configuration
  i32 sample_rate;
  LipSyncProfile *profile;

  // Ring buffer for Unity-compatible processing (LEFT CHANNEL ONLY)
  f32 *ring_buffer;
  i32 ring_buffer_size;
  i32 ring_buffer_index;

  // Unity-compatible pre-averaged phoneme data
  f32 *unity_phoneme_array;

  // Processing state
  bool32 is_data_received;
  i32 lipsync_frame_count;

  // Current results
  LipSyncResult current_result;
  f32 *phoneme_scores; // Persistent storage for scores
} LipSyncContext;

// Initialization - creates and configures the lipsync system
LipSyncContext lipsync_init(Allocator *allocator, i32 sample_rate,
                            LipSyncProfile *profile);

// Feed audio samples (mono audio) - call whenever you have new audio data
void lipsync_feed_audio(LipSyncContext *lipsync, GameContext *ctx, f32 *samples,
                        i32 sample_count, i32 channel_count);

// Process accumulated audio data - returns true if new results are available
bool32 lipsync_process(LipSyncContext *ctx, GameContext *game_ctx);

// Get current phoneme detection results
LipSyncResult lipsync_get_result(LipSyncContext *ctx);

// Get current volume level (convenience function)
f32 lipsync_get_volume(LipSyncContext *ctx);

#endif