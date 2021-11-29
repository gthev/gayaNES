#ifndef DRAW_TILE_HPP
#define DRAW_TILE_HPP

#include "SDL2/SDL.h"
#include "../types.hpp"
#include "../ppu_info.hpp"
#include "../sdl_utils.hpp"

typedef struct {
    SDL_Renderer *renderer;
    unsigned int block_size;
    SDL_PixelFormat *format;
} sdl_context;

typedef struct {
    UINT8 c0, c1, c2, c3;
} color4;

typedef struct {
    UINT8 r, g, b;
} color3;

color3 color_from_uint8(UINT8 c);

// SDL_Texture *texture_from_background_tile(sdl_context *ctx, TILE t, UINT8 att, UINT8 background_color, palette pal[4]);

SDL_Window *open_palette_window(palette background_pal[4], palette sprite_pal[4], UINT8 square_size);

static color3 nes_colors[64] = {
    {84, 84, 84},
    {0, 30, 116},
    {8, 16, 144},
    {48, 0, 136},
    {68, 0, 100},
    {92, 0, 48},
    {84, 4, 0},
    {60, 24, 0},
    {32, 42, 0},
    {8, 58, 0},
    {0, 64, 0},
    {0, 60, 0},
    {0, 50, 60},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {152,150, 152},
    {8, 76, 196},
    {48, 50, 236},
    {92, 30, 228},
    {136, 20, 176},
    {160, 20, 100},
    {152, 34, 32},
    {120, 60, 0},
    {84, 90, 0},
    {40, 114, 0},
    {8, 124, 0},
    {0, 118, 40},
    {0, 102, 120},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {236, 238, 236},
    {76, 154, 236},
    {120, 124, 236},
    {176, 98, 236},
    {228, 84, 236},
    {236, 88, 180},
    {236, 106, 100},
    {212, 136, 32},
    {160, 170, 0},
    {116, 196, 0},
    {76, 208, 32},
    {56, 204, 108},
    {56, 180, 204},
    {60, 60, 60},
    {0, 0, 0},
    {0, 0, 0},
    {236, 238, 236},
    {168, 204, 236},
    {188, 188, 236},
    {212, 178, 236},
    {236, 174, 236},
    {236, 174, 212},
    {236, 180, 176},
    {228, 196, 144},
    {204, 210, 120},
    {180, 222, 120},
    {168, 226, 144},
    {152, 226, 180},
    {160, 214, 228},
    {160, 162, 160},
    {0, 0, 0},
    {0, 0, 0}
};



#endif