/*
    audio.h - Audio playback system

    OVERVIEW

    --- AudioState manages all audio playback (init with audio_init, update each frame)

    --- AudioClip: play WAV files loaded in memory

    --- StreamingAudioClip: stream audio data progressively (for large files or network streams)

    --- supports volume, looping, sample rate conversion

    USAGE
        AudioState audio = audio_init(ctx);

        AudioClip clip = {.wav_file = &wav, .loop = false, .volume = 1.0};
        audio_play_clip(&audio, clip);

        StreamingAudioClip stream = streaming_clip_create(24000, 1, buffer_size, ctx);
        audio_play_streaming_clip(&audio, stream);
        streaming_clip_write_pcm(&stream, pcm_data, len);

        audio_update(&audio, ctx, dt);
*/

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

/* loaded WAV file data (16-bit PCM) */
typedef struct {
  WavFormatData format;
  i16 *audio_data;
  u32 data_size;
  u32 total_samples;
  b32 is_loaded;
} WavFile;

/* audio clip playing from WAV file in memory */
typedef struct {
  WavFile *wav_file;
  f32 playback_position;
  b32 is_playing;
  f32 volume;
  f32 sample_rate_ratio;
  b32 loop;
} AudioClip;
arr_define(AudioClip);

/* ring buffer for streaming audio data */
typedef struct {
  u8 *buffer;
  u32 capacity;
  u32 write_pos;
  u32 read_pos;
  b32 is_complete;
} StreamingBuffer;

/* audio clip streaming data progressively */
typedef struct {
  StreamingBuffer pcm_buffer;
  u32 source_sample_rate;
  u32 channels;
  f32 playback_position;
  f32 sample_rate_ratio;
  f32 volume;
  b32 is_playing;
  b32 paused;
  b32 loop;
} StreamingAudioClip;

arr_define(StreamingAudioClip);

/* main audio state managing all clips and output */
typedef struct {
  i32 output_sample_rate;
  i32 output_channels;
  f32 *sample_buffer;
  u32 sample_buffer_len;
  AudioClip_DynArray clips;
  StreamingAudioClip_DynArray streaming_clips;
} AudioState;

/* convert PCM16 sample to float [-1, 1] */
#define pcm16_to_float(sample) ((f32)(sample) / 32768.0f)
/* convert float [-1, 1] to PCM16 sample */
#define float_to_pcm16(f) ((i16)((f) * 32767.0f))

/* parse WAV file header and data from buffer */
b32 wav_parse_header(u8 *file_data, u32 file_size, WavFile *wav);
/* get interpolated stereo sample at position */
void wav_get_sample(WavFile *wav, f32 position, f32 *left, f32 *right);

/* calculate required buffer size for WAV file */
u32 wav_calculate_file_size(WavFile *wav);
/* write WAV file to buffer, returns bytes written */
u32 wav_write_file(WavFile *wav, u8 *buffer, u32 buffer_size);
u8_Array wav_write_file_alloc(WavFile *wav, Allocator *allocator);

/* create WAV file from PCM16 samples */
WavFile create_wav_from_samples(i16 *samples, u32 samples_len, u32 sample_rate);
WavFile *create_wav_from_samples_alloc(i16 *samples, u32 samples_len,
                                       u32 sample_rate, Allocator *allocator);

/* initialize audio system */
AudioState audio_init(AppContext *ctx);
/* update audio system, mix all clips, call each frame */
void audio_update(AudioState *state, AppContext *ctx, f32 dt);
/* start playing audio clip */
void audio_play_clip(AudioState *state, AudioClip clip);

/* create streaming audio clip with ring buffer */
StreamingAudioClip streaming_clip_create(u32 source_sample_rate, u32 channels,
                                         u32 buffer_capacity, AppContext *ctx);
/* write PCM data to streaming clip buffer */
void streaming_clip_write_pcm(StreamingAudioClip *clip, u8 *pcm_data, u32 size);
/* mark streaming clip as complete (no more data coming) */
void streaming_clip_mark_complete(StreamingAudioClip *clip);
/* get interpolated sample from streaming clip */
b32 streaming_clip_get_sample(StreamingAudioClip *clip, f32 position, f32 *left,
                              f32 *right);
/* reset streaming clip to start */
void streaming_clip_reset(StreamingAudioClip *clip);
/* start playing streaming clip, returns pointer to clip in audio state */
StreamingAudioClip *audio_play_streaming_clip(AudioState *state,
                                              StreamingAudioClip clip);
/* check if streaming clip has any audio data */
b32 streaming_clip_has_audio_content(StreamingAudioClip *clip);

/* get bytes available to read in streaming buffer */
u32 streaming_buffer_available_data_len(StreamingBuffer *buffer);

/* get bytes available to write in streaming buffer */
u32 streaming_buffer_available_space(StreamingBuffer *buffer);

#endif
