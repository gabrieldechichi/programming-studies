#ifndef H_AUDIO
#define H_AUDIO

#include "array.h"
#include "context.h"
#include "lib/common.h"
#include "lib/memory.h"
#include "typedefs.h"

// WAV file structures
typedef struct {
  u8 riff[4];    // "RIFF"
  u32 file_size; // File size - 8
  u8 wave[4];    // "WAVE"
} WavRiffHeader;

typedef struct {
  u16 audio_format;    // Audio format (1 for PCM)
  u16 channels;        // Number of channels
  u32 sample_rate;     // Sample rate
  u32 byte_rate;       // Byte rate
  u16 block_align;     // Block align
  u16 bits_per_sample; // Bits per sample
} WavFormatData;

typedef struct {
  WavFormatData format;
  i16 *audio_data;   // 16-bit PCM data
  u32 data_size;     // Data size in bytes
  u32 total_samples; // Total samples (not bytes)
  b32 is_loaded;
} WavFile;

typedef struct {
  WavFile *wav_file;
  f32 playback_position;
  b32 is_playing;
  f32 volume;
  f32 sample_rate_ratio;
  b32 loop;
} AudioClip;

slice_define(AudioClip);

// Streaming audio structures
typedef struct {
  u8 *buffer;
  u32 capacity;
  u32 write_pos;
  u32 read_pos;
  b32 is_complete;
} StreamingBuffer;

typedef struct {
  StreamingBuffer pcm_buffer;
  u32 source_sample_rate;
  u32 channels;          // 1 for mono
  f32 playback_position; // in source samples
  f32 sample_rate_ratio; // conversion ratio
  f32 volume;
  b32 is_playing;
  b32 loop;
} StreamingAudioClip;

slice_define(StreamingAudioClip);

typedef struct {
  i32 output_sample_rate;
  i32 output_channels;
  u32 max_samples_per_frame;
  // todo: use slice here
  f32 *sample_buffer;
  u32 sample_buffer_len;
  AudioClip_Slice clips;
  StreamingAudioClip_Slice streaming_clips;
} AudioState;

#define pcm16_to_float(sample) ((f32)(sample) / 32768.0f)
#define float_to_pcm16(f) ((i16)((f) * 32767.0f))

// WAV file functions
b32 wav_parse_header(u8 *file_data, u32 file_size, WavFile *wav);
void wav_get_sample(WavFile *wav, f32 position, f32 *left, f32 *right);

// WAV file writing functions
u32 wav_calculate_file_size(WavFile *wav);
u32 wav_write_file(WavFile *wav, u8 *buffer, u32 buffer_size);
u8_Array wav_write_file_alloc(WavFile *wav, Allocator *allocator);

WavFile create_wav_from_samples(i16 *samples, u32 samples_len, u32 sample_rate);
WavFile *create_wav_from_samples_alloc(i16 *samples, u32 samples_len,
                                       u32 sample_rate, Allocator *allocator);

// Audio API
AudioState audio_init(GameContext *ctx);
void audio_update(AudioState *state, GameContext *ctx, f32 dt);
void audio_play_clip(AudioState *state, AudioClip clip);

// Streaming audio API
StreamingAudioClip streaming_clip_create(u32 source_sample_rate, u32 channels,
                                         u32 buffer_capacity, GameContext *ctx);
void streaming_clip_write_pcm(StreamingAudioClip *clip, u8 *pcm_data, u32 size);
void streaming_clip_mark_complete(StreamingAudioClip *clip);
b32 streaming_clip_get_sample(StreamingAudioClip *clip, f32 position, f32 *left,
                              f32 *right);
void streaming_clip_reset(StreamingAudioClip *clip);
StreamingAudioClip *audio_play_streaming_clip(AudioState *state,
                                              StreamingAudioClip clip);
b32 streaming_clip_has_audio_content(StreamingAudioClip *clip);

u32 streaming_buffer_available_data_len(StreamingBuffer *buffer);

u32 streaming_buffer_available_space(StreamingBuffer *buffer);

#endif
