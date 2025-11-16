#include "lib/assert.h"
#include "os.h"
#include "lib/typedefs.h"
#include <stdlib.h>
#include "sokol/sokol_audio.h"

#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_CHANNELS 2
#define AUDIO_BUFFER_FRAMES 2048
#define AUDIO_PACKET_FRAMES 128

typedef struct {
  f32 *buffer;
  i32 buffer_size;
  i32 write_pos;
  i32 read_pos;
  i32 available_samples;
} AudioRingBuffer;

static struct {
  AudioRingBuffer ring_buffer;
  b32 initialized;
  f32 temp_buffer[MB(2)];
} audio_state;

#define TEMP_BUFFER_SIZE (MB(2))

#define RING_BUFFER_SIZE                                                       \
  (AUDIO_SAMPLE_RATE * AUDIO_CHANNELS * 2) // 2 seconds of audio

void os_audio_init(void) {
  saudio_setup(&(saudio_desc){
      .sample_rate = AUDIO_SAMPLE_RATE,
      .num_channels = AUDIO_CHANNELS,
      .buffer_frames = AUDIO_BUFFER_FRAMES,
      .packet_frames = AUDIO_PACKET_FRAMES,
  });

  audio_state.ring_buffer.buffer_size = RING_BUFFER_SIZE;
  audio_state.ring_buffer.buffer = (f32 *)calloc(RING_BUFFER_SIZE, sizeof(f32));
  audio_state.ring_buffer.write_pos = 0;
  audio_state.ring_buffer.read_pos = 0;
  audio_state.ring_buffer.available_samples = 0;

  audio_state.initialized = true;
}

void os_audio_shutdown(void) {
  if (audio_state.initialized) {
    saudio_shutdown();
    if (audio_state.ring_buffer.buffer) {
      free(audio_state.ring_buffer.buffer);
      audio_state.ring_buffer.buffer = NULL;
    }
    audio_state.initialized = false;
  }
}

void os_audio_write_samples(f32 *samples, i32 sample_count) {
  if (!audio_state.initialized || !samples || sample_count <= 0) {
    return;
  }

  AudioRingBuffer *rb = &audio_state.ring_buffer;

  // Write samples to ring buffer
  for (i32 i = 0; i < sample_count; i++) {
    // Check if buffer is full
    if (rb->available_samples >= rb->buffer_size) {
      // Buffer overflow - skip oldest samples
      rb->read_pos = (rb->read_pos + 1) % rb->buffer_size;
      rb->available_samples--;
    }

    rb->buffer[rb->write_pos] = samples[i];
    rb->write_pos = (rb->write_pos + 1) % rb->buffer_size;
    rb->available_samples++;
  }
}

void os_audio_update(void) {
  debug_assert_msg(audio_state.initialized,
                   "audio_update called without initializing audio");
  if (!audio_state.initialized) {
    return;
  }

  AudioRingBuffer *rb = &audio_state.ring_buffer;

  i32 frames_needed = saudio_expect();
  if (frames_needed <= 0) {
    return;
  }

  i32 samples_needed = frames_needed * AUDIO_CHANNELS;

  i32 samples_to_push = samples_needed;
  if (samples_to_push > rb->available_samples) {
    // LOG_WARN("Audio buffer underflow. Need % samples out of %",
    //          FMT_INT(samples_needed), FMT_INT(rb->available_samples));
    samples_to_push = rb->available_samples;
  } else if (samples_to_push < rb->available_samples) {
    LOG_WARN("Audio buffer overflow. Need % samples out of %",
             FMT_INT(samples_needed), FMT_INT(rb->available_samples));
  }

  if (samples_to_push <= 0) {
    return;
  }

  debug_assert_msg(samples_to_push < (i32)TEMP_BUFFER_SIZE,
                   "Pushing too many samples");

  for (i32 i = 0; i < samples_to_push; i++) {
    audio_state.temp_buffer[i] = rb->buffer[rb->read_pos];
    rb->read_pos = (rb->read_pos + 1) % rb->buffer_size;
  }
  rb->available_samples -= samples_to_push;

  i32 frames_to_push = samples_to_push / AUDIO_CHANNELS;
  saudio_push(audio_state.temp_buffer, frames_to_push);
}

i32 os_audio_get_sample_rate(void) { return AUDIO_SAMPLE_RATE; }

u32 os_audio_get_samples_needed() { return saudio_expect(); }