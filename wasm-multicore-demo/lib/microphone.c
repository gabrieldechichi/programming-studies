#include "microphone.h"
#include "os/os.h"

MicrophoneState microphone_init(AppContext *ctx) {
  UNUSED(ctx);
  MicrophoneState state = {0};

  state.sample_rate = os_mic_get_sample_rate();
  state.is_recording = false;
  state.is_initialized = true;

  LOG_INFO("Microphone initialized: sample_rate=%",
           FMT_UINT(state.sample_rate));

  return state;
}

void microphone_start_recording(MicrophoneState *mic) {
  debug_assert_msg(
      mic->is_initialized,
      "microphone_start_recording called before mic is initialized");

  if (!mic->is_initialized) {
    return;
  }

  if (mic->is_recording) {
    LOG_WARN("Microphone already recording");
    return;
  }

  os_mic_start_recording();
  mic->is_recording = true;

  LOG_INFO("Microphone recording started");
}

void microphone_stop_recording(MicrophoneState *mic) {
  if (!mic->is_initialized) {
    return;
  }

  if (!mic->is_recording) {
    return;
  }

  os_mic_stop_recording();
  mic->is_recording = false;

  LOG_INFO("Microphone recording stopped");
}

u32 microphone_get_available_samples(MicrophoneState *mic) {
  debug_assert_msg(
      mic->is_initialized,
      "microphone_get_available_samples called before mic is initialized");

  if (!mic->is_initialized || !mic->is_recording) {
    return 0;
  }

  return os_mic_get_available_samples();
}

u32 microphone_read_samples(MicrophoneState *mic, i16 *buffer,
                            u32 max_samples) {
  debug_assert_msg(mic->is_initialized,
                   "microphone_read_samples called before mic is initialized");
  if (!mic->is_initialized || !mic->is_recording) {
    return 0;
  }

  if (!buffer || max_samples == 0) {
    LOG_WARN("Invalid buffer or max_samples for microphone_read_samples");
    return 0;
  }

  u32 samples_read = os_mic_read_samples(buffer, max_samples);

  return samples_read;
}

u32 microphone_get_sample_rate(MicrophoneState *mic) {
  debug_assert_msg(mic->is_initialized,
                   "get_sample_rate called before mic is initialized");
  if (!mic->is_initialized) {
    return 48000;
  }

  return mic->sample_rate;
}