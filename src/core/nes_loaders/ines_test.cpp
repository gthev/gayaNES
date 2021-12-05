#include <stdio.h>
#include <stdexcept>
#include "../exceptions.hpp"
#include "ines.hpp"

/*
Small program to test iNES loader
donkey_kong.nes should be in the same dir

Compile with `g++ -o ines_test ines_test.cpp ines.cpp`
*/

int main() {
    struct nes_header header;
    struct nes_data nesdata;
    FILE *nesfile = fopen("donkey_kong.nes", "r");
    if(!nesfile) {
        printf("pb open\n");
        return 1;
    }

    try
    {
         header = read_nes_header(nesfile);
    }
    catch(const IncorrectFileFormat& e)
    {
        printf("Error : %s\n", e.what());
    }
    
    printf("========== NES HEADER ===========\n");
    printf("nb_PRG_ROM : %d\n", header.nb_PRG_ROM);
    printf("nb_CHR_ROM : %d\n", header.nb_CHR_ROM);
    printf("SCREEN_MIRRORING : %d\n", header.SCREEN_MIRRORING);
    printf("BB_PRG_RAM : %d\n", header.BB_PRG_RAM);
    printf("TRAINER : %d\n", header.TRAINER);
    printf("FOUR_SCREENS : %d\n", header.FOUR_SCREENS);
    printf("MAPPER_NB : %d\n", header.MAPPER_NB);
    printf("NES2 : %d\n", header.NES2);
    printf("CONSOLE_TYPE : %d\n", header.CONSOLE_TYPE);


    nesdata = read_nes_data(nesfile, header);

    printf("========== NES DATA ==========\n");
    printf("PRG_ROM_SIZE : $%x\n", nesdata.PRG_ROM_size);
    printf("PRG_ROM_data[0] : $%x\n", nesdata.PRG_ROM_data[0]);
    printf("PRG_ROM_data[0x30] : $%x\n", nesdata.PRG_ROM_data[0x30]);
    printf("CHR_ROM_SIZE : $%x\n", nesdata.CHR_ROM_size);
    printf("CHR_ROM_data[0] : $%x\n", nesdata.CHR_ROM_data[0]);
    printf("CHR_ROM_data[0x30] : $%x\n", nesdata.CHR_ROM_data[0x30]);

    return 0;
}