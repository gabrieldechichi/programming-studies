#include "SDL3/SDL_audio.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_keycode.h"
#include "SDL3/SDL_log.h"
#include "SDL3/SDL_pixels.h"
#include "SDL3/SDL_render.h"
#include "SDL3/SDL_stdinc.h"
#include <stdint.h>
#include <stdio.h>

#define PI 3.14159265358979323846
#define SAMPLE_RATE 48000
#define FREQUENCY 256
#define SINE_TIME_STEP (((2.0 * PI) * FREQUENCY) / SAMPLE_RATE)
#define BUFFER_SIZE 512
#define VOLUME 0.5

#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480

int main() {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
    SDL_Log("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
    return 1;
  }

  SDL_Window *window =
      SDL_CreateWindow("SDL3 Window", WINDOW_WIDTH, WINDOW_HEIGHT, 0);
  if (window == NULL) {
    SDL_Log("Window could not be created! SDL_Error: %s\n", SDL_GetError());
    return 1;
  }
  SDL_ShowWindow(window);

  SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
  if (renderer == NULL) {
    SDL_Log("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  SDL_Texture *frame_buffer =
      SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                        SDL_TEXTUREACCESS_STATIC, WINDOW_WIDTH, WINDOW_HEIGHT);

  if (!frame_buffer) {
    SDL_Log("Texture could not be created! SDL_Error: %s\n", SDL_GetError());
    return -1;
  }

  uint32_t pixels[WINDOW_WIDTH * WINDOW_HEIGHT];
  SDL_memset(pixels, 0, sizeof(pixels));

  SDL_AudioSpec audio_spec = {};

  audio_spec.channels = 1;
  audio_spec.format = SDL_AUDIO_F32;
  audio_spec.freq = SAMPLE_RATE;

  SDL_AudioStream *stream = SDL_OpenAudioDeviceStream(
      SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &audio_spec, NULL, NULL);

  if (!stream) {
    SDL_Log("Couldn't create audio stream: %s", SDL_GetError());
    return -1;
  }

  bool audio_playing = false;
  SDL_ResumeAudioStreamDevice(stream);

  bool quit = false;
  SDL_Event event;

  while (!quit) {
    // Event handling
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
      case SDL_EVENT_QUIT: {
        quit = true;
        break;
      }
      case SDL_EVENT_KEY_DOWN: {
        if (event.key.key == SDLK_ESCAPE) {
          quit = true;
        }
        break;
      }
      }
    }

    // audio
    {
      const int minimum_audio = (SAMPLE_RATE * sizeof(float)) / 2;
      if (SDL_GetAudioStreamQueued(stream) < minimum_audio) {
        static float samples[BUFFER_SIZE];
        static float time = 0;

        if (!audio_playing) {
          for (int i = 0; i < BUFFER_SIZE; i++) {
            samples[i] = 0;
            time += SINE_TIME_STEP;
          }
          audio_playing = true;
        } else {
          for (int i = 0; i < BUFFER_SIZE; i++) {
            float sine = SDL_sinf(time);
            samples[i] = sine * VOLUME;
            time += SINE_TIME_STEP;
          }
        }

        SDL_PutAudioStreamData(stream, samples, sizeof(samples));
      }
    }

    // pixel stuff
    {
      for (int y = 0; y < WINDOW_HEIGHT; y++) {
        for (int x = 0; x < WINDOW_WIDTH; x++) {
          int i = y * WINDOW_WIDTH + x;
          uint32_t color = 0xFF << 24 | 0xFF;
          pixels[i] = color;
        }
      }
    }

    SDL_UpdateTexture(frame_buffer, NULL, pixels,
                      WINDOW_WIDTH * sizeof(uint32_t));

    SDL_RenderTexture(renderer, frame_buffer, NULL, NULL);

    SDL_RenderPresent(renderer);
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
