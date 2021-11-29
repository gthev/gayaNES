#include "sdl_utils.hpp"

int init_window_renderer(const char *window_title, SDL_Window **window, SDL_Renderer **renderer, int width_screen, int height_screen) {
    if(SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::printf("SDL could not initialize! SDL_Error : %s\n", SDL_GetError());
        return -1;
    }
    *window = SDL_CreateWindow(window_title, SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          width_screen,
                                          height_screen,
                                          SDL_WINDOW_SHOWN);

    if(window == nullptr) {
        std::printf("Window could not be created! SDL_Error : %s\n", SDL_GetError());
        return -1;
    }

    *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if(renderer == nullptr) {
        std::printf("Renderer could not be created ! SDL_Error : %s\n", SDL_GetError());
        return -1;
    }
    return 0;
}
void SDL_pause()
{
    int continuer = 1;
    SDL_Event event;
 
    while (continuer)
    {
        SDL_WaitEvent(&event);
        switch(event.type)
        {
            case SDL_QUIT:
                std::printf("Quitting...\n");
                continuer = 0;
                break;
            case SDL_KEYDOWN:
                if(event.key.keysym.sym == SDLK_ESCAPE)
                    continuer = 0;
                break;
        }
    }
}

void set_surface_pixel(SDL_Surface *surface, int block_size, int x, int y, Uint32 color) {
    SDL_Rect r = {x*block_size, y*block_size, block_size, block_size};
    SDL_FillRect(surface, &r, color);
}