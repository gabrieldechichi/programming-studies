#include "game.h"
#include "typedefs.h"
#include <math.h>
#include <stdio.h>

#define VOLUME 0.5

#define SINE_FREQUENCY 256
#define SINE_TIME_STEP (((2.0 * PI) * SINE_FREQUENCY) / AUDIO_SAMPLE_RATE)

void debug_audio_sine_wave(Game_SoundBuffer *sound_buffer, bool flag) {
  local_persist float time = PI / 2;
  float time_step = (((2.0 * PI) * SINE_FREQUENCY) / sound_buffer->sample_rate);

  for (int i = 0; i < sound_buffer->sample_count; i++) {
    float sine = sinf(time);
    if (flag) {
      if (sine < 0) {
        sine = 0;
      }
    }
    sound_buffer->samples[i] = sine * VOLUME;
    time += time_step;
  }
}

export GAME_INIT(game_init) {
  memory->platform.platform_log("\n\n\nHey there\n\n\n", LOG_INFO);
}

export GAME_UPDATE_AND_RENDER(game_update_and_render) {
  UNUSED(memory);
  local_persist bool flag = false;

  // pixel stuff
  {
    static uint8 r_shift = 0;
    static uint8 g_shift = 0xFF / 2;
    r_shift += 10;
    g_shift -= 10;
    for (int y = 0; y < screen_buffer->height; y++) {
      for (int x = 0; x < screen_buffer->width; x++) {
        int i = y * screen_buffer->width + x;
        r_shift = 0;
        uint32_t color = r_shift << 24 | g_shift << 16 | 0xFF;
        screen_buffer->pixels[i] = color;
      }
    }
  }

  // audio
  {
    if (input->space_bar.pressed_this_frame) {
      flag = !flag;
      sound_buffer->clear_buffer = true;
    }
    debug_audio_sine_wave(sound_buffer, flag);
  }
}
