#ifndef GAYA_INES_HPP
#define GAYA_INES_HPP

#include <stdio.h>
#include "../types.hpp"

/*
NES loader structures
*/

struct nes_header {
    UINT8   nb_PRG_ROM; // in 16 KB units
    UINT8   nb_CHR_ROM; // in 8 KB units
    BOOL    SCREEN_MIRRORING; // 0 for horizontal, 1 for vertical
    BOOL    BB_PRG_RAM; // 1 if cartridge contains battery-backed PRG RAM ($6000-$7FFF)
    BOOL    TRAINER; // 1 if 512-bytes trainer at $7000-$71FF
    BOOL    FOUR_SCREENS; // 1 if mirror control ignored, and 4 screens VRAM
    UINT8   MAPPER_NB; // mapper number

    // TODO : NES 2.0

    BOOL    NES2;
    BOOL    CONSOLE_TYPE;
    
};

struct nes_data {
    struct nes_header   header;
    UINT8               *TRAINER; //null if not present, as indicated in header
                                  //512 bytes otherwise

    unsigned int        PRG_ROM_size; // 16 KB * header.nb_PRG_ROM
    UINT8               *PRG_ROM_data;

    unsigned int        CHR_ROM_size; // 8 KB * header.nb_CHR_ROM
    UINT8               *CHR_ROM_data;

    UINT8               *INST_ROM; // null or 8192 bytes long
    UINT8               *PROM; // often missing, not considered now

    char                TITLE[129]; //null-terminated title, not always present
};


/* Read the header of a nes file, in format FILE *
The file should be opened with r mode.

Can throw:
- FileReadingError
- IncorrectFormat
 */
struct nes_header read_nes_header(FILE *nesfile);

/* Read the data of a nes file, in format FILE * opened in r mode,
given the struct nes_header (as returned by read_nes_header) 

Can throw:
- FileReadingError
- IncorrectFormat
- MemAllocFailed*/
struct nes_data   read_nes_data(FILE *nesfile, struct nes_header header);

void print_ines_header_info(struct nes_header &h);

#endif