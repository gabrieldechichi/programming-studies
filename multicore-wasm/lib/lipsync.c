#include "lipsync.h"
#include "assert.h"
#include "lipsync_algs.h"
#include "math.h"

LipSyncContext lipsync_init(Allocator *allocator, i32 sample_rate,
                            LipSyncProfile *profile) {
  LipSyncContext ctx_s = {0};
  LipSyncContext *ctx = &ctx_s;

  ctx->sample_rate = sample_rate;
  ctx->profile = profile;

  ctx->ring_buffer_size = 3072; // Match Unity's fixed size

  ctx->ring_buffer = ALLOC_ARRAY(allocator, f32, ctx->ring_buffer_size);
  ctx->ring_buffer_index = 0;

  // Initialize Unity-compatible phoneme array
  i32 phoneme_array_size = profile->mfcc_count * profile->mfcc_num;
  ctx->unity_phoneme_array = ALLOC_ARRAY(allocator, f32, phoneme_array_size);

  // Convert profile data to Unity format (pre-averaged flat array)
  lipsync_convert_profile_to_unity_format(profile, ctx->unity_phoneme_array);

  // Initialize processing state
  ctx->is_data_received = false;
  ctx->lipsync_frame_count = 0;

  // Initialize persistent storage for scores
  ctx->phoneme_scores = ALLOC_ARRAY(allocator, f32, profile->mfcc_count);

  // Initialize current result
  ctx->current_result.best_phoneme_index = -1;
  ctx->current_result.best_phoneme_name = NULL;
  ctx->current_result.best_phoneme_score = 0.0f;
  ctx->current_result.all_scores = ctx->phoneme_scores;
  ctx->current_result.volume = 0.0f;
  ctx->current_result.has_new_result = false;

  return ctx_s;
}

void lipsync_feed_audio(LipSyncContext *lipsync, AppContext *ctx, f32 *samples,
                        i32 sample_count, i32 channel_count) {

  if (!lipsync->ring_buffer) {
    return;
  }

  debug_assert(channel_count == 2 || channel_count == 1);
  // make sure we are either 1 or 2 for channels
  channel_count = channel_count == 2 ? 2 : 1;

  f32 *left_channel_samples =
      ALLOC_ARRAY(&ctx->temp_allocator, f32, sample_count);
  for (i32 i = 0; i < sample_count; i++) {
    left_channel_samples[i] =
        samples[i * channel_count]; // Extract left channel
  }

  for (i32 i = 0; i < sample_count; i++) {
    lipsync->ring_buffer[lipsync->ring_buffer_index] = left_channel_samples[i];
    lipsync->ring_buffer_index =
        (lipsync->ring_buffer_index + 1) % lipsync->ring_buffer_size;
  }

  // Set data received flag (Unity's _isDataReceived = true)
  lipsync->is_data_received = true;
}

bool32 lipsync_process(LipSyncContext *ctx, AppContext *game_ctx) {
  // Only process if we have new data
  if (!ctx->is_data_received) {
    ctx->current_result.has_new_result = false;
    return false;
  }

  ctx->is_data_received = false;
  ctx->lipsync_frame_count++;

  // Calculate RMS volume from ring buffer like Unity
  f32 volume = lipsync_get_rms_volume(ctx->ring_buffer, ctx->ring_buffer_size);

  // =====================
  // UNITY-STYLE PROCESSING (frame rate, triggered by new data)
  // =====================

  i32 target_sample_rate = 16000;

  // Capture the ring buffer index when processing starts (Unity's startIndex
  // behavior)
  i32 start_index = ctx->ring_buffer_index;

  // Step 3a: Copy ring buffer (Unity's exact behavior)
  f32 *buffer =
      ALLOC_ARRAY(&game_ctx->temp_allocator, f32, ctx->ring_buffer_size);
  lipsync_copy_ring_buffer(ctx->ring_buffer, buffer, ctx->ring_buffer_size,
                           start_index);

  // Step 3b: Low-pass filter (Unity parameters: cutoff=8000Hz, range=500Hz)
  f32 cutoff = target_sample_rate / 2.0f; // 8000 Hz
  f32 range = 500.0f;                     // 500 Hz transition band
  lipsync_low_pass_filter(buffer, ctx->ring_buffer_size, ctx->sample_rate,
                          cutoff, range, &game_ctx->temp_allocator);

  // Step 3c: Downsample after low-pass filtering
  i32 max_downsampled_samples =
      (ctx->ring_buffer_size * target_sample_rate) / ctx->sample_rate + 1;
  f32 *downsampled_data =
      ALLOC_ARRAY(&game_ctx->temp_allocator, f32, max_downsampled_samples);
  i32 downsampled_length;

  lipsync_downsample(buffer, downsampled_data, ctx->ring_buffer_size,
                     ctx->sample_rate, target_sample_rate, &downsampled_length);

  // Step 3d: Pre-emphasis filter (0.97 factor like Unity)
  lipsync_pre_emphasis(downsampled_data, downsampled_length, 0.97f);

  // Step 3e: Hamming window
  lipsync_hamming_window(downsampled_data, downsampled_length);

  // Step 3f: Normalize to 1.0
  lipsync_normalize(downsampled_data, downsampled_length, 1.0f);

  // =====================
  // STEP 4: FFT and spectrum analysis
  // =====================

  // =====================
  // STEP 5 & 6: Real MFCC Extraction + Phoneme Recognition
  // =====================

  f32 real_mfcc[12];

  // Extract real MFCC coefficients from the FFT spectrum
  if (downsampled_length >= 4) {
    f32 *spectrum =
        ALLOC_ARRAY(&game_ctx->temp_allocator, f32, downsampled_length);

    lipsync_fft(downsampled_data, spectrum, downsampled_length,
                &game_ctx->temp_allocator);

    // Extract MFCC using the profile settings
    lipsync_extract_mfcc(
        spectrum, real_mfcc, downsampled_length, target_sample_rate,
        ctx->profile->mel_filter_bank_channels, ctx->profile->mfcc_num);
  } else {
    // Fallback: initialize with zeros if not enough data
    for (i32 i = 0; i < 12; i++) {
      real_mfcc[i] = 0.0f;
    }
  }

  // Implement cosine similarity matching Unity's exact algorithm
  f32 *scores =
      ALLOC_ARRAY(&game_ctx->temp_allocator, f32, ctx->profile->mfcc_count);
  f32 sum = 0.0f;

  // Calculate raw cosine similarity scores
  for (i32 p = 0; p < ctx->profile->mfcc_count; p++) {
    f32 mfcc_norm = 0.0f;
    f32 phoneme_norm = 0.0f;
    f32 prod = 0.0f;

    // Get phoneme data from Unity-style flat array
    f32 *phoneme_data = &ctx->unity_phoneme_array[p * 12];

    // Calculate norms and dot product (Unity's exact algorithm)
    for (i32 i = 0; i < 12; i++) {
      f32 x = (real_mfcc[i] - ctx->profile->means[i]) /
              ctx->profile->standard_deviations[i];
      f32 y = (phoneme_data[i] - ctx->profile->means[i]) /
              ctx->profile->standard_deviations[i];
      mfcc_norm += x * x;
      phoneme_norm += y * y;
      prod += x * y;
    }

    mfcc_norm = sqrtf(mfcc_norm);
    phoneme_norm = sqrtf(phoneme_norm);
    f32 similarity = prod / (mfcc_norm * phoneme_norm);
    similarity = MAX(similarity, 0.0f);

    scores[p] = powf(similarity, 100.0f);
    sum += scores[p];
  }

  // Normalize scores (Unity's exact algorithm)
  for (i32 p = 0; p < ctx->profile->mfcc_count; p++) {
    scores[p] = sum > 0.0f ? scores[p] / sum : 0.0f;
    ctx->phoneme_scores[p] = scores[p]; // Store in persistent memory
  }

  // Find best phoneme
  i32 best_phoneme = -1;
  f32 best_score = -1.0f;
  for (i32 p = 0; p < ctx->profile->mfcc_count; p++) {
    if (scores[p] > best_score) {
      best_phoneme = p;
      best_score = scores[p];
    }
  }

  // Update current result
  ctx->current_result.best_phoneme_index = best_phoneme;
  ctx->current_result.best_phoneme_name =
      (best_phoneme >= 0 && best_phoneme < ctx->profile->mfcc_count)
          ? ctx->profile->mfccs[best_phoneme].name
          : NULL;
  ctx->current_result.best_phoneme_score = best_score;
  ctx->current_result.volume = volume;
  ctx->current_result.has_new_result = true;

  return true;
}

LipSyncResult lipsync_get_result(LipSyncContext *ctx) {
  // Mark result as read
  ctx->current_result.has_new_result = false;
  return ctx->current_result;
}

f32 lipsync_get_volume(LipSyncContext *ctx) {
  return ctx->current_result.volume;
}