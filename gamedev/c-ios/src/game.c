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
extern GameContext *get_global_ctx() { return 0; }

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
