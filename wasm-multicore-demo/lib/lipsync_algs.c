#include "lipsync_algs.h"
#include "os/os.h"
#include "math.h"
#include "serialization.h"

f32 lipsync_get_rms_volume(f32 *audio_data, i32 sample_count) {
  if (!audio_data || sample_count <= 0) {
    return 0.0f;
  }

  f32 sum = 0.0f;
  for (i32 i = 0; i < sample_count; i++) {
    f32 sample = audio_data[i];
    sum += sample * sample;
  }

  f32 average = sum / (f32)sample_count;
  return sqrtf(average);
}

f32 lipsync_get_max_value(f32 *data, i32 length) {
  f32 max_val = 0.0f;
  for (i32 i = 0; i < length; i++) {
    f32 abs_val = fabsf(data[i]);
    if (abs_val > max_val) {
      max_val = abs_val;
    }
  }
  return max_val;
}

void lipsync_copy_ring_buffer(f32 *input, f32 *output, i32 length,
                              i32 start_index) {
  if (!input || !output || length <= 0) {
    return;
  }

  for (i32 i = 0; i < length; i++) {
    output[i] = input[(start_index + i) % length];
  }
}

void lipsync_low_pass_filter(f32 *data, i32 length, f32 sample_rate, f32 cutoff,
                             f32 range, Allocator *temp_arena) {
  if (!data || length <= 0 || !temp_arena) {
    return;
  }

  // Unity's exact calculation
  cutoff = (cutoff - range) / sample_rate;
  range /= sample_rate;

  // Create temporary copy of input data
  f32 *tmp = ALLOC_ARRAY(temp_arena, f32, length);
  for (i32 i = 0; i < length; i++) {
    tmp[i] = data[i];
  }

  // Calculate filter length (Unity formula)
  i32 n = (i32)roundf(3.1f / range);
  if ((n + 1) % 2 == 0)
    n += 1; // Ensure odd length

  // Allocate filter coefficients
  f32 *b = ALLOC_ARRAY(temp_arena, f32, n);

  // Generate sinc-based FIR filter coefficients (Unity implementation)
  for (i32 i = 0; i < n; i++) {
    f32 x = i - (n - 1) / 2.0f;
    f32 ang = 2.0f * PI * cutoff * x;

    if (fabsf(ang) < EPSILON) {
      // Handle sinc(0) = 1 case
      b[i] = 2.0f * cutoff;
    } else {
      b[i] = 2.0f * cutoff * sinf(ang) / ang;
    }
  }

  // NOTE: Unity does NOT clear the output buffer here!
  // Unity's implementation accumulates directly into the original data buffer
  // Removed the clearing step to match Unity's exact behavior

  // Apply FIR filter (convolution)
  for (i32 i = 0; i < length; i++) {
    for (i32 j = 0; j < n; j++) {
      if (i - j >= 0) {
        data[i] += b[j] * tmp[i - j];
      }
    }
  }
}

void lipsync_pre_emphasis(f32 *data, i32 length, f32 factor) {
  if (!data || length <= 1) {
    return;
  }

  // Process from end to beginning to avoid overwriting data we need
  for (i32 i = length - 1; i >= 1; i--) {
    data[i] = data[i] - factor * data[i - 1];
  }
  // data[0] remains unchanged
}

void lipsync_hamming_window(f32 *data, i32 length) {
  if (!data || length <= 0) {
    return;
  }

  for (i32 i = 0; i < length; i++) {
    f32 x = (f32)i / (length - 1);
    f32 window = 0.54f - 0.46f * cosf(2.0f * PI * x);
    data[i] *= window;
  }
}

void lipsync_normalize(f32 *data, i32 length, f32 max_value) {
  if (!data || length <= 0) {
    return;
  }

  f32 current_max = lipsync_get_max_value(data, length);
  if (current_max < EPSILON) {
    return;
  }

  f32 scale = max_value / current_max;
  for (i32 i = 0; i < length; i++) {
    data[i] *= scale;
  }
}

void lipsync_downsample(f32 *input, f32 *output, i32 input_length,
                        i32 input_sample_rate, i32 target_sample_rate,
                        i32 *output_length) {
  if (!input || !output || !output_length || input_length <= 0 ||
      input_sample_rate <= 0 || target_sample_rate <= 0) {
    *output_length = 0;
    return;
  }

  if (input_sample_rate <= target_sample_rate) {
    // No downsampling needed, just copy
    for (i32 i = 0; i < input_length; i++) {
      output[i] = input[i];
    }
    *output_length = input_length;
    return;
  }

  if (input_sample_rate % target_sample_rate == 0) {
    // Simple downsampling - take every Nth sample
    i32 skip = input_sample_rate / target_sample_rate;
    *output_length = input_length / skip;

    for (i32 i = 0; i < *output_length; i++) {
      output[i] = input[i * skip];
    }
  } else {
    // Linear interpolation downsampling
    f32 df = (f32)input_sample_rate / target_sample_rate;
    *output_length = (i32)roundf(input_length / df);

    for (i32 j = 0; j < *output_length; j++) {
      f32 f_index = df * j;
      i32 i0 = (i32)floorf(f_index);
      i32 i1 = MIN(i0 + 1, input_length - 1);
      f32 t = f_index - i0;

      f32 x0 = input[i0];
      f32 x1 = input[i1];
      output[j] = x0 + t * (x1 - x0); // Linear interpolation
    }
  }
}

// Internal recursive FFT function
internal void lipsync_fft_internal(f32 *spectrum_re, f32 *spectrum_im, i32 N,
                                   Allocator *temp_arena) {
  if (N < 2)
    return;

  // Allocate temporary arrays for even/odd split
  f32 *even_re = ALLOC_ARRAY(temp_arena, f32, (N / 2));
  f32 *even_im = ALLOC_ARRAY(temp_arena, f32, (N / 2));
  f32 *odd_re = ALLOC_ARRAY(temp_arena, f32, (N / 2));
  f32 *odd_im = ALLOC_ARRAY(temp_arena, f32, (N / 2));

  // Split into even/odd indices
  for (i32 i = 0; i < N / 2; i++) {
    even_re[i] = spectrum_re[i * 2];
    even_im[i] = spectrum_im[i * 2];
    odd_re[i] = spectrum_re[i * 2 + 1];
    odd_im[i] = spectrum_im[i * 2 + 1];
  }

  // Recursive FFT on even/odd parts
  lipsync_fft_internal(even_re, even_im, N / 2, temp_arena);
  lipsync_fft_internal(odd_re, odd_im, N / 2, temp_arena);

  // Combine results
  for (i32 i = 0; i < N / 2; i++) {
    f32 er = even_re[i];
    f32 ei = even_im[i];
    f32 or = odd_re[i];
    f32 oi = odd_im[i];

    f32 theta = -2.0f * PI * i / N;
    f32 cos_theta = cosf(theta);
    f32 sin_theta = sinf(theta);

    // Complex multiplication: (cos + i*sin) * (or + i*oi)
    f32 c_re = cos_theta * or - sin_theta * oi;
    f32 c_im = cos_theta * oi + sin_theta * or;

    // Butterfly operation
    spectrum_re[i] = er + c_re;
    spectrum_im[i] = ei + c_im;
    spectrum_re[N / 2 + i] = er - c_re;
    spectrum_im[N / 2 + i] = ei - c_im;
  }
}

void lipsync_fft(f32 *input_data, f32 *output_spectrum, i32 length,
                 Allocator *temp_arena) {
  if (!input_data || !output_spectrum || length <= 0 || !temp_arena) {
    return;
  }

  // Unity behavior: Use input length directly, no power-of-2 padding
  i32 fft_size = length;

  // Allocate temporary arrays for real and imaginary parts
  f32 *spectrum_re = ALLOC_ARRAY(temp_arena, f32, fft_size);
  f32 *spectrum_im = ALLOC_ARRAY(temp_arena, f32, fft_size);

  // Initialize with input data (real part)
  for (i32 i = 0; i < fft_size; i++) {
    spectrum_re[i] = input_data[i];
    spectrum_im[i] = 0.0f;
  }

  // Perform FFT on exact input size
  lipsync_fft_internal(spectrum_re, spectrum_im, fft_size, temp_arena);

  // Calculate magnitude spectrum for full length (Unity behavior)
  for (i32 i = 0; i < length; i++) {
    f32 re = spectrum_re[i];
    f32 im = spectrum_im[i];
    output_spectrum[i] = sqrtf(re * re + im * im);
  }
}

// =============================================
// Step 5: MFCC Extraction Functions
// =============================================

// Convert Hz to Mel scale
internal f32 hz_to_mel(f32 hz) { return 1127.0f * logf(hz / 700.0f + 1.0f); }

// Convert Mel scale to Hz
internal f32 mel_to_hz(f32 mel) {
  return 700.0f * (expf(mel / 1127.0f) - 1.0f);
}

void lipsync_mel_filter_bank(f32 *spectrum, f32 *mel_spectrum,
                             i32 spectrum_length, f32 sample_rate,
                             i32 mel_channels) {
  if (!spectrum || !mel_spectrum || spectrum_length <= 0 || mel_channels <= 0) {
    return;
  }

  f32 f_max = sample_rate / 2.0f;
  f32 mel_max = hz_to_mel(f_max);
  i32 n_max = spectrum_length / 2; // Only use positive frequencies
  f32 df = f_max / n_max;
  f32 d_mel = mel_max / (mel_channels + 1);

  // Initialize output to zero
  for (i32 n = 0; n < mel_channels; n++) {
    mel_spectrum[n] = 0.0f;
  }

  for (i32 n = 0; n < mel_channels; n++) {
    f32 mel_begin = d_mel * n;
    f32 mel_center = d_mel * (n + 1);
    f32 mel_end = d_mel * (n + 2);

    f32 f_begin = mel_to_hz(mel_begin);
    f32 f_center = mel_to_hz(mel_center);
    f32 f_end = mel_to_hz(mel_end);

    i32 i_begin = (i32)ceilf(f_begin / df);
    i32 i_center = (i32)roundf(f_center / df);
    i32 i_end = (i32)floorf(f_end / df);

    // Clamp indices to valid range
    i_begin = MAX(i_begin, 0);
    i_center = MAX(i_center, 0);
    i_end = MIN(i_end, n_max - 1);

    f32 sum = 0.0f;
    for (i32 i = i_begin + 1; i <= i_end; i++) {
      if (i >= spectrum_length)
        break;

      f32 f = df * i;
      f32 a;

      if (i < i_center) {
        // Rising slope
        a = (f - f_begin) / (f_center - f_begin);
      } else {
        // Falling slope
        a = (f_end - f) / (f_end - f_center);
      }

      // Normalize by filter width
      a /= (f_end - f_begin) * 0.5f;
      sum += a * spectrum[i];
    }
    mel_spectrum[n] = sum;
  }
}

void lipsync_power_to_db(f32 *data, i32 length) {
  if (!data || length <= 0) {
    return;
  }

  for (i32 i = 0; i < length; i++) {
    // Add small epsilon to avoid log(0)
    f32 power = MAX(data[i], 1e-10f);
    data[i] = 10.0f * log10f(power);
  }
}

void lipsync_dct(f32 *mel_spectrum, f32 *mfcc_output, i32 length) {
  if (!mel_spectrum || !mfcc_output || length <= 0) {
    return;
  }

  f32 a = PI / length;
  for (i32 i = 0; i < length; i++) {
    f32 sum = 0.0f;
    for (i32 j = 0; j < length; j++) {
      f32 ang = (j + 0.5f) * i * a;
      sum += mel_spectrum[j] * cosf(ang);
    }
    mfcc_output[i] = sum;
  }
}

void lipsync_extract_mfcc(f32 *spectrum, f32 *mfcc_output, i32 spectrum_length,
                          f32 sample_rate, i32 mel_channels, i32 mfcc_count) {
  if (!spectrum || !mfcc_output || spectrum_length <= 0 || mel_channels <= 0 ||
      mfcc_count <= 0) {
    return;
  }

  // Temporary buffer for mel spectrum (on stack for small sizes)
  f32 mel_spectrum[64]; // Should be enough for typical mel_channels (20-40)
  if (mel_channels > 64) {
    return; // Safety check
  }

  // Step 1: Apply Mel filter bank
  lipsync_mel_filter_bank(spectrum, mel_spectrum, spectrum_length, sample_rate,
                          mel_channels);

  // Step 2: Convert power to dB
  lipsync_power_to_db(mel_spectrum, mel_channels);

  // Step 3: Apply DCT
  f32 mel_cepstrum[64]; // Temp buffer for full DCT output
  if (mel_channels > 64) {
    return; // Safety check
  }

  lipsync_dct(mel_spectrum, mel_cepstrum, mel_channels);

  // Step 4: Extract MFCC coefficients (skip coefficient 0, take 1 to
  // mfcc_count)
  for (i32 i = 0; i < mfcc_count; i++) {
    if (i + 1 < mel_channels) {
      mfcc_output[i] = mel_cepstrum[i + 1]; // Skip mel_cepstrum[0]
    } else {
      mfcc_output[i] = 0.0f;
    }
  }
}

// =============================================
// Step 6: Phoneme Recognition Functions
// =============================================

void lipsync_get_phoneme_averages(LipSyncProfile *profile, i32 phoneme_index,
                                  f32 *averages_output) {
  if (!profile || !averages_output || phoneme_index < 0 ||
      phoneme_index >= profile->mfcc_count) {
    return;
  }

  MfccData *phoneme_data = &profile->mfccs[phoneme_index];

  // Calculate average MFCC values for this phoneme from all calibration samples
  for (i32 i = 0; i < profile->mfcc_num; i++) {
    averages_output[i] = 0.0f;
    for (i32 j = 0; j < phoneme_data->calibration_data_count; j++) {
      averages_output[i] +=
          phoneme_data->mfcc_calibration_data_list[j].array[i];
    }
    averages_output[i] /= (f32)phoneme_data->calibration_data_count;
  }
}

f32 lipsync_calc_phoneme_score(f32 *mfcc, LipSyncProfile *profile,
                               i32 phoneme_index, CompareMethod method) {
  if (!mfcc || !profile || phoneme_index < 0 ||
      phoneme_index >= profile->mfcc_count) {
    return 0.0f;
  }

  // Get average MFCC values for this phoneme
  f32 phoneme_averages[MAX_MFCC_COEFFICIENTS];
  lipsync_get_phoneme_averages(profile, phoneme_index, phoneme_averages);

  switch (method) {
  case COMPARE_METHOD_L1_NORM: {
    // L1 Norm (Manhattan distance)
    f32 distance = 0.0f;
    for (i32 i = 0; i < profile->mfcc_num; i++) {
      f32 x = profile->use_standardization ? (mfcc[i] - profile->means[i]) /
                                                 profile->standard_deviations[i]
                                           : mfcc[i];
      f32 y = profile->use_standardization
                  ? (phoneme_averages[i] - profile->means[i]) /
                        profile->standard_deviations[i]
                  : phoneme_averages[i];
      distance += fabsf(x - y);
    }
    distance /= (f32)profile->mfcc_num;
    return powf(10.0f, -distance);
  }

  case COMPARE_METHOD_L2_NORM: {
    // L2 Norm (Euclidean distance)
    f32 distance = 0.0f;
    for (i32 i = 0; i < profile->mfcc_num; i++) {
      f32 x = profile->use_standardization ? (mfcc[i] - profile->means[i]) /
                                                 profile->standard_deviations[i]
                                           : mfcc[i];
      f32 y = profile->use_standardization
                  ? (phoneme_averages[i] - profile->means[i]) /
                        profile->standard_deviations[i]
                  : phoneme_averages[i];
      f32 diff = x - y;
      distance += diff * diff;
    }
    distance = sqrtf(distance / (f32)profile->mfcc_num);
    return powf(10.0f, -distance);
  }

  case COMPARE_METHOD_COSINE_SIMILARITY: {
    // Cosine similarity
    f32 dot_product = 0.0f;
    f32 mfcc_norm = 0.0f;
    f32 phoneme_norm = 0.0f;

    for (i32 i = 0; i < profile->mfcc_num; i++) {
      f32 x = profile->use_standardization ? (mfcc[i] - profile->means[i]) /
                                                 profile->standard_deviations[i]
                                           : mfcc[i];
      f32 y = profile->use_standardization
                  ? (phoneme_averages[i] - profile->means[i]) /
                        profile->standard_deviations[i]
                  : phoneme_averages[i];

      dot_product += x * y;
      mfcc_norm += x * x;
      phoneme_norm += y * y;
    }

    mfcc_norm = sqrtf(mfcc_norm);
    phoneme_norm = sqrtf(phoneme_norm);

    if (mfcc_norm < EPSILON || phoneme_norm < EPSILON) {
      return 0.0f;
    }

    f32 similarity = dot_product / (mfcc_norm * phoneme_norm);
    similarity = MAX(similarity, 0.0f); // Clamp to positive
    return powf(similarity, 100.0f); // High exponent like Unity implementation
  }

  default:
    return 0.0f;
  }
}

i32 lipsync_recognize_phoneme(f32 *mfcc, LipSyncProfile *profile,
                              f32 *scores_output) {
  if (!mfcc || !profile || !scores_output) {
    return -1;
  }

  CompareMethod method = (CompareMethod)profile->compare_method;
  f32 sum = 0.0f;

  // Calculate raw scores for each phoneme
  for (i32 i = 0; i < profile->mfcc_count; i++) {
    scores_output[i] = lipsync_calc_phoneme_score(mfcc, profile, i, method);
    sum += scores_output[i];
  }

  // Normalize scores to sum to 1.0
  if (sum > EPSILON) {
    for (i32 i = 0; i < profile->mfcc_count; i++) {
      scores_output[i] = scores_output[i] / sum;
    }
  } else {
    // If all scores are zero, distribute equally
    for (i32 i = 0; i < profile->mfcc_count; i++) {
      scores_output[i] = 1.0f / (f32)profile->mfcc_count;
    }
  }

  // Find phoneme with highest score
  i32 best_phoneme_index = -1;
  f32 max_score = -1.0f;
  for (i32 i = 0; i < profile->mfcc_count; i++) {
    if (scores_output[i] > max_score) {
      max_score = scores_output[i];
      best_phoneme_index = i;
    }
  }

  return best_phoneme_index;
}

// =============================================
// Unity Compatibility Functions
// =============================================

void lipsync_convert_profile_to_unity_format(LipSyncProfile *profile,
                                             f32 *flat_phoneme_array) {
  if (!profile || !flat_phoneme_array) {
    return;
  }

  // Convert calibration data to pre-averaged flat array (Unity's
  // Profile.cs:73-86 behavior)
  i32 array_index = 0;

  for (i32 phoneme_idx = 0; phoneme_idx < profile->mfcc_count; phoneme_idx++) {
    MfccData *phoneme_data = &profile->mfccs[phoneme_idx];

    // Average each MFCC coefficient across all calibration samples (Unity's
    // UpdateNativeArray)
    for (i32 mfcc_idx = 0; mfcc_idx < profile->mfcc_num; mfcc_idx++) {
      f32 sum = 0.0f;

      // Sum all calibration samples for this coefficient
      for (i32 calib_idx = 0; calib_idx < phoneme_data->calibration_data_count;
           calib_idx++) {
        sum +=
            phoneme_data->mfcc_calibration_data_list[calib_idx].array[mfcc_idx];
      }

      // Calculate average and store in flat array
      if (phoneme_data->calibration_data_count > 0) {
        flat_phoneme_array[array_index] =
            sum / (f32)phoneme_data->calibration_data_count;
      } else {
        flat_phoneme_array[array_index] = 0.0f;
      }

      array_index++;
    }
  }
}

f32 lipsync_calc_phoneme_score_unity(f32 *mfcc, f32 *flat_phoneme_array,
                                     LipSyncProfile *profile, i32 phoneme_index,
                                     CompareMethod method) {
  if (!mfcc || !flat_phoneme_array || !profile || phoneme_index < 0 ||
      phoneme_index >= profile->mfcc_count) {
    return 0.0f;
  }

  // Access pre-averaged phoneme data from flat array (Unity's LipSyncJob.cs
  // behavior)
  f32 *phoneme_averages =
      &flat_phoneme_array[phoneme_index * profile->mfcc_num];

  // Debug: Log phoneme data for first phoneme only to avoid spam
  if (phoneme_index == 0) {
    LOG_INFO("C Phoneme[0] Avg[0-3]: [%, %, %, %], UseStd: %",
             FMT_FLOAT(phoneme_averages[0]), FMT_FLOAT(phoneme_averages[1]),
             FMT_FLOAT(phoneme_averages[2]), FMT_FLOAT(phoneme_averages[3]),
             FMT_INT(profile->use_standardization));
  }

  switch (method) {
  case COMPARE_METHOD_L1_NORM: {
    // L1 Norm (Manhattan distance) - Unity's CalcL1NormScore
    f32 distance = 0.0f;
    for (i32 i = 0; i < profile->mfcc_num; i++) {
      f32 x = profile->use_standardization ? (mfcc[i] - profile->means[i]) /
                                                 profile->standard_deviations[i]
                                           : mfcc[i];
      f32 y = profile->use_standardization
                  ? (phoneme_averages[i] - profile->means[i]) /
                        profile->standard_deviations[i]
                  : phoneme_averages[i];
      distance += fabsf(x - y);
    }
    distance /= (f32)profile->mfcc_num;
    return powf(10.0f, -distance);
  }

  case COMPARE_METHOD_L2_NORM: {
    // L2 Norm (Euclidean distance) - Unity's CalcL2NormScore
    f32 distance = 0.0f;
    for (i32 i = 0; i < profile->mfcc_num; i++) {
      f32 x = profile->use_standardization ? (mfcc[i] - profile->means[i]) /
                                                 profile->standard_deviations[i]
                                           : mfcc[i];
      f32 y = profile->use_standardization
                  ? (phoneme_averages[i] - profile->means[i]) /
                        profile->standard_deviations[i]
                  : phoneme_averages[i];
      f32 diff = x - y;
      distance += diff * diff;
    }
    distance = sqrtf(distance / (f32)profile->mfcc_num);
    return powf(10.0f, -distance);
  }

  case COMPARE_METHOD_COSINE_SIMILARITY: {
    // Cosine similarity - Unity's CalcCosineSimilarityScore
    f32 dot_product = 0.0f;
    f32 mfcc_norm = 0.0f;
    f32 phoneme_norm = 0.0f;

    for (i32 i = 0; i < profile->mfcc_num; i++) {
      f32 x = profile->use_standardization ? (mfcc[i] - profile->means[i]) /
                                                 profile->standard_deviations[i]
                                           : mfcc[i];
      f32 y = profile->use_standardization
                  ? (phoneme_averages[i] - profile->means[i]) /
                        profile->standard_deviations[i]
                  : phoneme_averages[i];

      dot_product += x * y;
      mfcc_norm += x * x;
      phoneme_norm += y * y;
    }

    mfcc_norm = sqrtf(mfcc_norm);
    phoneme_norm = sqrtf(phoneme_norm);

    if (mfcc_norm < EPSILON || phoneme_norm < EPSILON) {
      return 0.0f;
    }

    f32 similarity = dot_product / (mfcc_norm * phoneme_norm);
    similarity = MAX(similarity, 0.0f); // Clamp to positive
    return powf(similarity, 100.0f); // High exponent like Unity implementation
  }

  default:
    return 0.0f;
  }
}

i32 lipsync_recognize_phoneme_unity(f32 *mfcc, f32 *flat_phoneme_array,
                                    LipSyncProfile *profile,
                                    f32 *scores_output) {
  if (!mfcc || !flat_phoneme_array || !profile || !scores_output) {
    return -1;
  }

  CompareMethod method = (CompareMethod)profile->compare_method;
  f32 sum = 0.0f;

  // Calculate raw scores for each phoneme using flat array (Unity's CalcScores
  // behavior)
  for (i32 i = 0; i < profile->mfcc_count; i++) {
    scores_output[i] = lipsync_calc_phoneme_score_unity(
        mfcc, flat_phoneme_array, profile, i, method);
    sum += scores_output[i];
  }

  // Debug: Log first few raw scores and sum
  LOG_INFO("C Raw Scores[0-4]: [%, %, %, %, %], Sum: %, Method: %, Count: %",
           FMT_FLOAT(scores_output[0]), FMT_FLOAT(scores_output[1]),
           FMT_FLOAT(scores_output[2]), FMT_FLOAT(scores_output[3]),
           FMT_FLOAT(scores_output[4]), FMT_FLOAT(sum), FMT_INT(method),
           FMT_INT(profile->mfcc_count));

  // Normalize scores to sum to 1.0 (Unity's CalcScores normalization)
  if (sum > EPSILON) {
    for (i32 i = 0; i < profile->mfcc_count; i++) {
      scores_output[i] = scores_output[i] / sum;
    }
  } else {
    // If all scores are zero, distribute equally
    for (i32 i = 0; i < profile->mfcc_count; i++) {
      scores_output[i] = 1.0f / (f32)profile->mfcc_count;
    }
  }

  // Find phoneme with highest score (Unity's GetVowel behavior)
  i32 best_phoneme_index = -1;
  f32 max_score = -1.0f;
  for (i32 i = 0; i < profile->mfcc_count; i++) {
    if (scores_output[i] > max_score) {
      max_score = scores_output[i];
      best_phoneme_index = i;
    }
  }

  return best_phoneme_index;
}

b32 lipsync_profile_write(const LipSyncProfile *profile, Allocator *allocator,
                          _out_ u8 **buffer, _out_ size_t *buffer_size) {
  if (!profile || !allocator) {
    return false;
  }

  // calculate total size
  size_t total_size = 0;
  total_size += sizeof(i32); // mfcc_num
  total_size += sizeof(i32); // mfcc_data_count
  total_size += sizeof(i32); // mel_filter_bank_channels
  total_size += sizeof(i32); // target_sample_rate
  total_size += sizeof(i32); // sample_count
  total_size += sizeof(b32); // use_standardization
  total_size += sizeof(i32); // compare_method
  total_size += sizeof(i32); // mfcc_count

  // mfcc data arrays
  for (i32 i = 0; i < profile->mfcc_count; i++) {
    total_size += MAX_PHONEME_NAME_LENGTH; // name
    total_size += sizeof(i32);             // calibration_data_count
    total_size += sizeof(f32) * MAX_MFCC_COEFFICIENTS *
                  profile->mfccs[i].calibration_data_count; // calibration data
  }

  // means and standard_deviations arrays
  total_size += sizeof(f32) * MAX_MFCC_COEFFICIENTS; // means
  total_size += sizeof(f32) * MAX_MFCC_COEFFICIENTS; // standard_deviations

  BinaryWriter writer = {.cur_offset = 0,
                         .len = total_size,
                         .bytes = ALLOC_ARRAY(allocator, u8, total_size)};
  if (!writer.bytes) {
    return false;
  }

  // write header fields
  write_i32(&writer, profile->mfcc_num);
  write_i32(&writer, profile->mfcc_data_count);
  write_i32(&writer, profile->mel_filter_bank_channels);
  write_i32(&writer, profile->target_sample_rate);
  write_i32(&writer, profile->sample_count);
  write_u32(&writer, profile->use_standardization);
  write_i32(&writer, profile->compare_method);
  write_i32(&writer, profile->mfcc_count);

  // write mfcc data
  for (i32 i = 0; i < profile->mfcc_count; i++) {
    const MfccData *mfcc_data = &profile->mfccs[i];

    // write name
    write_u8(&writer, (u8 *)mfcc_data->name, MAX_PHONEME_NAME_LENGTH);

    // write calibration_data_count
    write_i32(&writer, mfcc_data->calibration_data_count);

    // write calibration data
    for (i32 j = 0; j < mfcc_data->calibration_data_count; j++) {
      write_f32_array(&writer,
                      (f32 *)mfcc_data->mfcc_calibration_data_list[j].array,
                      MAX_MFCC_COEFFICIENTS);
    }
  }

  // write means and standard_deviations
  write_f32_array(&writer, (f32 *)profile->means, MAX_MFCC_COEFFICIENTS);
  write_f32_array(&writer, (f32 *)profile->standard_deviations,
                  MAX_MFCC_COEFFICIENTS);

  *buffer = writer.bytes;
  *buffer_size = writer.len;
  return true;
}

LipSyncProfile *lipsync_profile_read(const uint8 *binary_data, u32 data_len,
                                     Allocator *allocator) {
  if (binary_data == NULL || data_len == 0 || allocator == NULL) {
    return NULL;
  }

  BinaryReader reader = {
      .bytes = binary_data, .len = data_len, .cur_offset = 0};

  LipSyncProfile *profile = ALLOC(allocator, LipSyncProfile);
  if (profile == NULL) {
    return NULL;
  }

  // read header fields
  if (!read_i32(&reader, &profile->mfcc_num) ||
      !read_i32(&reader, &profile->mfcc_data_count) ||
      !read_i32(&reader, &profile->mel_filter_bank_channels) ||
      !read_i32(&reader, &profile->target_sample_rate) ||
      !read_i32(&reader, &profile->sample_count) ||
      !read_u32(&reader, (u32 *)&profile->use_standardization) ||
      !read_i32(&reader, &profile->compare_method) ||
      !read_i32(&reader, &profile->mfcc_count)) {
    return NULL;
  }

  // validate bounds
  if (profile->mfcc_count > MAX_PHONEME_GROUPS) {
    return NULL;
  }

  // read mfcc data
  for (i32 i = 0; i < profile->mfcc_count; i++) {
    MfccData *mfcc_data = &profile->mfccs[i];

    // read name
    if (!read_u8_array(&reader, (u8 *)mfcc_data->name,
                       MAX_PHONEME_NAME_LENGTH)) {
      return NULL;
    }

    // read calibration_data_count
    if (!read_i32(&reader, &mfcc_data->calibration_data_count)) {
      return NULL;
    }

    // validate bounds
    if (mfcc_data->calibration_data_count > MAX_CALIBRATION_SAMPLES) {
      return NULL;
    }

    // read calibration data
    for (i32 j = 0; j < mfcc_data->calibration_data_count; j++) {
      if (!read_f32_array(&reader,
                          mfcc_data->mfcc_calibration_data_list[j].array,
                          MAX_MFCC_COEFFICIENTS)) {
        return NULL;
      }
    }
  }

  // read means and standard_deviations
  if (!read_f32_array(&reader, profile->means, MAX_MFCC_COEFFICIENTS) ||
      !read_f32_array(&reader, profile->standard_deviations,
                      MAX_MFCC_COEFFICIENTS)) {
    return NULL;
  }

  return profile;
}