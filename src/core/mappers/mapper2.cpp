#include "mapper_resolve.hpp"

ROMMapper2::ROMMapper2(struct nes_data *nesdata) {
    PRG_ROM_SIZE = nesdata->PRG_ROM_size;
    PRG_ROM_DATA = nesdata->PRG_ROM_data;

    PRG_ROM_RESOLUTION[0] = 0;
    // second is fixed to the last bank
    PRG_ROM_RESOLUTION[1] = PRG_ROM_SIZE - 0x4000;

    if(nesdata->CHR_ROM_size / 8192 > 1) {
        std::printf("Mapper 2 used with %d CHR_ROM 8kb chunks.\n", nesdata->CHR_ROM_size / 8192);
        throw IncorrectFileFormat("Mapper 2 with > 1x8kb of CHR_ROM");
    }
    if(nesdata->CHR_ROM_size / 8192 == 1) {
        // we copy everything
        for(int nr_table = 0; nr_table<2; nr_table++) {
            for(int i=0; i<4096; i++) {
                pt[nr_table][i] = nesdata->CHR_ROM_data[i + (nr_table << 12)];
            }
        }
    } else {
        // nothing ?
        for(int nr_table = 0; nr_table<2; nr_table++) {
            for(int i=0; i<4096; i++) {
                pt[nr_table][i] = 0;
            }
        }
    }
}

ROMMemManager *ROMMapper2::save_state() {
    ROMMapper2 *cloned = new ROMMapper2();
    cloned->PRG_ROM_SIZE = PRG_ROM_SIZE;
    cloned->PRG_ROM_DATA = PRG_ROM_DATA;
    cloned->PRG_ROM_RESOLUTION[0] = PRG_ROM_RESOLUTION[0];
    cloned->PRG_ROM_RESOLUTION[1] = PRG_ROM_RESOLUTION[1];
    // now the pt
    for(int ptable=0; ptable<2; ptable++) {
        for(int i=0; i<4096; i++) {
            cloned->pt[ptable][i] = pt[ptable][i];
        }
    }
    return cloned;
}

void ROMMapper2::write(MEMADDR a, UINT8 val) {
    // I think it doesn't matter where we write ?

    UINT8 new_latch = val & 0x07; // we take the last 3 bits
    if(new_latch >= PRG_ROM_SIZE / 0x4000) {
        std::printf("Trying to bankswitch past the end of rom ?\n");
        new_latch = (PRG_ROM_SIZE / 0X4000) - 1;
    }

    PRG_ROM_RESOLUTION[0] = new_latch * 0x4000;
}

void ROMMapper2::write_pt(MEMADDR a, UINT8 val) {
    // mapper 2, well ok
    int nr_table = !!(a & 0x1000);
    pt[nr_table][a & 0x0FFF] = val;
}