#ifndef GAYA_PPU_RENDER
#define GAYA_PPU_RENDER

#include "../types.hpp"
#include "draw_tile.hpp"
#include "../sdl_utils.hpp"

#include <tuple>
#include <map>
#include <mutex>
#include <thread>

using namespace std;

enum class PPU_DEBUG_MODE {
    NONE, BACKGROUND, SPRITE, SPRITE0HIT = 4, BACKGROUNDFULL = 9
};

// first is for # of PT, second one is for # of tile
typedef std::tuple<UINT8,UINT8,UINT8> tile_idx;

class PPU_Render
{
private:
    sdl_context                               *sdl_ctx;
    PPU_mem                                   *ppu_mem; // PPU_state is in ppu_mem
    cpu6502                                   *cpu;
    PPU_state                                 *ppu_state;

    PPU_DEBUG_MODE                            debug_mode;

    UINT16                                    tile, in_tile;
    UINT32                                    ticks_frame; // ticks since the beginning of the last frame
    SDL_Surface                               *screen_surface;
    SDL_Texture                               *screen_texture;

    void                                      step_pre_render();
    void                                      step_post_render();
    void                                      step_vblank();
    void                                      step_visible();
    void                                      step_two_first_tiles();

    void                                      fetch_nt_byte();
    void                                      fetch_attr_byte();
    void                                      fetch_low_pt_byte();
    void                                      fetch_high_pt_byte();
    void                                      set_regs_first_tile();
    void                                      set_regs_next_tile();
    void                                      fetch_first_tile_attr();
    void                                      fetch_next_tile_attr();

    /* sprites related */

    void                                      init_secondary_oam_sprite(UINT8 n);
    void                                      second_oam_eval_loop(UINT8 sprite);
    void                                      sprite_fetches(UINT8 sprite);
    UINT8                                     sprite_fetch_pt(UINT8 tile_idx, UINT8 offset);
    void                                      compute_fine_Y_sprite();

    /* actual rendering, multiplexing */
    void                                      update_background_regs();
    UINT8                                     compute_background_sub_attr();
    UINT8                                     render_background_pixel(UINT8 sub_attr);

    void                                      update_sprite_regs();

    typedef struct {UINT8 sprite, color, sub_attr;} sprite_res;
    /* returns the sprite displayed, and the color of the pixel. If no pixel is displayed, returns {8, 0x00} */
    sprite_res                                choose_sprite_pixel();

    void                                      render_pixel();

    /* general utility functions */
    UINT8                                     reverse_byte(UINT8 byte);

public:

    struct save_state {
        UINT16 tile, in_tile; // not even useful
        UINT32 ticks;
    };

    PPU_Render(sdl_context *sdl_ctx, PPU_mem *ppu_mem, PPU_state *ppu_state, cpu6502 *cpu);
    ~PPU_Render();

    void                                      draw_debug_tiles_grid(UINT8 half_size);
    void                                      ppu_step();
    void                                      ppu_execute_ticks(UINT32 nr_ticks);
    // keep in mind that a frame begins at the start of post render scanline
    void                                      ppu_execute_up_to(UINT32 nr_ticks);
    void                                      ppu_begin_frame();
    save_state                                ppu_render_save_state();
    void                                      ppu_render_restore_state(save_state s);

    /* Debug stuff, render() shouldn't be used ! */
    void                                      render();
    void                                      set_debug_mode(PPU_DEBUG_MODE debug);


    SDL_Window                                *open_pattern_table_window();
};


#endif