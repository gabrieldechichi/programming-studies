#include "game.h"
#include "typedefs.h"

export GAME_UPDATE_AND_RENDER(game_update_and_render) {
  // pixel stuff
  {
    static uint8_t r_shift = 0;
    static uint8_t g_shift = 0xFF / 2;
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
  // {
  //   const int minimum_audio = (sound_buffer->sample_rate * sizeof(float)) /
  //   2; if (SDL_GetAudioStreamQueued(stream) < minimum_audio) {
  //     static float samples[BUFFER_SIZE];
  //     static float time = 0;
  //
  //     if (!audio_playing) {
  //       for (int i = 0; i < BUFFER_SIZE; i++) {
  //         samples[i] = 0;
  //         time += SINE_TIME_STEP;
  //       }
  //       audio_playing = true;
  //     } else {
  //       for (int i = 0; i < BUFFER_SIZE; i++) {
  //         float sine = SDL_sinf(time);
  //         samples[i] = sine * VOLUME;
  //         time += SINE_TIME_STEP;
  //       }
  //     }
  //
  //     SDL_PutAudioStreamData(stream, samples, sizeof(samples));
  //   }
  // }
}
