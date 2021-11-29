#include <stdlib.h>
#include <thread>
#include "ppu_render.hpp"

#define BITSELECT16(val, pos) (((val) >> pos) & 0x0001)
#define BITSELECT8(val, pos) (((val) >> pos) & 0x01)

PPU_Render::PPU_Render(sdl_context *sdl_ctx, PPU_mem *ppu_mem, PPU_state *ppu_state, cpu6502 *cpu) : sdl_ctx(sdl_ctx), ppu_mem(ppu_mem), ppu_state(ppu_state),
        cpu(cpu), in_tile(0), tile(0)
{
    ppu_state->ticks = 0;
    ppu_state->scanline = 261;
    screen_surface = SDL_CreateRGBSurface(0, 256*sdl_ctx->block_size, 240*sdl_ctx->block_size, 32, 0, 0, 0, 0);
    if(!screen_surface) {
        throw MemAllocFailed("SDL surface allocation for PPU_Render failed\n");
    }
}

PPU_Render::~PPU_Render() 
{
    SDL_FreeSurface(screen_surface);
}

PPU_Render::save_state PPU_Render::ppu_render_save_state() {
    save_state res;
    res.in_tile = in_tile;
    res.tile = tile;
    res.ticks = ticks_frame;
    return res;
}

void PPU_Render::ppu_render_restore_state(save_state s) {
    in_tile = s.in_tile;
    tile = s.tile;
    ticks_frame = s.ticks;
}

void PPU_Render::ppu_execute_ticks(UINT32 nr_ticks) {
    for(uint64_t tick = 0; tick < nr_ticks; tick++) {
        ppu_step();
    }
}

void PPU_Render::ppu_execute_up_to(UINT32 nr_ticks) {
    int32_t todo_ticks = (int32_t) nr_ticks - (int32_t) ticks_frame;
    if(todo_ticks > 0) {
        ppu_execute_ticks(todo_ticks);
    }
}

void PPU_Render::ppu_begin_frame() {
    // step up to pre_render scanline
    ppu_state->ticks = 0;
    ppu_state->scanline = 261;
    ticks_frame = 0;
}

void PPU_Render::ppu_step() {
    // first, update values
    ppu_state->ticks++;
    ticks_frame++;
    if(ppu_state->ticks >= 341) {
        // new scan line
        ppu_state->ticks = 0;
        ppu_state->scanline++;
        // TODO : even/odd frames, and skip one tick !
        if(ppu_state->scanline >= 262) {
            // new frame !
            ppu_state->scanline = 0;
        }
    }

    // then, the tests
    switch(ppu_state->scanline) {
        case 240:
            step_post_render();
            break;
        case 261:
            step_pre_render();
            break;
        default:
            if(ppu_state->scanline < 240) step_visible();
            else step_vblank();
            break;
    }
}

/*
***********************
      POST RENDER
***********************
*/

void PPU_Render::step_post_render() {
    switch (ppu_state->ticks) {
    case 0:
        // we display the screen
        screen_texture = SDL_CreateTextureFromSurface(sdl_ctx->renderer, screen_surface);
        if(!screen_texture) {
            throw MemAllocFailed("Allocation of sdl texture failed in ppu render\n");
        }
        SDL_RenderCopy(sdl_ctx->renderer, screen_texture, NULL, NULL);
        SDL_DestroyTexture(screen_texture);
        SDL_RenderPresent(sdl_ctx->renderer);
        break;
    
    default:
        break;
    }
}

/*
***********************
        VBLANK
***********************
*/

void PPU_Render::step_vblank() {
    switch(ppu_state->ticks) {
        case 1:
            if(ppu_state->scanline == 241) {
                ppu_state->IN_VBLANK = 1;
                if(ppu_state->NMI_VBLANK) cpu->enter_nmi();
            }
            break;
        default:
            break; // not much
    }
}

/*
***********************
       PRE RENDER     
***********************
*/

void PPU_Render::step_pre_render() {
    if(ppu_state->ticks == 320) {
        ppu_state->SEC_OAM_IDX = 0;
        ppu_state->NEXT_OAM_IDX = 0;
        ppu_state->VRAM_ADDRESS = ppu_state->T_VRAM_ADDRESS;
    }
    switch (ppu_state->ticks)
    {
    case 1:
        ppu_state->SPRITE0HIT = 0;
        ppu_state->IN_VBLANK = 0;
        ppu_state->SPRITE_OVERFLOW = 0;
        ppu_state->SEC_OAM_IDX = 0;
        ppu_state->NEXT_OAM_IDX = 0;
        break;

    default:
        step_two_first_tiles();
        break;
    }
}


void PPU_Render::step_two_first_tiles() {
    switch (ppu_state->ticks)
    {
    case 322:
    case 330:
        fetch_nt_byte();
        break;
    case 324:
    case 332:
        fetch_attr_byte();
        break;
    case 326:
    case 334:
        fetch_low_pt_byte();
        break;
    case 328:
        fetch_high_pt_byte();
        // then init regs
        set_regs_first_tile();
        ppu_mem->increment_coarse_x_vram_addr();
        break;
    case 336:
        fetch_high_pt_byte();
        set_regs_next_tile();
        ppu_mem->increment_coarse_x_vram_addr();
        break;
    
    default:
        break;
    }
}

/*
************************
        VISIBLE
************************
*/


void PPU_Render::step_visible() {
    // todo : big (big) switch ?
    if(!ppu_state->ticks) {
        ppu_state->SEC_OAM_IDX = ppu_state->NEXT_OAM_IDX;
        ppu_state->NEXT_OAM_IDX = 0;
        return; // idle cycle
    }

    if(ppu_state->ticks <= 256) {

        if(ppu_state->ticks == 64) {
            for(int sprite = 0; sprite<8; sprite++) {
                init_secondary_oam_sprite(sprite);
            }
        } else if(ppu_state->ticks > 64 && ppu_state->ticks < 191 && (ppu_state->ticks & 0x0001)) {
            second_oam_eval_loop((ppu_state->ticks - 65) >> 1); // we evaluate the sprites
        }

        render_pixel();

        if(!(ppu_state->ticks & 0x0001)) {
            switch(((ppu_state->ticks >> 1) - 1) % 4) {
            case 0:
                fetch_nt_byte();
                break;
            case 1:
                fetch_attr_byte();
            case 2:
                fetch_low_pt_byte();
                break;
            default:
                fetch_high_pt_byte();
                set_regs_next_tile();
                ppu_mem->increment_coarse_x_vram_addr();
                break;
            }
        }

    } else if(ppu_state->ticks == 257) {

        // update vram address
        ppu_mem->increment_y_vram_addr(); // slight imprecision : should be done on tick 256
        ppu_state->VRAM_ADDRESS &= 0xFBE0;
		ppu_state->VRAM_ADDRESS |= ppu_state->T_VRAM_ADDRESS & 0x041F;

    } else if(ppu_state->ticks <= 320) {
        // sprite regs
        UINT8 sprite_idx = (ppu_state->ticks - 257) / 8;
        UINT8 tick_offset = (ppu_state->ticks - 257) % 8;
        if(sprite_idx >= ppu_state->NEXT_OAM_IDX) return;
        // if(ppu_state->SPRITE_SIZE) throw NotImplemented("8x16 mode");
        
        switch(tick_offset) {
        case 1:
            ppu_state->current_oame = ppu_mem->secondary_oam[sprite_idx];
            compute_fine_Y_sprite();
            break;
        case 2:
            //we load attribute
            ppu_state->SPRITE_ATTR[sprite_idx] = ppu_state->current_oame.attr & 0x03;
            ppu_state->SPRITE_PRIORITY[sprite_idx] = !!(ppu_state->current_oame.attr & 0x20);
            break;
        case 3:
            //x coordinates
            ppu_state->SPRITE_POS[sprite_idx] = ppu_state->current_oame.X_off;
            ppu_state->SPRITE_ACTIVE[sprite_idx] = !(ppu_state->current_oame.X_off) * 8;
            break;
        case 5:
            // pt low
            ppu_state->SPRITE_LOW_REG[sprite_idx] = sprite_fetch_pt(ppu_state->current_oame.tile_idx, ppu_state->SPRITE_FINE_Y);
            if(ppu_state->current_oame.attr & 0x40) ppu_state->SPRITE_LOW_REG[sprite_idx] = reverse_byte(ppu_state->SPRITE_LOW_REG[sprite_idx]);
            break;

        case 7:
            //pt high
            ppu_state->SPRITE_HIGH_REG[sprite_idx] = sprite_fetch_pt(ppu_state->current_oame.tile_idx, 8 + ppu_state->SPRITE_FINE_Y);
            if(ppu_state->current_oame.attr & 0x40) ppu_state->SPRITE_HIGH_REG[sprite_idx] = reverse_byte(ppu_state->SPRITE_HIGH_REG[sprite_idx]);
            break;
        default:
            break;
        }
    } else if(ppu_state->ticks <= 336) {
        step_two_first_tiles();
    }

}

void PPU_Render::compute_fine_Y_sprite() {
    int max_rev = (ppu_state->SPRITE_SIZE)? 15 : 7;
    ppu_state->SPRITE_FINE_Y = (ppu_state->current_oame.attr & 0x80)? max_rev + ppu_state->current_oame.Y_off - ppu_state->scanline : ppu_state->scanline - ppu_state->current_oame.Y_off;
}

void PPU_Render::set_regs_first_tile() {
    if(ppu_state->ATTR_BYTE & 0x01) ppu_state->BG_ATTR_REG_LOW = 0xFF00; else ppu_state->BG_ATTR_REG_LOW = 0x0000;
    if(ppu_state->ATTR_BYTE & 0x02) ppu_state->BG_ATTR_REG_HIGH = 0xFF00; else ppu_state->BG_ATTR_REG_HIGH = 0x0000;
    ppu_state->BG_TILE_REG_LOW = ppu_state->LOW_PT_BYTE << 8;
    ppu_state->BG_TILE_REG_HIGH = ppu_state->HIGH_PT_BYTE << 8;
}

void PPU_Render::set_regs_next_tile() {
    if(ppu_state->ATTR_BYTE & 0x01) ppu_state->BG_ATTR_REG_LOW |= 0x00FF; else ppu_state->BG_ATTR_REG_LOW |= 0x0000;
    if(ppu_state->ATTR_BYTE & 0x02) ppu_state->BG_ATTR_REG_HIGH |= 0x00FF; else ppu_state->BG_ATTR_REG_HIGH |= 0x0000;
    ppu_state->BG_TILE_REG_LOW |= ppu_state->LOW_PT_BYTE;
    ppu_state->BG_TILE_REG_HIGH |= ppu_state->HIGH_PT_BYTE;
}

void PPU_Render::render_pixel() {
    UINT8 bg_sub_attr, pixel_color;

    sprite_res sprite_r = choose_sprite_pixel();
    UINT8 sprite = sprite_r.sprite;

    bg_sub_attr = compute_background_sub_attr();

    if(ppu_state->SHOW_SPRITE && sprite < 8 && (!ppu_state->SPRITE_PRIORITY[sprite] || !bg_sub_attr)) {
        // render sprite
        pixel_color = sprite_r.color;

        if(!ppu_state->SPRITE0HIT && ppu_state->SHOW_BACKGROUND && !sprite && ppu_state->ticks >= 3 && bg_sub_attr && sprite_r.sub_attr) {
            // test for sprite 0 hit
            ppu_state->SPRITE0HIT = 1;
        }

    } else if(ppu_state->SHOW_BACKGROUND) {
        // render background
        pixel_color = render_background_pixel(bg_sub_attr);

        // test for sprite 0 hit
        if(!ppu_state->SPRITE0HIT && ppu_state->SHOW_SPRITE && !sprite && ppu_state->ticks >= 3 && bg_sub_attr && sprite_r.sub_attr) {
            ppu_state->SPRITE0HIT = 1;
        }

    } else {
        pixel_color = ppu_mem->BACKGROUND_palette[0][0];
    }

    // update regs
    update_background_regs();
    update_sprite_regs();

    // display the pixel
    color3 sdl_color = color_from_uint8(pixel_color);
    set_surface_pixel(screen_surface, sdl_ctx->block_size, ppu_state->ticks - 1, ppu_state->scanline, SDL_MapRGB(sdl_ctx->format, sdl_color.r, sdl_color.g, sdl_color.b));
}


PPU_Render::sprite_res PPU_Render::choose_sprite_pixel() {
    UINT8 sub_attr, color;
    for(int sprite = 0; sprite < ppu_state->SEC_OAM_IDX; sprite++) {
        if(ppu_state->SPRITE_ACTIVE[sprite]) {
            sub_attr = (!!(ppu_state->SPRITE_LOW_REG[sprite] & 0x80)) | ((!!(ppu_state->SPRITE_HIGH_REG[sprite] & 0x80)) << 1);
            if(sub_attr) {
                // we return this one !
                color = ppu_mem->SPRITE_palette[ppu_state->SPRITE_ATTR[sprite]][sub_attr];
                return {(UINT8)sprite, color, sub_attr};
            }
            // else, go on to the next one
        }
    }
    return {8, 0x00, 0x00};
}

void PPU_Render::update_background_regs() {
    ppu_state->BG_TILE_REG_LOW <<= 1;
    ppu_state->BG_TILE_REG_HIGH <<= 1;
    ppu_state->BG_ATTR_REG_LOW <<= 1;
    ppu_state->BG_ATTR_REG_HIGH <<= 1;
}

void PPU_Render::update_sprite_regs() {
    for(int sprite = 0; sprite < ppu_state->SEC_OAM_IDX; sprite++) {
        if(ppu_state->SPRITE_ACTIVE[sprite]) {
            ppu_state->SPRITE_ACTIVE[sprite]--;
            ppu_state->SPRITE_LOW_REG[sprite] <<= 1;
            ppu_state->SPRITE_HIGH_REG[sprite] <<= 1;
        } else {
            if(!(--ppu_state->SPRITE_POS[sprite])) {
                // the sprite becomes active !
                ppu_state->SPRITE_ACTIVE[sprite] = 8;
            }
        }
    }
}

UINT8 PPU_Render::compute_background_sub_attr() {
    return BITSELECT16(ppu_state->BG_TILE_REG_LOW, 15 - ppu_state->FINE_X) | (BITSELECT16(ppu_state->BG_TILE_REG_HIGH, 15 - ppu_state->FINE_X) << 1);
}

UINT8 PPU_Render::render_background_pixel(UINT8 sub_attr) {
    UINT8 attr = BITSELECT16(ppu_state->BG_ATTR_REG_LOW, 15 - ppu_state->FINE_X) | (BITSELECT16(ppu_state->BG_ATTR_REG_HIGH, 15 - ppu_state->FINE_X) << 1); 
    UINT8 color = (sub_attr)? ppu_mem->BACKGROUND_palette[attr][sub_attr] : ppu_mem->BACKGROUND_palette[0][0];

    return color;
}


void PPU_Render::fetch_nt_byte() {
    MEMADDR tile_address = 0x2000 | (ppu_state->VRAM_ADDRESS & 0x0FFF);
    ppu_state->NT_BYTE = ppu_mem->read_nt(tile_address) << 4;
}

void PPU_Render::fetch_low_pt_byte() {
    UINT16 fine_Y = (ppu_state->VRAM_ADDRESS & 0x7000) >> 12;
    ppu_state->LOW_PT_BYTE = ppu_mem->read_ppu_pt(ppu_state->NT_BYTE + fine_Y + ((ppu_state->BACKGROUND_TABLE) << 12));
}

void PPU_Render::fetch_high_pt_byte() {
    UINT16 fine_Y = (ppu_state->VRAM_ADDRESS & 0x7000) >> 12;
    ppu_state->HIGH_PT_BYTE = ppu_mem->read_ppu_pt(ppu_state->NT_BYTE + 8 + fine_Y + ((ppu_state->BACKGROUND_TABLE) << 12));
}

void PPU_Render::fetch_attr_byte() {
    MEMADDR attr_address = 0x23C0 | (ppu_state->VRAM_ADDRESS & 0x0C00) | ((ppu_state->VRAM_ADDRESS >> 4) & 0x38) | ((ppu_state->VRAM_ADDRESS >> 2) & 0x07);
    UINT8 full_attr = ppu_mem->read_ppu_mem(attr_address);
    UINT16 coarse_y = (ppu_state->VRAM_ADDRESS >> 5) & 0x1F;
    UINT16 coarse_x = ppu_state->VRAM_ADDRESS & 0x1F;
    UINT8 sub_block_idx = ((coarse_x & 0x02) >> 1) | (coarse_y & 0x02);

    ppu_state->ATTR_BYTE = (full_attr >> (sub_block_idx << 1)) & 0x03;
}

void PPU_Render::init_secondary_oam_sprite(UINT8 sprite) {
    ppu_mem->secondary_oam[sprite].Y_off = 0xFF; // each of these should take 2 ppu cycles
    ppu_mem->secondary_oam[sprite].tile_idx = 0xFF;
    ppu_mem->secondary_oam[sprite].attr = 0xFF;
    ppu_mem->secondary_oam[sprite].X_off = 0xFF;
}

void PPU_Render::second_oam_eval_loop(UINT8 sprite) {
    if(ppu_state->NEXT_OAM_IDX >= 8) return;

    int tile_y_size = (ppu_state->SPRITE_SIZE)? 16 : 8;
    UINT8 i = ppu_state->NEXT_OAM_IDX;
    struct OAMentry *oame = ppu_mem->get_oam_entry(sprite);
    UINT8 y = oame->Y_off;
    ppu_mem->secondary_oam[i].Y_off = y;
    if(ppu_state->scanline >= y && ppu_state->scanline < y + tile_y_size) {
        // we copy the oam entry into the secondary oam
        ppu_mem->secondary_oam[i].tile_idx = oame->tile_idx;
        ppu_mem->secondary_oam[i].attr = oame->attr;
        ppu_mem->secondary_oam[i].X_off = oame->X_off;

        ppu_state->SPRITE_IDS[i] = sprite;
        ppu_state->NEXT_OAM_IDX++;

    }
}

UINT8 PPU_Render::sprite_fetch_pt(UINT8 tile_idx, UINT8 offset) {
    if(ppu_state->SPRITE_SIZE) {
        UINT8 nr_table = tile_idx & 0x01;
        // tile_idx--; // useless
        UINT8 real_idx = (tile_idx & 0xE0) + ((tile_idx & 0x1F) >> 1);
        if(ppu_state->SPRITE_FINE_Y) real_idx += 0x10; // + 16
        return ppu_mem->read_ppu_mem((real_idx << 4) + offset + (nr_table << 12));
    } else {
        return ppu_mem->read_ppu_pt((tile_idx << 4) + offset + ((ppu_state->SPRITE_TABLE) << 12));
    }
}

void PPU_Render::set_debug_mode(PPU_DEBUG_MODE debug) {
    debug_mode = debug;
}

UINT8 PPU_Render::reverse_byte(UINT8 b) {
    //Thx sth on stack overflow
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

// =====================================================
// ==================
// =====================================================

/* below, unnecessary stuff, but cool for debug */






















void PPU_Render::sprite_fetches(UINT8 sprite) {
    // 'sprite' is the secondary oam idx
    if(ppu_state->NEXT_OAM_IDX <= sprite) return; // not actually loaded

    struct OAMentry oame = ppu_mem->secondary_oam[sprite];
    int max_rev = (ppu_state->SPRITE_SIZE)? 15 : 7;
    UINT8 fine_Y = (oame.attr & 0x80)? max_rev + oame.Y_off - ppu_state->scanline : ppu_state->scanline - oame.Y_off ; // do we flip vertically ?
    
    ppu_state->SPRITE_LOW_REG[sprite] = sprite_fetch_pt(oame.tile_idx, fine_Y);
    ppu_state->SPRITE_HIGH_REG[sprite] = sprite_fetch_pt(oame.tile_idx, fine_Y + 8);

    if(oame.attr & 0x40) {
        // swap horizontally
        ppu_state->SPRITE_LOW_REG[sprite] = reverse_byte(ppu_state->SPRITE_LOW_REG[sprite]);
        ppu_state->SPRITE_HIGH_REG[sprite] = reverse_byte(ppu_state->SPRITE_HIGH_REG[sprite]);
    }

    ppu_state->SPRITE_PRIORITY[sprite] = !!(oame.attr & 0x20);
    ppu_state->SPRITE_POS[sprite] = oame.X_off;
    ppu_state->SPRITE_ACTIVE[sprite] = !(oame.X_off) * 8; // active if X offset is 0
    ppu_state->SPRITE_ATTR[sprite] = oame.attr & 0x03;
}

void PPU_Render::render() {
    /* essentially for debug, the real thing should be done with step() */

    SDL_Texture *texture;
    UINT8 ppu_table_background = ppu_state->BACKGROUND_TABLE;

    ppu_state->SPRITE0HIT = 0;
    ppu_state->IN_VBLANK = 0;
    ppu_state->SEC_OAM_IDX = 0;
    ppu_state->NEXT_OAM_IDX = 0;
    ppu_state->VRAM_ADDRESS = ppu_state->T_VRAM_ADDRESS;


    //*** Load information about 1st tile

    fetch_first_tile_attr();

    ppu_mem->increment_coarse_x_vram_addr();

    //*** Load information about 2nd tile

    fetch_next_tile_attr();

    ppu_mem->increment_coarse_x_vram_addr();

    // ppu_state->scanlines
    for(ppu_state->scanline = 0; ppu_state->scanline < 240; ppu_state->scanline++) {

        ppu_state->SEC_OAM_IDX = ppu_state->NEXT_OAM_IDX;
        ppu_state->NEXT_OAM_IDX = 0;
        for(int sprite = 0; sprite<8; sprite++) {
            init_secondary_oam_sprite(sprite);
        }
        for(int sprite = 0; sprite<64; sprite++) {
            second_oam_eval_loop(sprite);
        }

        for(tile = 0; tile < 32; tile++) {
            for(in_tile = 0; in_tile < 8; in_tile++) {
                // render pixel, and update regs
                ppu_state->ticks = 1+(in_tile+8*(tile));
                render_pixel();
            }
            // time to feed registers

            fetch_next_tile_attr();

            ppu_mem->increment_coarse_x_vram_addr();
        }

        // do sprite fetches

        for(int sprite=0; sprite < ppu_state->NEXT_OAM_IDX; sprite++) {
            sprite_fetches(sprite);
        }


        // time to feed the two first tiles

        ppu_mem->increment_y_vram_addr();

        // copy all bits related to x position
        ppu_state->VRAM_ADDRESS &= 0xFBE0;
		ppu_state->VRAM_ADDRESS |= ppu_state->T_VRAM_ADDRESS & 0x041F;

        fetch_first_tile_attr();

        ppu_mem->increment_coarse_x_vram_addr();

        fetch_next_tile_attr();

        ppu_mem->increment_coarse_x_vram_addr();
    }

    ppu_state->IN_VBLANK = 1;
    texture = SDL_CreateTextureFromSurface(sdl_ctx->renderer, screen_surface);
    SDL_RenderCopy(sdl_ctx->renderer, texture, NULL, NULL);
    SDL_DestroyTexture(texture);
    SDL_RenderPresent(sdl_ctx->renderer);
}





void PPU_Render::fetch_first_tile_attr() {
    // fetch pattern table data
    
    fetch_nt_byte();
    fetch_attr_byte();
    fetch_low_pt_byte();
    fetch_high_pt_byte();
    set_regs_first_tile();

    
}


void PPU_Render::fetch_next_tile_attr() {
    
    fetch_nt_byte();
    fetch_attr_byte();
    fetch_low_pt_byte();
    fetch_high_pt_byte();
    set_regs_next_tile();
}


/* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= 
        DEBUG STUFF
   =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/
void PPU_Render::draw_debug_tiles_grid(UINT8 half_size) {
    SDL_Surface *surf = nullptr;
    SDL_Texture *text = nullptr;
    SDL_Rect r = {0, 0, 0, 0};
    int b_size = sdl_ctx->block_size;
    for(int i=8; i<256; i+=8) {
        r = {0, 0, 2*half_size, 240*b_size};
        if(!(i % 32)) r.w *= 2;
        surf = SDL_CreateRGBSurface(0, r.w, r.h, 32, 0, 0, 0, 0);
        SDL_FillRect(surf, &r, SDL_MapRGB(sdl_ctx->format, 255, 255, 255));
        r.x = (i*b_size)-half_size;
        text = SDL_CreateTextureFromSurface(sdl_ctx->renderer, surf);
        SDL_FreeSurface(surf);
        SDL_RenderCopy(sdl_ctx->renderer, text, NULL, &r);
        SDL_DestroyTexture(text);
    }
    for(int i=8; i<240; i+=8) {
        r = {0, 0, 256*b_size, 2*half_size};
        if(!(i % 32)) r.h *= 2;
        surf = SDL_CreateRGBSurface(0, r.w, r.h, 32, 0, 0, 0, 0);
        SDL_FillRect(surf, &r, SDL_MapRGB(sdl_ctx->format, 255, 255, 255));
        r.y = (i*b_size)-half_size;
        text = SDL_CreateTextureFromSurface(sdl_ctx->renderer, surf);
        SDL_FreeSurface(surf);
        SDL_RenderCopy(sdl_ctx->renderer, text, NULL, &r);
        SDL_DestroyTexture(text);
    }
    SDL_RenderPresent(sdl_ctx->renderer);
}

SDL_Window *PPU_Render::open_pattern_table_window() {
    SDL_Window *tiles_window = nullptr;
    SDL_Renderer *tiles_renderer = nullptr;
    int b_size = sdl_ctx->block_size;
    int tile_size = 8*b_size;
    palette pal;
    SDL_Surface *surface = nullptr;
    SDL_Texture *texture = nullptr;
    int low = 0, high = 0, bit = 0, x_tile, y_tile, sub_attr, color;
    color3 sdl_color;
    SDL_Rect r = {0, 0, 0, 0};

    if(init_window_renderer("Tiles pattern tables", &tiles_window, &tiles_renderer, 32*8*b_size, 16*8*b_size) < 0) {
        std::printf("Tiles window opening failed : %s\n", SDL_GetError());
        return nullptr;
    }

    pal[0] = 0x0F; pal[1] = 0x07; pal[2] = 0x27; pal[3] = 0x37;

    surface = SDL_CreateRGBSurface(0, 32*8*b_size, 16*8*b_size, 32, 0, 0, 0, 0);
    if(!surface) {
        std::printf("Creation of surface failed : %s\n", SDL_GetError());
        return nullptr;
    }

    for(int pt = 0; pt < 2; pt++) {
        for(int tile = 0; tile < 256; tile++)
        {

            for(int y = 0; y < 8; y++) {
                low = ppu_mem->read_ppu_pt((tile << 4) + y + (pt << 12));
                high = ppu_mem->read_ppu_pt((tile << 4) + 8 + y + (pt << 12));
                for(bit = 7; bit >= 0; bit--) {
                    x_tile = tile % 16;
                    y_tile = tile / 16;
                    r = {((x_tile + pt*16)*8+bit) * b_size, (y_tile*8+y) * b_size, b_size, b_size};
                    sub_attr = BITSELECT8(low, 7-bit) | (BITSELECT8(high, 7-bit) << 1);

                    color = pal[sub_attr];
                    sdl_color = color_from_uint8(color);
                    SDL_FillRect(surface, &r, SDL_MapRGB(sdl_ctx->format, sdl_color.r, sdl_color.g, sdl_color.b));
                }

            }
        }
    }

    texture = SDL_CreateTextureFromSurface(tiles_renderer, surface);
    SDL_RenderCopy(tiles_renderer, texture, NULL, NULL);
    SDL_RenderPresent(tiles_renderer);
    return tiles_window;
}