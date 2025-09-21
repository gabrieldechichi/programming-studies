#ifndef H_LIPSYNC_ALGS
#define H_LIPSYNC_ALGS

#include "memory.h"
#include "typedefs.h"

typedef struct {
  f32 *audio_data;
  i32 sample_count;
  i32 start_index;
} LipSyncInput;

typedef struct {
  i32 best_phoneme_index;
  char *best_phoneme_name;
  f32 best_phoneme_score;
  f32 *all_scores;       // Array of scores for all phonemes (optional)
  f32 volume;            // RMS volume level
  bool32 has_new_result; // True if this is a new result since last query
} LipSyncResult;

typedef enum {
  COMPARE_METHOD_L1_NORM = 0,
  COMPARE_METHOD_L2_NORM = 1,
  COMPARE_METHOD_COSINE_SIMILARITY = 2,
} CompareMethod;

#define MAX_PHONEME_NAME_LENGTH 8
#define MAX_CALIBRATION_SAMPLES 10
#define MAX_MFCC_COEFFICIENTS 12
#define MAX_PHONEME_GROUPS 40

typedef struct {
  f32 array[MAX_MFCC_COEFFICIENTS];
} MfccCalibrationData;

typedef struct {
  char name[MAX_PHONEME_NAME_LENGTH];
  MfccCalibrationData mfcc_calibration_data_list[MAX_CALIBRATION_SAMPLES];
  i32 calibration_data_count;
} MfccData;

typedef struct {
  i32 mfcc_num;
  i32 mfcc_data_count;
  i32 mel_filter_bank_channels;
  i32 target_sample_rate;
  i32 sample_count;
  b32 use_standardization;
  i32 compare_method;
  MfccData mfccs[MAX_PHONEME_GROUPS];
  i32 mfcc_count;
  f32 means[MAX_MFCC_COEFFICIENTS];
  f32 standard_deviations[MAX_MFCC_COEFFICIENTS];
} LipSyncProfile;

f32 lipsync_get_rms_volume(f32 *audio_data, i32 sample_count);

void lipsync_copy_ring_buffer(f32 *input, f32 *output, i32 length,
                              i32 start_index);
void lipsync_low_pass_filter(f32 *data, i32 length, f32 sample_rate, f32 cutoff,
                             f32 range, Allocator *temp_arena);
void lipsync_pre_emphasis(f32 *data, i32 length, f32 factor);
void lipsync_hamming_window(f32 *data, i32 length);
void lipsync_normalize(f32 *data, i32 length, f32 max_value);
void lipsync_downsample(f32 *input, f32 *output, i32 input_length,
                        i32 input_sample_rate, i32 target_sample_rate,
                        i32 *output_length);

void lipsync_fft(f32 *input_data, f32 *output_spectrum, i32 length,
                 Allocator *temp_arena);

void lipsync_mel_filter_bank(f32 *spectrum, f32 *mel_spectrum,
                             i32 spectrum_length, f32 sample_rate,
                             i32 mel_channels);
void lipsync_power_to_db(f32 *data, i32 length);
void lipsync_dct(f32 *mel_spectrum, f32 *mfcc_output, i32 length);
void lipsync_extract_mfcc(f32 *spectrum, f32 *mfcc_output, i32 spectrum_length,
                          f32 sample_rate, i32 mel_channels, i32 mfcc_count);

f32 lipsync_calc_phoneme_score(f32 *mfcc, LipSyncProfile *profile,
                               i32 phoneme_index, CompareMethod method);
i32 lipsync_recognize_phoneme(f32 *mfcc, LipSyncProfile *profile,
                              f32 *scores_output);
void lipsync_get_phoneme_averages(LipSyncProfile *profile, i32 phoneme_index,
                                  f32 *averages_output);

void lipsync_convert_profile_to_unity_format(LipSyncProfile *profile,
                                             f32 *flat_phoneme_array);

f32 lipsync_calc_phoneme_score_unity(f32 *mfcc, f32 *flat_phoneme_array,
                                     LipSyncProfile *profile, i32 phoneme_index,
                                     CompareMethod method);
i32 lipsync_recognize_phoneme_unity(f32 *mfcc, f32 *flat_phoneme_array,
                                    LipSyncProfile *profile,
                                    f32 *scores_output);

b32 lipsync_profile_write(const LipSyncProfile *profile, Allocator *allocator,
                          _out_ u8 **buffer, _out_ u32 *buffer_size);

LipSyncProfile *lipsync_profile_read(const uint8 *binary_data, u32 data_len,
                                     Allocator *allocator);

#endif