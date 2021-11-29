#ifndef PPU_INFO_GAYANES_HPP
#define PPU_INFO_GAYANES_HPP
#include "types.hpp"
#include "cpu.hpp"


typedef enum {
    MIRORING_HORIZONTAL, MIRORING_VERTICAL, MIRORING_SINGLE, MIRORING_FOUR
} screen_miroring;

struct OAMentry {
    UINT8 Y_off;
    UINT8 tile_idx;
    UINT8 attr;
    UINT8 X_off;
};

struct OAM {
    struct OAMentry     entries[64];
    UINT8               OAMaddr; //current OAM addr
    MEMADDR             OAMDMA;
};

/*
The first 960 (0x3C0) bytes correspond to the nametable
The next (last) 64 (0x40) bytes correspond to the attributes
Total : 1024 (0x400) bytes
*/
typedef UINT8 PPU_NT[1024];


/*
=================
PPU STATE
=================
*/
struct PPU_state {

    BOOL            NMI_VBLANK;
    BOOL            SPRITE_SIZE; // 0 = 8x8

    UINT8           SPRITE_TABLE; //0 for $0000, 1 for $1000 (ignored in 8x16 mode)
    UINT8           BACKGROUND_TABLE; //0 for $0000, 1 for $1000


    BOOL            SHOW_SPRITE;
    BOOL            SHOW_BACKGROUND;

    BOOL            GREYSCALE;
    BOOL            SPRITE_CORNER;
    BOOL            BACKGROUND_CORNER;


    BOOL            IN_VBLANK;
    BOOL            EVEN_FRAME;

    // scrolling
    /*
     (0,0)     (256,0)     (511,0)
       +-----------+-----------+
       |           |           |
       |           |           |
       |   $2000   |   $2400   |
       |           |           |
       |           |           |
(0,240)+-----------+-----------+(511,240)
       |           |           |
       |           |           |
       |   $2800   |   $2C00   |
       |           |           |
       |           |           |
       +-----------+-----------+
     (0,479)   (256,479)   (511,479)
    */

    BOOL            WRITE_TOGGLE; // 'w'
    MEMADDR         VRAM_ADDRESS; // 'v'
    MEMADDR         internal_vram;
    MEMADDR         T_VRAM_ADDRESS; // 't'
    UINT8           FINE_X; // 'x', only 3 bits will be used

    MEMADDR         VRAM_INCREMENT;

    /* BACKGROUND RENDERING RELATED */

    UINT16          BG_TILE_REG_LOW;
    UINT16          BG_TILE_REG_HIGH;

    UINT16          BG_ATTR_REG_LOW;
    UINT16          BG_ATTR_REG_HIGH;

    UINT16          NT_BYTE;
    UINT16          LOW_PT_BYTE;
    UINT16          HIGH_PT_BYTE;
    UINT16          ATTR_BYTE;
    
    /* SPRITE RENDERING/EVALUATION RELATED */

    UINT8           SEC_OAM_IDX;
    struct OAMentry current_oame;
    UINT16          SPRITE_FINE_Y;
    UINT8           NEXT_OAM_IDX;
    UINT8           SPRITE_LOW_REG[8];
    UINT8           SPRITE_HIGH_REG[8];
    UINT8           SPRITE_ATTR[8];
    UINT8           SPRITE_PRIORITY[8];
    UINT8           SPRITE_ACTIVE[8];
    UINT8           SPRITE_IDS[8];
    UINT8           SPRITE_POS[8];

    /* INFORMATION ABOUT RENDERING */

    BOOL            SPRITE0HIT;
    BOOL            SPRITE_OVERFLOW;

    UINT16          ticks;
    UINT16          scanline;

};

PPU_state *ppu_state_save(PPU_state &p);

// ** PALETTES

typedef UINT8 palette[4];


/*
=================
PPU MEMORY (including OAM)
=================
*/

class PPU_mem
{
private:

    /* Correspond to PHYSICAL nametables, so very often, only two will be used
    (but we have lots of memory so we can waste a few ko yay) */
    PPU_NT              NT[4]; 

    /* this is used to remember which address space block correspond to which actual (physical) nametable */
    UINT8               NT_correspondance[4];

    UINT8               PPUDATA_read_buffer;


    UINT8               last_bits_written; // to be returned by PPUSTATUS


    struct OAM          primary_oam;

public:
    PPU_mem(struct PPU_state *ppu_state);
    PPU_mem(PPU_mem &p);
    ~PPU_mem();

    // ** Palettes
    palette             BACKGROUND_palette[4];
    palette             SPRITE_palette[4];

    struct OAMentry     secondary_oam[8];

    void                set_screen_miroring(screen_miroring scrmir);

    struct PPU_state    *ppu_state;
    cpu6502             *cpu;
    ROMMemManager       *rom;


    void                increment_coarse_x_vram_addr();
    void                increment_y_vram_addr();
    void                set_ppu_state(PPU_state *s){ppu_state = s;};

    void                print_debug();
    void                print_debug_sprite(int sprite_nr);

    PPU_NT              *get_nt() {return NT;};
    // nt_nr should be between 0 and 3
    UINT8               get_real_nt(UINT8 nt_nr) {return NT_correspondance[nt_nr];};
    struct OAMentry     *get_oam_entry(UINT8 n) {return &(primary_oam.entries[n]);};

    void                write_ppu_pt(MEMADDR a, UINT8 val){rom->write_pt(a, val);};
    UINT8               read_ppu_pt(MEMADDR a){return rom->read_pt(a);};

    void    write_PPUCTRL(UINT8 ctrl);
    void    write_PPUMASK(UINT8 mask);
    UINT8   read_PPUSTATUS();
    void    write_OAMADDR(UINT8 oamaddr);
    void    write_OAMDATA(UINT8 oamdata);
    UINT8   read_OAMDATA();
    void    write_PPUSCROLL(UINT8 scrollval);
    void    write_PPUADDR(UINT8 ppuaddr);
    void    write_PPUDATA(UINT8 ppudata);
    UINT8   read_PPUDATA();
    void    write_OAMDMA(UINT8 oamdma);

    void    write_nt(MEMADDR a, UINT8 val);
    UINT8   read_nt(MEMADDR a);

    void    load_dma();

    void    write_ppu_mem(MEMADDR addr, UINT8 memaddr);
    UINT8   read_ppu_mem(MEMADDR addr);
};

#endif