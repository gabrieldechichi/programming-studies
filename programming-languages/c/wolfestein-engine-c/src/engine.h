#ifndef ENGINE_H
#define ENGINE_H

#include <SDL3/SDL.h>

#define BASE_RESOLUTION 128
#define RENDER_RESOLUTION 128 * 6

int engine_init(const char *title);
int engine_is_running(void);
void engine_handle_input(void);
void engine_render(void);
void engine_cleanup(void);

#endif