#include "audio.h"
#include "lib/typedefs.h"
#include "os/os.h"
#include "array.h"
#include "common.h"
#include "fmt.h"
#include "math.h"
#include "memory.h"

u32 streaming_buffer_available_data_len(StreamingBuffer *buffer) {
  return (buffer->write_pos - buffer->read_pos + buffer->capacity) %
         buffer->capacity;
}

u32 streaming_buffer_available_space(StreamingBuffer *buffer) {
  u32 available_data = streaming_buffer_available_data_len(buffer);
  // NOTE: very important to leave 1 byte gap to distinguish full from empty
  return buffer->capacity - available_data - 1;
}

void wav_get_sample(WavFile *wav, f32 position, f32 *left, f32 *right) {
  if (position >= wav->total_samples - 1) {
    *left = *right = 0.0f;
    return;
  }

  u32 sample_index = (u32)position;
  f32 fraction = position - sample_index;

  if (wav->format.channels == 1) {
    // Mono - interpolate and duplicate to stereo
    i16 sample1 = wav->audio_data[sample_index];
    i16 sample2 = (sample_index + 1 < wav->total_samples)
                      ? wav->audio_data[sample_index + 1]
                      : sample1;

    f32 interpolated = lerp((f32)sample1, (f32)sample2, fraction);
    *left = *right = pcm16_to_float(interpolated);

  } else if (wav->format.channels == 2) {
    // Stereo - interpolate each channel
    i16 left1 = wav->audio_data[sample_index * 2];
    i16 right1 = wav->audio_data[sample_index * 2 + 1];

    i16 left2 = left1;
    i16 right2 = right1;
    if (sample_index + 1 < wav->total_samples) {
      left2 = wav->audio_data[(sample_index + 1) * 2];
      right2 = wav->audio_data[(sample_index + 1) * 2 + 1];
    }

    *left = pcm16_to_float(lerp((f32)left1, (f32)left2, fraction));
    *right = pcm16_to_float(lerp((f32)right1, (f32)right2, fraction));
  }
}

AudioState audio_init(AppContext *ctx) {
  AudioState state = {0};
  state.output_sample_rate = os_audio_get_sample_rate();
  state.output_channels = 2;
  state.clips = dyn_arr_new_alloc(&ctx->allocator, AudioClip, 16);
  state.streaming_clips =
      dyn_arr_new_alloc(&ctx->allocator, StreamingAudioClip, 16);
  state.sample_buffer_len = 0;

  os_audio_init();
  return state;
}

void audio_update(AudioState *state, AppContext *ctx, f32 dt) {
    UNUSED(dt);
  i32 samples_needed = os_audio_get_samples_needed();

  if (samples_needed < 0) {
    samples_needed = 0;
  }

  state->sample_buffer_len = samples_needed;

  i32 buffer_size = samples_needed * state->output_channels;
  f32 *audio_samples = ALLOC_ARRAY(&ctx->temp_allocator, f32, buffer_size);

  state->sample_buffer = audio_samples;

  b32 did_write = false;

  // Mix regular WAV clips
  for (i32 clip_idx = state->clips.len - 1; clip_idx >= 0; clip_idx--) {
    AudioClip *clip = &state->clips.items[clip_idx];

    if (!clip->is_playing || !clip->wav_file || !clip->wav_file->is_loaded) {
      arr_remove_swap(state->clips, clip_idx);
      continue;
    }
    did_write = true;

    for (i32 i = 0; i < samples_needed; i++) {
      f32 left_sample = 0.0f;
      f32 right_sample = 0.0f;

      if (clip->playback_position < clip->wav_file->total_samples - 1) {
        wav_get_sample(clip->wav_file, clip->playback_position, &left_sample,
                       &right_sample);

        left_sample *= clip->volume;
        right_sample *= clip->volume;

        audio_samples[i * 2] += left_sample;
        audio_samples[i * 2 + 1] += right_sample;

        clip->playback_position += clip->sample_rate_ratio;
      } else {
        if (clip->loop) {
          clip->playback_position = 0.0f;
        } else {
          clip->is_playing = false;
        }
      }
    }
  }

  // Mix streaming clips
  for (i32 clip_idx = state->streaming_clips.len - 1; clip_idx >= 0;
       clip_idx--) {
    StreamingAudioClip *clip = &state->streaming_clips.items[clip_idx];

    if (!clip->is_playing) {
      continue;
    }

    if (clip->paused) {
      continue;
    }

    for (i32 i = 0; i < samples_needed; i++) {
      f32 left_sample = 0.0f;
      f32 right_sample = 0.0f;

      if (streaming_clip_get_sample(clip, clip->playback_position, &left_sample,
                                    &right_sample)) {
        did_write = true;
        left_sample *= clip->volume;
        right_sample *= clip->volume;

        audio_samples[i * 2] += left_sample;
        audio_samples[i * 2 + 1] += right_sample;

        clip->playback_position += clip->sample_rate_ratio;
      } else if (clip->pcm_buffer.is_complete) {
        // No more data and stream is complete
        if (clip->loop) {
          clip->playback_position = 0.0f;
        } else {
          clip->is_playing = false;
        }
      }
      // If stream is not complete but no data available, just wait
    }

    // Advance read position to free consumed samples
    // Keep samples that might still be needed for interpolation
    u32 bytes_per_sample = sizeof(i16) * clip->channels;
    u32 safe_position = (u32)clip->playback_position;
    if (safe_position > 1) {
      u32 bytes_to_advance = (safe_position)*bytes_per_sample;
      clip->pcm_buffer.read_pos =
          (clip->pcm_buffer.read_pos + bytes_to_advance) %
          clip->pcm_buffer.capacity;
      clip->playback_position -= (safe_position);
    }
  }

  if (did_write) {
    os_audio_write_samples(audio_samples,
                           samples_needed * state->output_channels);
  }

  os_audio_update();
}

void audio_play_clip(AudioState *state, AudioClip clip) {
  if (clip.wav_file && clip.wav_file->is_loaded) {
    // Initialize clip for playback
    clip.playback_position = 0.0f;
    clip.is_playing = true;
    // todo: volume is currently linear, should be log10
    clip.volume = clip.volume;

    i32 target_rate = os_audio_get_sample_rate();
    clip.sample_rate_ratio =
        (f32)clip.wav_file->format.sample_rate / (f32)target_rate;

    // Add to clips slice
    arr_append(state->clips, clip);
  }
}

b32 wav_parse_header(u8 *file_data, u32 file_size, WavFile *wav) {
  if (file_size < sizeof(WavRiffHeader)) {
    LOG_ERROR("File too small for WAV header: %", FMT_INT(file_size));
    return false;
  }

  u8 *ptr = file_data;
  WavRiffHeader *riff = (WavRiffHeader *)ptr;
  ptr += sizeof(WavRiffHeader);

  // Validate RIFF header
  if (riff->riff[0] != 'R' || riff->riff[1] != 'I' || riff->riff[2] != 'F' ||
      riff->riff[3] != 'F') {
    LOG_ERROR("Invalid RIFF header");
    return false;
  }

  if (riff->wave[0] != 'W' || riff->wave[1] != 'A' || riff->wave[2] != 'V' ||
      riff->wave[3] != 'E') {
    LOG_ERROR("Invalid WAVE header");
    return false;
  }

  // Parse chunks
  b32 found_fmt = false;
  b32 found_data = false;

  while (ptr < file_data + file_size - 8) {
    // Read chunk header (4 bytes ID + 4 bytes size)
    u8 *chunk_id = ptr;
    u32 chunk_size = *(u32 *)(ptr + 4);
    ptr += 8;

    LOG_INFO("Found chunk: %%%%, size: %", FMT_UINT(chunk_id[0]),
             FMT_UINT(chunk_id[1]), FMT_UINT(chunk_id[2]),
             FMT_UINT(chunk_id[3]), FMT_UINT(chunk_size));

    if (chunk_id[0] == 'f' && chunk_id[1] == 'm' && chunk_id[2] == 't' &&
        chunk_id[3] == ' ') {
      // Format chunk
      if (chunk_size < sizeof(WavFormatData)) {
        LOG_ERROR("Format chunk too small");
        return false;
      }

      wav->format = *(WavFormatData *)ptr;
      found_fmt = true;

      LOG_INFO("Format: channels=%, rate=%, bits=%",
               FMT_INT(wav->format.channels), FMT_INT(wav->format.sample_rate),
               FMT_INT(wav->format.bits_per_sample));

      if (wav->format.audio_format != 1) {
        LOG_ERROR("Unsupported audio format: %",
                  FMT_INT(wav->format.audio_format));
        return false;
      }

    } else if (chunk_id[0] == 'd' && chunk_id[1] == 'a' && chunk_id[2] == 't' &&
               chunk_id[3] == 'a') {
      // Data chunk
      wav->data_size = chunk_size;
      wav->audio_data = (i16 *)ptr;
      found_data = true;

      LOG_INFO("Data chunk: size=% bytes", FMT_INT(chunk_size));
      break; // Stop parsing after finding data
    }

    // Skip to next chunk (ensure even alignment)
    ptr += (chunk_size + 1) & ~1;
  }

  if (!found_fmt || !found_data) {
    LOG_ERROR("Missing required chunks: fmt=%, data=%", FMT_INT(found_fmt),
              FMT_INT(found_data));
    return false;
  }

  // Calculate total samples
  u32 bytes_per_sample = wav->format.bits_per_sample / 8;
  wav->total_samples =
      wav->data_size / (wav->format.channels * bytes_per_sample);

  LOG_INFO("Calculated total_samples: %", FMT_INT(wav->total_samples));

  wav->is_loaded = true;

  return true;
}

// Streaming audio functions
StreamingAudioClip streaming_clip_create(u32 source_sample_rate, u32 channels,
                                         u32 buffer_capacity, AppContext *ctx) {
  StreamingAudioClip clip = {0};

  clip.pcm_buffer.buffer = ALLOC_ARRAY(&ctx->allocator, u8, buffer_capacity);
  clip.pcm_buffer.capacity = buffer_capacity;
  clip.pcm_buffer.write_pos = 0;
  clip.pcm_buffer.read_pos = 0;
  clip.pcm_buffer.is_complete = false;

  clip.source_sample_rate = source_sample_rate;
  clip.channels = channels;
  clip.playback_position = 0.0f;
  clip.volume = 1.0f;
  clip.is_playing = false;
  clip.loop = false;

  // Calculate sample rate conversion ratio
  i32 target_rate = os_audio_get_sample_rate();
  clip.sample_rate_ratio = (f32)source_sample_rate / (f32)target_rate;

  return clip;
}

void streaming_clip_write_pcm(StreamingAudioClip *clip, u8 *pcm_data,
                              u32 size) {
  StreamingBuffer *buffer = &clip->pcm_buffer;

  u32 available_space = streaming_buffer_available_space(buffer);

  if (size > available_space) {
    // Buffer overrun - advance read position to make space for new data
    u32 bytes_count_to_override = size - available_space;
    LOG_WARN("Audio buffer overrun: overriding % bytes of old data",
             FMT_UINT(bytes_count_to_override));
  }

  // Write data to ring buffer (will override old data if buffer was full)
  for (u32 i = 0; i < size; i++) {
    buffer->buffer[buffer->write_pos] = pcm_data[i];
    buffer->write_pos = (buffer->write_pos + 1) % buffer->capacity;
  }
}

void streaming_clip_mark_complete(StreamingAudioClip *clip) {
  clip->pcm_buffer.is_complete = true;
}

internal i16 ring_buffer_read_i16(StreamingBuffer *buffer, u32 byte_pos) {
  u32 p0 = byte_pos % buffer->capacity;
  u32 p1 = (byte_pos + 1) % buffer->capacity;
  u8 lo = buffer->buffer[p0];
  u8 hi = buffer->buffer[p1];
  return (i16)(lo | (hi << 8));
}

b32 streaming_clip_get_sample(StreamingAudioClip *clip, f32 position, f32 *left,
                              f32 *right) {
  StreamingBuffer *buffer = &clip->pcm_buffer;

  u32 sample_index = (u32)position;
  f32 fraction = position - sample_index;

  u32 bytes_per_sample = sizeof(i16) * clip->channels;
  u32 available_data = streaming_buffer_available_data_len(buffer);
  u32 available_samples = available_data / bytes_per_sample;

  if (sample_index >= available_samples) {
    *left = *right = 0.0f;
    return false;
  }

  u32 byte_offset1 = buffer->read_pos + sample_index * bytes_per_sample;
  u32 byte_offset2 = byte_offset1 + bytes_per_sample;

  if (clip->channels == 1) {
    i16 sample1 = ring_buffer_read_i16(buffer, byte_offset1);
    i16 sample2 = sample1;

    if (sample_index + 1 < available_samples) {
      sample2 = ring_buffer_read_i16(buffer, byte_offset2);
    }

    f32 interpolated =
        lerp(pcm16_to_float(sample1), pcm16_to_float(sample2), fraction);
    *left = *right = interpolated;

  } else if (clip->channels == 2) {
    i16 left1 = ring_buffer_read_i16(buffer, byte_offset1);
    i16 right1 = ring_buffer_read_i16(buffer, byte_offset1 + 2);

    i16 left2 = left1;
    i16 right2 = right1;

    if (sample_index + 1 < available_samples) {
      left2 = ring_buffer_read_i16(buffer, byte_offset2);
      right2 = ring_buffer_read_i16(buffer, byte_offset2 + 2);
    }

    *left = lerp(pcm16_to_float(left1), pcm16_to_float(left2), fraction);
    *right = lerp(pcm16_to_float(right1), pcm16_to_float(right2), fraction);
  }

  return true;
}

StreamingAudioClip *audio_play_streaming_clip(AudioState *state,
                                              StreamingAudioClip clip) {
  clip.playback_position = 0.0f;
  clip.is_playing = true;

  arr_append(state->streaming_clips, clip);
  return &state->streaming_clips.items[state->streaming_clips.len - 1];
}

void streaming_clip_reset(StreamingAudioClip *clip) {
  clip->playback_position = 0;
  clip->pcm_buffer.read_pos = 0;
  clip->pcm_buffer.write_pos = 0;
  // todo: fade remaining samples instead of hard cut
  memset(clip->pcm_buffer.buffer, 0, clip->pcm_buffer.capacity);
}

b32 streaming_clip_has_audio_content(StreamingAudioClip *clip) {
  if (!clip->is_playing) {
    return false;
  }

  StreamingBuffer *buffer = &clip->pcm_buffer;
  u32 available_data = streaming_buffer_available_data_len(buffer);
  u32 bytes_per_sample = sizeof(i16) * clip->channels;
  return available_data > bytes_per_sample;
}

u32 wav_calculate_file_size(WavFile *wav) {
  // RIFF header (12 bytes) + fmt chunk (8 + 16 bytes) + data chunk (8 bytes) +
  // data
  return 12 + 8 + 16 + 8 + wav->data_size;
}

u8_Array wav_write_file_alloc(WavFile *wav, Allocator *allocator) {
  u32 required_size = wav_calculate_file_size(wav);
  u8_Array wav_bytes = arr_new_alloc(allocator, u8, required_size);
  wav_write_file(wav, wav_bytes.items, wav_bytes.len);
  return wav_bytes;
}

u32 wav_write_file(WavFile *wav, u8 *buffer, u32 buffer_size) {
  u32 required_size = wav_calculate_file_size(wav);
  if (buffer_size < required_size) {
    LOG_ERROR("Buffer too small: need % bytes, have %", FMT_UINT(required_size),
              FMT_UINT(buffer_size));
    return 0;
  }

  u8 *ptr = buffer;

  // Write RIFF header
  WavRiffHeader riff_header;
  riff_header.riff[0] = 'R';
  riff_header.riff[1] = 'I';
  riff_header.riff[2] = 'F';
  riff_header.riff[3] = 'F';
  riff_header.file_size = required_size - 8; // Total file size minus 8 bytes
  riff_header.wave[0] = 'W';
  riff_header.wave[1] = 'A';
  riff_header.wave[2] = 'V';
  riff_header.wave[3] = 'E';

  memcpy(ptr, &riff_header, sizeof(WavRiffHeader));
  ptr += sizeof(WavRiffHeader);

  // Write fmt chunk header
  ptr[0] = 'f';
  ptr[1] = 'm';
  ptr[2] = 't';
  ptr[3] = ' ';
  ptr += 4;
  *(u32 *)ptr = 16; // fmt chunk data size (16 bytes for PCM)
  ptr += 4;

  // Write format data
  memcpy(ptr, &wav->format, sizeof(WavFormatData));
  ptr += sizeof(WavFormatData);

  // Write data chunk header
  ptr[0] = 'd';
  ptr[1] = 'a';
  ptr[2] = 't';
  ptr[3] = 'a';
  ptr += 4;
  *(u32 *)ptr = wav->data_size;
  ptr += 4;

  // Write audio data
  memcpy(ptr, wav->audio_data, wav->data_size);
  ptr += wav->data_size;

  LOG_INFO("WAV file written: % bytes total", FMT_UINT(required_size));
  return required_size;
}

WavFile *create_wav_from_samples_alloc(i16 *samples, u32 samples_len,
                                       u32 sample_rate, Allocator *allocator) {
  WavFile *wav = ALLOC(allocator, WavFile);
  wav->audio_data = ALLOC_ARRAY(allocator, i16, samples_len);
  memcpy(wav->audio_data, samples, sizeof(i16) * samples_len);
  *wav = create_wav_from_samples(wav->audio_data, samples_len, sample_rate);
  return wav;
}

WavFile create_wav_from_samples(i16 *samples, u32 samples_len,
                                u32 sample_rate) {
  WavFile wav = {0};

  wav.format.audio_format = 1; // PCM
  wav.format.channels = 1;     // Mono
  wav.format.sample_rate = sample_rate;
  wav.format.bits_per_sample = 16;
  wav.format.block_align =
      wav.format.channels * (wav.format.bits_per_sample / 8);
  wav.format.byte_rate = wav.format.sample_rate * wav.format.block_align;

  wav.total_samples = samples_len;
  wav.data_size = wav.total_samples * sizeof(i16);
  wav.audio_data = samples;

  wav.is_loaded = true;

  LOG_INFO("Created WAV: %Hz, % samples, % bytes", FMT_UINT(sample_rate),
           FMT_UINT(wav.total_samples), FMT_UINT(wav.data_size));

  return wav;
}