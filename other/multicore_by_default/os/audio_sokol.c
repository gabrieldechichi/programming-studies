#include "lib/assert.h"
#include "os.h"
#include "lib/typedefs.h"
#include "sokol/sokol_audio.h"

#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_CHANNELS 2
#define AUDIO_BUFFER_FRAMES 2048
#define AUDIO_PACKET_FRAMES 128

static b32 audio_initialized = false;

void os_audio_init(void) {
  saudio_setup(&(saudio_desc){
      .sample_rate = AUDIO_SAMPLE_RATE,
      .num_channels = AUDIO_CHANNELS,
      .buffer_frames = AUDIO_BUFFER_FRAMES,
      .packet_frames = AUDIO_PACKET_FRAMES,
  });
  audio_initialized = true;
}

void os_audio_shutdown(void) {
  if (audio_initialized) {
    saudio_shutdown();
    audio_initialized = false;
  }
}

void os_audio_write_samples(f32 *samples, i32 sample_count) {
  if (!audio_initialized || !samples || sample_count <= 0) {
    return;
  }
  i32 frames = sample_count / AUDIO_CHANNELS;
  saudio_push(samples, frames);
}

void os_audio_update(void) {
}

i32 os_audio_get_sample_rate(void) { return AUDIO_SAMPLE_RATE; }

u32 os_audio_get_samples_needed() { return saudio_expect(); }
