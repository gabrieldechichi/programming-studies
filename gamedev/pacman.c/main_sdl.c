#include "SDL3/SDL.h"
#include "SDL3/SDL_audio.h"
#include "SDL3/SDL_error.h"
#include "SDL3/SDL_events.h"
#include "SDL3/SDL_filesystem.h"
#include "SDL3/SDL_keycode.h"
#include "SDL3/SDL_loadso.h"
#include "SDL3/SDL_log.h"
#include "SDL3/SDL_stdinc.h"
#include "SDL3/SDL_timer.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "./game.h"
#include "./typedefs.h"

#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480

#define TARGET_FPS 60
#define TARGET_DT 1.0f / TARGET_FPS
#define TARGET_DT_NS SECS_TO_NS(TARGET_DT)
#define SLEEP_BUFFER_NS MS_TO_NS(1)

#define AUDIO_SAMPLE_RATE 48000
// #define AUDIO_BUFFER_SIZE 1024
#define AUDIO_BUFFER_SIZE (int)(AUDIO_SAMPLE_RATE * TARGET_DT * 3)

#define SINE_FREQUENCY 256
#define SINE_TIME_STEP (((2.0 * PI) * SINE_FREQUENCY) / AUDIO_SAMPLE_RATE)

typedef struct {
  SDL_SharedObject *dll;
  SDL_Time last_modify_time;
  Game_Init *init;
  Game_UpdateAndRender *update_and_render;
} SDL_GameCode;

typedef struct {
  SDL_AudioStream *stream;
  int32 sample_rate;
  float *samples;
  int32 samples_len;
  int32 samples_byte_len;
} SDL_AudioBuffer;

global SDL_GameCode game_code = {};
global const char *game_dll_path = "./build/game.so";
global SDL_AudioBuffer audio_buffer = {};

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

  SDL_FunctionPointer init = SDL_LoadFunction(game_dll, "game_init");
  if (!init) {
    SDL_Log("Failed to load init from game_dll. %s\n", SDL_GetError());
    return 0;
  }

  SDL_FunctionPointer update_and_render =
      SDL_LoadFunction(game_dll, "game_update_and_render");
  if (!update_and_render) {
    SDL_Log("Failed to load game_update_and_render from game_dll. %s\n",
            SDL_GetError());
    return 0;
  }

  game_code.init = (Game_Init *)init;
  game_code.update_and_render = (Game_UpdateAndRender *)update_and_render;
  game_code.last_modify_time = info.modify_time;

  return 1;
}

void sdl_handle_button_event(Game_InputButton *button, uint64_t event) {
  bool was_pressed = button->is_pressed;
  button->is_pressed = event == SDL_EVENT_KEY_DOWN;
  button->pressed_this_frame = !was_pressed && button->is_pressed;
  button->released_this_frame = was_pressed && !button->is_pressed;
}

void sdl_clear_button_event(Game_InputButton *button) {
  button->pressed_this_frame = false;
  button->released_this_frame = false;
}

void platform_log(const char *fmt, LogType log_type, ...) {
  SDL_LogPriority log_priority;
  switch (log_type) {
  case LOG_INFO:
    log_priority = SDL_LOG_PRIORITY_INFO;
    break;
  case LOG_ERROR:
    log_priority = SDL_LOG_PRIORITY_ERROR;
    break;
  default:
    log_priority = SDL_LOG_PRIORITY_VERBOSE;
    SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_ERROR,
                   "Unhandled LogType from application %d\n", log_type);
  }
  va_list args;
  va_start(args, log_type);

  SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, log_priority, fmt, args);
  va_end(args);
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

  uint32 pixels[WINDOW_WIDTH * WINDOW_HEIGHT];
  SDL_memset(pixels, 0, sizeof(pixels));

  Game_ScreenBuffer screen_buffer = {};
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
  SDL_Log("Audio buffer size: %d\n", AUDIO_BUFFER_SIZE);
  SDL_memset(audio_samples, 0, sizeof(audio_samples));
  audio_buffer.sample_rate = AUDIO_SAMPLE_RATE;
  audio_buffer.samples = audio_samples;
  audio_buffer.samples_len = AUDIO_BUFFER_SIZE;
  audio_buffer.samples_byte_len = sizeof(float) * AUDIO_BUFFER_SIZE;

  Game_SoundBuffer game_sound_buffer = {};
  game_sound_buffer.samples = audio_samples;
  game_sound_buffer.sample_count = audio_buffer.samples_len;
  game_sound_buffer.sample_rate = audio_buffer.sample_rate;
  game_sound_buffer.clear_buffer = false;

  SDL_ResumeAudioStreamDevice(audio_buffer.stream);

  if (!load_game_code()) {
    return -1;
  }

  Game_Memory game_memory = {};
  game_memory.platform.platform_log = platform_log;

  game_code.init(&game_memory);

  Game_Input game_input = {};

  bool quit = false;
  SDL_Event event;
  uint64 last_ticks = 0;
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
      }

      if (event.type == SDL_EVENT_KEY_UP || event.type == SDL_EVENT_KEY_DOWN) {
        sdl_handle_button_event(&game_input.space_bar, event.type);
      }
    }

    // tick
    uint64 now = SDL_GetTicksNS();
    uint64 dt_ns = now - last_ticks;
    last_ticks = now;

    // game update
    {
      game_code.update_and_render(&game_memory, &game_input, &screen_buffer,
                                  &game_sound_buffer);

      sdl_clear_button_event(&game_input.space_bar);
    }

    // audio
    {
      // if game wrote new audio clear any pending samples
      if (game_sound_buffer.clear_buffer) {
        SDL_ClearAudioStream(audio_buffer.stream);
        game_sound_buffer.clear_buffer = false;
      }

      // write to audio stream
      SDL_PutAudioStreamData(audio_buffer.stream, audio_buffer.samples,
                             audio_buffer.samples_byte_len);
    }

    // render
    {
      SDL_UpdateTexture(frame_buffer, NULL, pixels,
                        WINDOW_WIDTH * sizeof(uint32));

      SDL_RenderTexture(renderer, frame_buffer, NULL, NULL);

      SDL_RenderPresent(renderer);
    }

    // wait for target frame rate
    {
      now = SDL_GetTicksNS();
      dt_ns = now - last_ticks;
      if (dt_ns < TARGET_DT_NS - SLEEP_BUFFER_NS) {
        uint64 sleep_time = TARGET_DT_NS - dt_ns;
        SDL_DelayNS(sleep_time - SLEEP_BUFFER_NS);
      }

      now = SDL_GetTicksNS();
      dt_ns = now - last_ticks;
      last_ticks = now;
    }
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
