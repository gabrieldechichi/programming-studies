#ifndef MICROPHONE_H
#define MICROPHONE_H

#include "context.h"
#include "typedefs.h"

typedef struct {
  b32 is_recording;
  u32 sample_rate;
  b32 is_initialized;
} MicrophoneState;

MicrophoneState microphone_init(AppContext *ctx);

void microphone_start_recording(MicrophoneState *mic);
void microphone_stop_recording(MicrophoneState *mic);

u32 microphone_get_available_samples(MicrophoneState *mic);

// Read available microphone samples
// Returns number of samples actually read
u32 microphone_read_samples(MicrophoneState *mic, i16 *buffer, u32 max_samples);

u32 microphone_get_sample_rate(MicrophoneState *mic);

#endif // MICROPHONE_H