#include "engine.h"

SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture *frameBuffer = NULL;
int running = 1;

int engine_init(const char *title)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        return 0;
    }

    window = SDL_CreateWindowWithPosition(
        title,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        RENDER_RESOLUTION,
        RENDER_RESOLUTION,
        SDL_WINDOW_OPENGL);

    if (window == NULL)
    {
        return 0;
    }

    renderer = SDL_CreateRenderer(window, NULL, SDL_RENDERER_SOFTWARE);
    if (renderer == NULL)
    {
        engine_cleanup();
        return 0;
    }

    frameBuffer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGRA8888, SDL_TEXTUREACCESS_TARGET, BASE_RESOLUTION, BASE_RESOLUTION);

    if (frameBuffer == NULL)
    {
        engine_cleanup();
        return 0;
    }

    running = 1;
    return 1;
}

int engine_is_running(void)
{
    return running;
}

void engine_handle_input(void)
{
    SDL_Event event;

    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_EVENT_QUIT || (event.type == SDL_EVENT_KEY_DOWN && event.key.keysym.sym == SDLK_ESCAPE))
        {
            running = 0;
        }
    }
}

void engine_render(void)
{
    SDL_SetRenderTarget(renderer, frameBuffer);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    SDL_FRect square = {BASE_RESOLUTION / 2, BASE_RESOLUTION / 2, 10, 10};
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_RenderFillRect(renderer, &square);

    SDL_SetRenderTarget(renderer, NULL);

    SDL_RenderTexture(renderer, frameBuffer, NULL, NULL);
    SDL_RenderPresent(renderer);
}

void engine_cleanup(void)
{
    if (frameBuffer)
    {
        SDL_DestroyTexture(frameBuffer);
    }
    if (renderer)
    {
        SDL_DestroyRenderer(renderer);
    }
    if (window)
    {
        SDL_DestroyWindow(window);
    }

    SDL_Quit();
}