#ifndef GAYA_SDL_UTILS_HPP
#define GAYA_SDL_UTILS_HPP
#include <SDL2/SDL.h>
#include <iostream>

int init_window_renderer(const char *window_title, SDL_Window **window, SDL_Renderer **renderer, int width_screen, int height_screen);
void SDL_pause();
void set_surface_pixel(SDL_Surface *surface, int block_size, int x, int y, Uint32 color);

#endif