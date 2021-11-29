#include <stdlib.h>
#include <iostream>
#include <string.h>
#include "ines.hpp"
#include "../exceptions.hpp"

#define EXCEPTION_READ(ptr, size, nb, s) \
    if(fread(ptr, size, nb, s) < nb)     \
        throw FileReadingError("Error while reading the header of the nes file.")

#define EXCEPTION_SEEK(s, off, whence) \
    if(fseek(s, off, whence))          \
        throw FileReadingError("Error while positioning the reading cursor. Incorrect format ?")



struct nes_header read_nes_header(FILE *nesfile) {
    struct nes_header header;
    UINT8 magic_word[4];
    UINT8 byte;
    // size_t read_size = 0;

    rewind(nesfile);

    EXCEPTION_READ(magic_word, 1, 4, nesfile);

    
    // now we check the magic word : 4e 45 53 1a, "NES"
    if(magic_word[0] != 0x4E ||
       magic_word[1] != 0x45 ||
       magic_word[2] != 0x53 ||
       magic_word[3] != 0x1A) throw IncorrectFileFormat("Magic word of the nes file is incorrect.");

    EXCEPTION_READ(&byte, 1, 1, nesfile);
    
    if(!byte) throw IncorrectFileFormat("nb_PRG_ROM null.");
    header.nb_PRG_ROM = byte;

    EXCEPTION_READ(&byte, 1, 1, nesfile);

    header.nb_CHR_ROM = byte;

    // flag 6
    EXCEPTION_READ(&byte, 1, 1, nesfile);
    header.SCREEN_MIRRORING = !!(byte & 0x01);
    header.BB_PRG_RAM = !!(byte & 0x02);
    header.TRAINER = !!(byte & 0x04);
    header.FOUR_SCREENS = !!(byte & 0x08);

    header.MAPPER_NB = (byte & 0xF0) >> 4;

    // flag 7
    EXCEPTION_READ(&byte, 1, 1, nesfile);
    header.CONSOLE_TYPE = byte & 0x03;
    header.NES2 = (byte & 0x0C) == 0x08;

    header.MAPPER_NB |= byte & 0xF0;

    // TODO : the rest, corresponding to NES 2.0 format

    return header;
}

struct nes_data read_nes_data(FILE *nesfile, struct nes_header header) {
    struct nes_data nesdata;
    size_t size_read;
    
    nesdata.header = header;
    EXCEPTION_SEEK(nesfile, 16, SEEK_SET);

    if(header.TRAINER) {
        nesdata.TRAINER = (UINT8*) calloc(512, 1);
        if(!nesdata.TRAINER) throw MemAllocFailed("mem alloc for TRAINER failed");
        EXCEPTION_READ(nesdata.TRAINER, 1, 512, nesfile);
    }

    // PRG ROM

    // we double check:
    if(!header.nb_PRG_ROM) 
        throw IncorrectFileFormat("nb_PRG_ROM larger null.");

    size_read = 16384 * header.nb_PRG_ROM;
    nesdata.PRG_ROM_size = size_read;
    nesdata.PRG_ROM_data = (UINT8*) calloc(size_read, 1);
    if(!nesdata.PRG_ROM_data) throw MemAllocFailed("mem alloc failed for PRG-ROM");

    EXCEPTION_READ(nesdata.PRG_ROM_data, 1, size_read, nesfile);

    // CHR ROM

    size_read = 8192 * header.nb_CHR_ROM;
    nesdata.CHR_ROM_size = size_read;
    nesdata.CHR_ROM_data = (UINT8*) calloc(size_read, 1);
    if(!nesdata.CHR_ROM_data) throw MemAllocFailed("mem alloc failed for CHR-ROM");

    EXCEPTION_READ(nesdata.CHR_ROM_data, 1, size_read, nesfile);

    if(header.CONSOLE_TYPE == 2) {
        // PlayChoice
        nesdata.INST_ROM = (UINT8*) calloc(1, 8192);
        if(!nesdata.INST_ROM) throw MemAllocFailed("mem alloc failed for INST-ROM");
        EXCEPTION_READ(nesdata.INST_ROM, 1, 8192, nesfile);

        nesdata.PROM = (UINT8*) calloc(1, 32);
        if(!nesdata.PROM) throw MemAllocFailed("mem alloc failed for PROM");
        EXCEPTION_READ(nesdata.PROM, 1, 32, nesfile);
    }

    // finally, sometimes there is a title
    memset(nesdata.TITLE, '\0', 129);
    fread(nesdata.TITLE, 1, 128, nesfile);
    // we don't check for error as this is optional

    return nesdata;
}

void print_ines_header_info(struct nes_header &header) {
    std::printf("========== NES HEADER ===========\n");
    std::printf("nb_PRG_ROM : %d x 16kb\n", header.nb_PRG_ROM);
    std::printf("nb_CHR_ROM : %d x 8kb\n", header.nb_CHR_ROM);
    std::printf("SCREEN_MIRRORING : %d\n", header.SCREEN_MIRRORING);
    std::printf("BB_PRG_RAM : %d\n", header.BB_PRG_RAM);
    std::printf("TRAINER : %d\n", header.TRAINER);
    std::printf("FOUR_SCREENS : %d\n", header.FOUR_SCREENS);
    std::printf("MAPPER_NB : %d\n", header.MAPPER_NB);
    std::printf("NES2 : %d\n", header.NES2);
    std::printf("CONSOLE_TYPE : %d\n", header.CONSOLE_TYPE);
    std::printf("=================================\n");
}