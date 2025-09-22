#include "game.h"
// #include "gyms/gym_triangle.c"
// #include "gyms/character_test.c"
// #include "gyms/audio_test.c"
// #include "gyms/audio_wav_test.c"
// #include "gyms/audio_stream_test_from_file.c"
// #include "gyms/http_test.c"
#include "lib/typedefs.h"
#include "lib/fmt.h"
#include <stdio.h>

// Simple logging implementation
typedef enum { LOGLEVEL_INFO, LOGLEVEL_WARN, LOGLEVEL_ERROR } LogLevel;

void platform_log(LogLevel log_level, const char *fmt, const FmtArgs *args,
                  const char *file_name, uint32 line_number) {
  char buffer[1024];
  fmt_string(buffer, sizeof(buffer), fmt, args);

  const char *level_str;
  FILE *output;
  switch (log_level) {
  case LOGLEVEL_INFO:
    level_str = "INFO";
    output = stdout;
    break;
  case LOGLEVEL_WARN:
    level_str = "WARN";
    output = stderr;
    break;
  case LOGLEVEL_ERROR:
    level_str = "ERROR";
    output = stderr;
    break;
  default:
    level_str = "UNKNOWN";
    output = stderr;
    break;
  }

  fprintf(output, "[%s] %s:%u: %s\n", level_str, file_name, line_number,
          buffer);
  fflush(output);
}

#define PLATFORM_LOG(level, fmt, ...)                                          \
  do {                                                                         \
    FmtArg args[] = {__VA_ARGS__};                                             \
    FmtArgs fmtArgs = {args, COUNT_VARGS(FmtArg, __VA_ARGS__)};                \
    platform_log(level, fmt, &fmtArgs, __FILE__, __LINE__);               \
  } while (0)

#define LOG_INFO(fmt, ...) PLATFORM_LOG(LOGLEVEL_INFO, fmt, __VA_ARGS__)
#define LOG_WARN(fmt, ...) PLATFORM_LOG(LOGLEVEL_WARN, fmt, __VA_ARGS__)
#define LOG_ERROR(fmt, ...) PLATFORM_LOG(LOGLEVEL_ERROR, fmt, __VA_ARGS__)

// Define the input button and event names
const char *input_button_names[KEY_MAX] = {
#define INPUT_BUTTON(v, a) a
    INPUT_BUTTONS
#undef INPUT_BUTTON
};

const char *input_event_names[EVENT_MAX] = {
#define EVENT_TYPE(v, a) a
    EVENT_TYPES
#undef EVENT_TYPE
};

// Stub implementation for get_global_ctx - not used in video_renderer
GameContext *get_global_ctx() {
  return NULL;
}

export void game_init(GameMemory *memory) {
  LOG_INFO("Game initialized");
  LOG_INFO("Permanent memory: % MB",
           FMT_FLOAT(BYTES_TO_MB(memory->pernament_memory_size)));
  LOG_INFO("Temporary memory: % MB",
           FMT_FLOAT(BYTES_TO_MB(memory->temporary_memory_size)));

  // gym_init(memory);
}

export void game_update_and_render(GameMemory *memory) {
  // gym_update_and_render(memory);
}
