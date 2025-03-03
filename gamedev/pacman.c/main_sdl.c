#include "./game.h"

#include "SDL3/SDL.h"
#include "SDL3/SDL_audio.h"
#include "SDL3/SDL_error.h"
#include "SDL3/SDL_filesystem.h"
#include "SDL3/SDL_loadso.h"
#include "SDL3/SDL_log.h"
#include "SDL3/SDL_stdinc.h"
#include "SDL3/SDL_timer.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "./typedefs.h"

#define PI 3.14159265358979323846

#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480

#define TARGET_FPS 120
#define TARGET_DT 1.0f / TARGET_FPS
#define TARGET_DT_NS SECS_TO_NS(TARGET_DT)
#define SLEEP_BUFFER_NS MS_TO_NS(1)

#define AUDIO_SAMPLE_RATE 48000
#define AUDIO_MARGIN_SECONDS                                                   \
  MS_TO_SECS(25) // assume our frame rate varies by 3 ms at most
#define AUDIO_BUFFER_SIZE                                                      \
  (int)(AUDIO_SAMPLE_RATE * AUDIO_MARGIN_SECONDS)
// #define AUDIO_BUFFER_SIZE 2048
#define VOLUME 0.5

#define SINE_FREQUENCY 256
#define SINE_TIME_STEP (((2.0 * PI) * SINE_FREQUENCY) / AUDIO_SAMPLE_RATE)

typedef struct {
  SDL_SharedObject *dll;
  SDL_Time last_modify_time;
  game_update_and_render_t *update_and_render;
} sdl_game_code_t;

typedef struct {
  SDL_AudioStream *stream;
  int32_t sample_rate;
  float *samples;
  int32_t samples_len;
  int32_t samples_byte_len;
} sdl_audio_buffer_t;

global sdl_game_code_t game_code = {};
global const char *game_dll_path = "./build/game.so";
global sdl_audio_buffer_t audio_buffer = {};

bool should_reload_game_code() {
  SDL_PathInfo info = {};
  if (!SDL_GetPathInfo(game_dll_path, &info)) {
    SDL_Log("Failed to get path info for game dll %s. Error: %s\n",
            game_dll_path, SDL_GetError());
    return false;
  }
  //== true to make sure we return 1 or 0
  return (info.modify_time > game_code.last_modify_time) == true;
}

int load_game_code() {
  SDL_PathInfo info = {};
  if (!SDL_GetPathInfo(game_dll_path, &info)) {
    SDL_Log("Failed to get path info for game dll %s. Error: %s\n",
            game_dll_path, SDL_GetError());
    return 0;
  }

  if (game_code.dll) {
    SDL_UnloadObject(game_code.dll);
  }

  SDL_SharedObject *game_dll = SDL_LoadObject(game_dll_path);
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
  game_code.last_modify_time = info.modify_time;

  return 1;
}

void debug_audio_sine_wave(bool flag) {
  local_persist float time = PI / 2;
  SDL_Log("%d\n", audio_buffer.samples_len);

  for (int i = 0; i < audio_buffer.samples_len; i++) {
    float sine = SDL_sinf(time);
    if (flag) {
      if (sine < 0) {
        sine = 0;
      }
    }
    audio_buffer.samples[i] = sine * VOLUME;
    time += SINE_TIME_STEP;
  }

  SDL_PutAudioStreamData(audio_buffer.stream, audio_buffer.samples,
                         audio_buffer.samples_byte_len);
  SDL_FlushAudioStream(audio_buffer.stream);
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
  audio_spec.freq = AUDIO_SAMPLE_RATE;

  audio_buffer.stream = SDL_OpenAudioDeviceStream(
      SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &audio_spec, NULL, NULL);

  if (!audio_buffer.stream) {
    SDL_Log("Couldn't create audio stream: %s", SDL_GetError());
    return -1;
  }

  float audio_samples[AUDIO_BUFFER_SIZE];
  SDL_memset(audio_samples, 0, sizeof(audio_samples));
  audio_buffer.sample_rate = AUDIO_SAMPLE_RATE;
  audio_buffer.samples = audio_samples;
  audio_buffer.samples_len = AUDIO_BUFFER_SIZE;
  audio_buffer.samples_byte_len = sizeof(float) * AUDIO_BUFFER_SIZE;

  SDL_ResumeAudioStreamDevice(audio_buffer.stream);

  if (!load_game_code()) {
    return -1;
  }

  bool quit = false;
  bool flag = false;
  SDL_Event event;
  uint64_t last_ticks = 0;
  while (!quit) {
    // check if we should reload game code
    if (should_reload_game_code()) {
      SDL_Log("reloading game code\n");
      load_game_code();
    }

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
      case SDL_EVENT_KEY_UP: {
        if (event.key.key == SDLK_A) {
          flag = !flag;
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
    //
    if (SDL_GetAudioStreamQueued(audio_buffer.stream) <
        audio_buffer.samples_byte_len) {
      debug_audio_sine_wave(flag);
    }

    game_code.update_and_render(NULL, NULL, &screen_buffer, NULL);

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
