#include "./game.h"

#include "SDL3/SDL.h"
#include "SDL3/SDL_error.h"
#include "SDL3/SDL_loadso.h"
#include "SDL3/SDL_log.h"
#include "SDL3/SDL_timer.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "./typedefs.h"

#define PI 3.14159265358979323846
#define SAMPLE_RATE 48000
#define FREQUENCY 256
#define SINE_TIME_STEP (((2.0 * PI) * FREQUENCY) / SAMPLE_RATE)
#define BUFFER_SIZE 2048
#define VOLUME 0.5

#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480

#define TARGET_FPS 60
#define TARGET_DT 1.0f / TARGET_FPS
#define TARGET_DT_NS SECS_TO_NS(TARGET_DT)
#define SLEEP_BUFFER_NS MS_TO_NS(1)

typedef struct {
  SDL_SharedObject *dll;
  game_update_and_render_t *update_and_render;
} sdl_game_code;

global sdl_game_code game_code = {};

int load_game_code() {
  SDL_SharedObject *game_dll = SDL_LoadObject("./build/game.so");
  if (!game_dll) {
    SDL_Log("Error loading game dll %s\n", SDL_GetError());
    return 0;
  }
  game_code.dll = game_dll;

  SDL_FunctionPointer update_and_render =
      SDL_LoadFunction(game_dll, "game_update_and_render");
  if (!update_and_render) {
    SDL_Log("Failed to load game_update_and_render from game_dll. %s\n",
            SDL_GetError());
    return 0;
  }
  game_code.update_and_render = (game_update_and_render_t *)update_and_render;

  return 1;
}

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

  SDL_SetRenderVSync(renderer, 1);

  SDL_Texture *frame_buffer =
      SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                        SDL_TEXTUREACCESS_STATIC, WINDOW_WIDTH, WINDOW_HEIGHT);

  if (!frame_buffer) {
    SDL_Log("Texture could not be created! SDL_Error: %s\n", SDL_GetError());
    return -1;
  }

  uint32_t pixels[WINDOW_WIDTH * WINDOW_HEIGHT];
  SDL_memset(pixels, 0, sizeof(pixels));

  game_offscreen_buffer_t screen_buffer = {};
  screen_buffer.pixels = pixels;
  screen_buffer.width = WINDOW_WIDTH;
  screen_buffer.height = WINDOW_HEIGHT;

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

  if (!load_game_code()) {
    return -1;
  }

  bool quit = false;
  SDL_Event event;
  uint64_t last_ticks = 0;
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

    // tick
    uint64_t now = SDL_GetTicksNS();
    uint64_t dt_ns = now - last_ticks;
    last_ticks = now;

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

    game_code.update_and_render(NULL, NULL, &screen_buffer);

    SDL_UpdateTexture(frame_buffer, NULL, pixels,
                      WINDOW_WIDTH * sizeof(uint32_t));

    SDL_RenderTexture(renderer, frame_buffer, NULL, NULL);

    SDL_RenderPresent(renderer);

    now = SDL_GetTicksNS();
    dt_ns = now - last_ticks;
    if (dt_ns < TARGET_DT_NS - SLEEP_BUFFER_NS) {
      uint64_t sleep_time = TARGET_DT_NS - dt_ns;
      SDL_DelayNS(sleep_time - SLEEP_BUFFER_NS);
    }

    now = SDL_GetTicksNS();
    dt_ns = now - last_ticks;
    last_ticks = now;
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
