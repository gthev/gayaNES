#include <iostream>
#include "ppu_info.hpp"

PPU_state *ppu_state_save(PPU_state &p) {
    PPU_state *cloned = new PPU_state;
    
    cloned->VRAM_ADDRESS = p.VRAM_ADDRESS;
    cloned->internal_vram = p.internal_vram;
    cloned->VRAM_INCREMENT = p.VRAM_INCREMENT;
    cloned->EVEN_FRAME = p.EVEN_FRAME;
    cloned->WRITE_TOGGLE = p.WRITE_TOGGLE;
    cloned->T_VRAM_ADDRESS = p.T_VRAM_ADDRESS;
    cloned->FINE_X = p.FINE_X;
    cloned->BG_TILE_REG_LOW = p.BG_TILE_REG_LOW; cloned->BG_TILE_REG_HIGH = p.BG_TILE_REG_HIGH; 
    cloned->BG_ATTR_REG_LOW = p.BG_ATTR_REG_LOW; cloned->BG_ATTR_REG_HIGH = p.BG_ATTR_REG_HIGH;
    cloned->NT_BYTE = p.NT_BYTE; cloned->LOW_PT_BYTE = p.LOW_PT_BYTE; cloned->HIGH_PT_BYTE = p.HIGH_PT_BYTE; cloned->ATTR_BYTE = p.ATTR_BYTE;
    cloned->SPRITE0HIT = p.SPRITE0HIT;
    cloned->SPRITE_OVERFLOW = p.SPRITE_OVERFLOW;
    cloned->SPRITE_SIZE = p.SPRITE_SIZE;

    cloned->IN_VBLANK = p.IN_VBLANK;
    cloned->NMI_VBLANK = p.NMI_VBLANK;

    cloned->GREYSCALE = p.GREYSCALE;
    cloned->SHOW_BACKGROUND = p.SHOW_BACKGROUND;
    cloned->SHOW_SPRITE = p.SHOW_SPRITE;

    cloned->SEC_OAM_IDX = p.SEC_OAM_IDX;
    cloned->current_oame.attr = p.current_oame.attr; cloned->current_oame.Y_off = p.current_oame.Y_off; cloned->current_oame.X_off = p.current_oame.X_off; cloned->current_oame.tile_idx = p.current_oame.tile_idx;

    cloned->SPRITE_FINE_Y = p.SPRITE_FINE_Y;
    cloned->NEXT_OAM_IDX = p.NEXT_OAM_IDX;

    cloned->ticks = p.ticks;
    cloned->scanline = p.scanline;

    for(int i=0; i<8; i++) {
        cloned->SPRITE_LOW_REG[i] = p.SPRITE_LOW_REG[i];
        cloned->SPRITE_HIGH_REG[i] = p.SPRITE_HIGH_REG[i];
        cloned->SPRITE_ATTR[i] = p.SPRITE_ATTR[i];
        cloned->SPRITE_PRIORITY[i] = p.SPRITE_PRIORITY[i];
        cloned->SPRITE_ACTIVE[i] = p.SPRITE_ACTIVE[i];
        cloned->SPRITE_IDS[i] = p.SPRITE_IDS[i];
        cloned->SPRITE_POS[i] = p.SPRITE_POS[i];
    }

    return cloned;
}

PPU_mem::PPU_mem(PPU_state *ppu_s) {

    /*
    Ok we init everything in PPU memory here
    */

    ppu_state = ppu_s;
    ppu_state->VRAM_ADDRESS = 0;
    ppu_state->internal_vram = 0;
    ppu_state->VRAM_INCREMENT = 1;
    ppu_state->EVEN_FRAME = 0;
    ppu_state->WRITE_TOGGLE = 0;
    ppu_state->T_VRAM_ADDRESS = 0;
    ppu_state->FINE_X = 0;
    ppu_state->BG_TILE_REG_LOW = 0; ppu_state->BG_TILE_REG_HIGH = 0; 
    ppu_state->BG_ATTR_REG_LOW = 0; ppu_state->BG_ATTR_REG_HIGH = 0;
    ppu_state->NT_BYTE = 0; ppu_state->LOW_PT_BYTE = 0; ppu_state->HIGH_PT_BYTE = 0; ppu_state->ATTR_BYTE = 0;
    ppu_state->SPRITE0HIT = 0;
    ppu_state->SPRITE_OVERFLOW = 0;
    ppu_state->SPRITE_SIZE = 0;
    last_bits_written = 0;

    ppu_state->IN_VBLANK = 1;
    ppu_state->NMI_VBLANK = 0;

    ppu_state->GREYSCALE = 0;
    ppu_state->SHOW_BACKGROUND = 0;
    ppu_state->SHOW_SPRITE = 0;

    PPUDATA_read_buffer = 0;

    ppu_state->SEC_OAM_IDX = 0;
    ppu_state->current_oame.attr = 0; ppu_state->current_oame.Y_off = 0; ppu_state->current_oame.X_off = 0; ppu_state->current_oame.tile_idx = 0;

    ppu_state->SPRITE_FINE_Y = 0;
    ppu_state->NEXT_OAM_IDX = 0;
    for(int i=0; i<8; i++) {
        ppu_state->SPRITE_LOW_REG[i] = 0;
        ppu_state->SPRITE_HIGH_REG[i] = 0;
        ppu_state->SPRITE_ATTR[i] = 0;
        ppu_state->SPRITE_PRIORITY[i] = 0;
        ppu_state->SPRITE_ACTIVE[i] = 0;
        ppu_state->SPRITE_IDS[i] = 0;
        ppu_state->SPRITE_POS[i] = 0;
    }

    primary_oam.OAMaddr = 0;
    primary_oam.OAMDMA = 0;
    for(int i=0; i<64; i++) {
        primary_oam.entries[i] = {0, 0, 0, 0};
    }
    for(int i=0; i<4; i++) {
        for(int x=0; x<1024; x++) {
            NT[i][x] = 0x00;
        }
    }
    for(int a=0; a<4; a++) {
        for(int b=0; b<4; b++) {
            BACKGROUND_palette[a][b] = 0x00;
            SPRITE_palette[a][b] = 0x00;
        }
    }
}

static void copy_oam(struct OAM &src, struct OAM &dst) {
    dst.OAMaddr = src.OAMaddr;
    dst.OAMDMA = src.OAMDMA;
    for(int i=0; i<64; i++) {
        dst.entries[i] = src.entries[i];
    }
}

PPU_mem::PPU_mem(PPU_mem &p) {
    // ppu state will be handled else where
    ppu_state = nullptr;
    // Nametables
    for(int nt=0; nt<4; nt++) {
        for(int i=0; i<1024; i++) {
            NT[nt][i] = p.NT[nt][i];
        }
    }
    for(int i=0; i<4; i++) {
        NT_correspondance[i] = p.NT_correspondance[i];
    }
    PPUDATA_read_buffer = p.PPUDATA_read_buffer;
    last_bits_written = p.last_bits_written;
    // oam
    copy_oam(p.primary_oam, primary_oam);
    for(int i=0; i<8; i++) {
        secondary_oam[i] = p.secondary_oam[i];
    }
    // finally, palettes
    for(int i=0; i<4; i++) {
        for(int j=0; j<4; j++) {
            BACKGROUND_palette[i][j] = p.BACKGROUND_palette[i][j];
            SPRITE_palette[i][j] = p.SPRITE_palette[i][j];
        }
    }
}

PPU_mem::~PPU_mem() {
}

void PPU_mem::write_PPUCTRL(UINT8 ctrl) {
    BOOL vram_inc = ctrl & 0x04;
    BOOL has_nmi = !!(ctrl & 0x80);

    ppu_state->T_VRAM_ADDRESS &= 0x73FF; // we set bits 10 and 11 off
    ppu_state->T_VRAM_ADDRESS |= ((MEMADDR)(ctrl & 0x03)) << 10; 
    
    ppu_state->VRAM_INCREMENT = 8*vram_inc + !(vram_inc);
    ppu_state->SPRITE_TABLE = !!(ctrl & 0x08);
    ppu_state->BACKGROUND_TABLE = !!(ctrl & 0x10);
    ppu_state->SPRITE_SIZE = !!(ctrl & 0x20);
    // TODO : PPU master/slave ?

    if(ppu_state->IN_VBLANK && !ppu_state->NMI_VBLANK && has_nmi) {
        // immediately triggers an NMI
        cpu->enter_nmi();
    }

    ppu_state->NMI_VBLANK = has_nmi;
    last_bits_written = ctrl & 0x1F;
}

void PPU_mem::write_PPUMASK(UINT8 mask) {
    ppu_state->GREYSCALE = mask & 0x01;
    ppu_state->BACKGROUND_CORNER = !!(mask & 0x02);
    ppu_state->SPRITE_CORNER = !!(mask & 0x04);
    ppu_state->SHOW_BACKGROUND = !!(mask & 0x08);
    ppu_state->SHOW_SPRITE = !!(mask & 0x10);
    // TODO : emphasized colors ?

    last_bits_written = mask & 0x1F;
}

UINT8 PPU_mem::read_PPUSTATUS() {
    UINT8 res = last_bits_written;
    /*
    Ok so Sprite overflow : TODO,
    */
   res &= 0x1F;
   res |= (!!(ppu_state->IN_VBLANK)) << 7;
   res |= (!!(ppu_state->SPRITE0HIT)) << 6;
   res |= (!!(ppu_state->SPRITE_OVERFLOW)) << 5;

   ppu_state->IN_VBLANK = 0;
   ppu_state->WRITE_TOGGLE = 0;

    //std::printf("Returning %02X\n", res);
   return res;
}

void PPU_mem::write_OAMADDR(UINT8 oamaddr) {
    primary_oam.OAMaddr = oamaddr;
    last_bits_written = oamaddr & 0x1F;
}

void PPU_mem::write_OAMDATA(UINT8 oamdata) {
    UINT8 oamaddr = primary_oam.OAMaddr;
    ((UINT8*)&(primary_oam.entries[oamaddr/4]))[oamaddr % 4] = oamdata;
    primary_oam.OAMaddr++;
    //last_bits_written = oamdata & 0x1F;
}

UINT8 PPU_mem::read_OAMDATA() {
    // TODO : if during ticks 1-64 of a scanline, return $FF
    UINT8 oamaddr = primary_oam.OAMaddr;
    return ((UINT8*)&(primary_oam.entries[oamaddr/4]))[oamaddr % 4];
}

void PPU_mem::write_PPUSCROLL(UINT8 scrollval) {
    UINT16 l_scrollval = scrollval;
    if(ppu_state->WRITE_TOGGLE) {
        // set y
        ppu_state->T_VRAM_ADDRESS &= 0x0C1F; // we set the appropriate bits off
        ppu_state->T_VRAM_ADDRESS |= (l_scrollval & 0x07) << 12; // fine y
        ppu_state->T_VRAM_ADDRESS |= (l_scrollval & 0xF8) << 2; // coarse y
        ppu_state->WRITE_TOGGLE = 0;
    } else {
        // set x
        ppu_state->T_VRAM_ADDRESS &= 0xFFE0;
        ppu_state->FINE_X = l_scrollval & 0x07; // fine x
        ppu_state->T_VRAM_ADDRESS |= (l_scrollval & 0xF8) >> 3;
        ppu_state->WRITE_TOGGLE = 1;
    }
    last_bits_written = scrollval & 0x1F;
}

void PPU_mem::write_PPUADDR(UINT8 ppuaddr) {
    UINT16 l_ppuaddr = ppuaddr;
    if(ppu_state->WRITE_TOGGLE) {
        ppu_state->T_VRAM_ADDRESS &= 0x7F00;
        ppu_state->T_VRAM_ADDRESS |= l_ppuaddr;
        ppu_state->internal_vram = ppu_state->T_VRAM_ADDRESS;
        ppu_state->WRITE_TOGGLE = 0;
    } else {
        ppu_state->T_VRAM_ADDRESS &= 0x00FF; // bits 14 is also cleared
        ppu_state->T_VRAM_ADDRESS |= (l_ppuaddr & 0x3F) << 8;
        ppu_state->WRITE_TOGGLE = 1;
    }
    last_bits_written = ppuaddr & 0x1F;
}

void PPU_mem::write_PPUDATA(UINT8 ppudata) {
    write_ppu_mem(ppu_state->internal_vram, ppudata);
    ppu_state->internal_vram += ppu_state->VRAM_INCREMENT;
    //last_bits_written = ppudata & 0x1F;
}

UINT8 PPU_mem::read_PPUDATA() {
    UINT8 val = PPUDATA_read_buffer;
    PPUDATA_read_buffer = read_ppu_mem(ppu_state->internal_vram);
    ppu_state->internal_vram += ppu_state->VRAM_INCREMENT;
    return val;
}

void PPU_mem::write_OAMDMA(UINT8 oamdma) {
    primary_oam.OAMDMA = oamdma;
    load_dma();
    last_bits_written = oamdma & 0x1F;
}

// TODO : read/write_ppu_mem

/* writes byte to NT
address a is assumed to be between $2000 and $2FFF */
void PPU_mem::write_nt(MEMADDR a, UINT8 val) {
    //std::printf("writing 0x%02X at 0x%04X\n", val, a);
    UINT8 nr_table = (a & 0x0C00) >> 10; // this number is between 0 and 3
    NT[NT_correspondance[nr_table]][a & 0x03FF] = val;
}

UINT8 PPU_mem::read_nt(MEMADDR a) {
    UINT8 nr_table = (a & 0x0C00) >> 10; // this number is between 0 and 3
    return NT[NT_correspondance[nr_table]][a & 0x03FF];
}


/*
Read and write from PPU memory
*/

void PPU_mem::write_ppu_mem(MEMADDR a, UINT8 val) {
    /*
    TODO : for now we authorize write to PT, we oughta refuse it unless it is CHR-RAM
    */

    // if(a == 0x2125) std::printf("Writing 0x%02X to 0x2125\n", val);

    if (!(a & 0xE000)) // if a < 0x2000
        write_ppu_pt(a, val);

    else if (a < 0x3000) {
        write_nt(a, val);
    } else if(a < 0x3F00) 
        // mirror of $2000-$2EFF
        write_nt(a - 0x1000, val); // we remove $1000, could be done with & 0xEFFF
    else if(a < 0x3F20) {
        // palettes
        UINT8 subidx = (UINT8)(a & 0x000F);
        if(a < 0x3F10)
            BACKGROUND_palette[subidx >> 2][subidx & 0x03] = val;
        else {
            if(!(a & 0x0003))
                // mirror of the background palette
                BACKGROUND_palette[subidx >> 2][0] = val;
            else
                SPRITE_palette[subidx >> 2][subidx & 0x03] = val;
        }
        // end main palettes
    }
    else if(a < 0x4000) {
        // mirrors of palettes
        write_ppu_mem(((a - 0x3F20) % 0x0020)|0x3F00, val);
    } else {
        write_ppu_mem(a % 0x4000, val);
    }
}

UINT8 PPU_mem::read_ppu_mem(MEMADDR a) {

    if (!(a & 0xE000)) // if a < 0x2000
        return read_ppu_pt(a);

    else if (a < 0x3000) {
        return read_nt(a);
    } else if(a < 0x3F00) 
        // mirror of $2000-$2EFF
        return read_nt(a - 0x1000); // we remove $1000
    else if(a < 0x3F20) {
        // palettes
        UINT8 subidx = (UINT8)(a & 0x000F);
        if(a < 0x3F10)
            return BACKGROUND_palette[subidx >> 2][subidx & 0x03];
        else {
            if(!(a & 0x0003))
                // mirror of the background palette
                return BACKGROUND_palette[subidx >> 2][0];
            else
                return SPRITE_palette[subidx >> 2][subidx & 0x03];
        }
        // end main palettes
    }
    else if(a<0x4000){
        // mirrors of palettes
        return read_ppu_mem(((a - 0x3F20) % 0x0020)|0x3F00);
    } else {
        return read_ppu_mem(a % 0x4000);
    }
}

//*** DMA LOADING

void PPU_mem::load_dma() {
    MEMADDR a = primary_oam.OAMDMA * 256 + primary_oam.OAMaddr;
    MEMADDR max_a = a + 256;
    UINT8 oam_addr = 0;
    for(;a < max_a; a++) {
        ((UINT8*)&(primary_oam.entries[oam_addr/4]))[oam_addr % 4] = cpu->read_mem(a);
        oam_addr++;
    }
}

//***

void PPU_mem::increment_coarse_x_vram_addr() {
    UINT16 v = ppu_state->VRAM_ADDRESS;
    if((v & 0X001F) == 0x001F) {
        v &= 0xFFE0; // set coarse x to 0;
        v ^= 0x0400; // switch horizontal nametable
    } else 
        v += 1;
    ppu_state->VRAM_ADDRESS = v;
}

void PPU_mem::increment_y_vram_addr() {
    UINT16 v = ppu_state->VRAM_ADDRESS;
    if((v & 0x7000) == 0x7000) {
        v &= 0x0FFF;
        UINT16 y = (v & 0x3E0) >> 5; // y = coarse Y
        if(y == 29) {
            y = 0;
            v ^= 0x0800; // switch vertical nametable
        } else if(y == 31)
            y = 0; // no nametable switch
        else
            y += 1;
        
        v = (v & 0x7C1F) | (y << 5); // put back coarse Y

    } else {
        v += 0x1000; // increment fine Y
    }

    ppu_state->VRAM_ADDRESS = v;
}

//===

void PPU_mem::set_screen_miroring(screen_miroring scrmir) {
    NT_correspondance[0] = 0;
    switch (scrmir)
    {
    case MIRORING_HORIZONTAL:
        NT_correspondance[1] = 0;
        NT_correspondance[2] = 1;
        NT_correspondance[3] = 1;
        break;

    case MIRORING_VERTICAL:
        NT_correspondance[1] = 1;
        NT_correspondance[2] = 0;
        NT_correspondance[3] = 1;
        break;

    case MIRORING_SINGLE:
        NT_correspondance[1] = 0;
        NT_correspondance[2] = 0;
        NT_correspondance[3] = 0;
        break;

    case MIRORING_FOUR:
        NT_correspondance[1] = 1;
        NT_correspondance[2] = 2;
        NT_correspondance[3] = 3;
        break;

    default:
        break;
    }
}


// ============ DEBUG

void PPU_mem::print_debug_sprite(int sprite_nr) {
    struct OAMentry *sprite = get_oam_entry(sprite_nr);
    std::printf("----------\nSPRITE NR %d\n", sprite_nr);
    std::printf("Offset [%d, %d]\n", sprite->X_off, sprite->Y_off);
    std::printf("Attr %02X\n", sprite->attr);
    std::printf("Tile idx : [%d %d]\n", sprite->tile_idx % 16, sprite->tile_idx / 16);
    std::printf("----------\n");
}

void PPU_mem::print_debug() {
    std::printf("NT_correspondance[0]=%d\n", NT_correspondance[0]);
    // Background palette print
    std::printf("BACKGROUND PALETTE\n");
    for(int i=0; i<4; i++) {
        std::printf("Palette[%d]: ", i);
        for(int j=0; j<4; j++) {
            std::printf("0x%02X ", BACKGROUND_palette[i][j]);
        }
        std::printf("\n");
    }
    std::printf("SPRITE PALETTE\n");
    for(int i=0; i<4; i++) {
        std::printf("Palette[%d]: ", i);
        for(int j=0; j<4; j++) {
            std::printf("0x%02X ", SPRITE_palette[i][j]);
        }
        std::printf("\n");
    }
}