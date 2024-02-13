#include <SDL3/SDL.h>
#include <linmath.h>
#include <kvec.h>
#include <stdio.h>
#include "engine.h"

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (!engine_init("Wolfestein C"))
    {
        SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
        return 1;
    }

    while (engine_is_running())
    {
        engine_handle_input();
        engine_render();
    }

    engine_cleanup();
    return 0;
}