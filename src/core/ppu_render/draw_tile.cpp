#include <iostream>
#include <SDL2/SDL.h>
#include "draw_tile.hpp"

#define TILE_SIZE 8


color3 color_from_uint8(UINT8 c) {
    if(c < 64) return nes_colors[c];
    else return {0, 0, 0};
}

SDL_Window *open_palette_window(palette background_pal[4], palette sprite_pal[4], UINT8 square_size) {
    SDL_Window *palettes_window = nullptr;
    SDL_Renderer *palettes_renderer = nullptr;
    SDL_Surface *square_surf = nullptr;
    SDL_Texture *square_text = nullptr;
    SDL_Rect rect = {0, 0, square_size, square_size};
    int i, j, k;
    color3 square_color;

    if(init_window_renderer("Palettes", &palettes_window, &palettes_renderer, 4*square_size, 8*square_size) < 0) {
        std::printf("Palettes window opening failed : %s\n", SDL_GetError());
        return nullptr;
    }

    SDL_PixelFormat *format = SDL_AllocFormat(SDL_GetWindowPixelFormat(palettes_window));

    for(i=0; i<4; i++) {
        for(j=0; j<4; j++) {
            rect.x = 0; rect.y = 0;
            square_surf = SDL_CreateRGBSurface(0, square_size, square_size, 32, 0, 0, 0, 0);
            square_color = color_from_uint8(background_pal[i][j]);
            SDL_FillRect(square_surf, &rect, SDL_MapRGB(format, square_color.r, square_color.g, square_color.b));
            square_text = SDL_CreateTextureFromSurface(palettes_renderer, square_surf);
            rect.x = j*square_size; rect.y = i*square_size;
            SDL_RenderCopy(palettes_renderer, square_text, NULL, &rect);
            SDL_DestroyTexture(square_text);
            SDL_FreeSurface(square_surf);

            rect.x = 0; rect.y = 0;
            square_surf = SDL_CreateRGBSurface(0, square_size, square_size, 32, 0, 0, 0, 0);
            square_color = color_from_uint8(sprite_pal[i][j]);
            SDL_FillRect(square_surf, &rect, SDL_MapRGB(format, square_color.r, square_color.g, square_color.b));
            square_text = SDL_CreateTextureFromSurface(palettes_renderer, square_surf);
            rect.x = j*square_size; rect.y = (i+4)*square_size;
            SDL_RenderCopy(palettes_renderer, square_text, NULL, &rect);
            SDL_DestroyTexture(square_text);
            SDL_FreeSurface(square_surf);
        
        }
    }

    SDL_RenderPresent(palettes_renderer);
    //SDL_FreeFormat(format);
    return palettes_window;
}

