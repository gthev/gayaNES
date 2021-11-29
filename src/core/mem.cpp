#include "mem.hpp"
#include "cpu.hpp"
#include "ppu_info.hpp"
#include "nes_loaders/ines.hpp"
#include <vector>


// ========== NES MEMORY

static char game_genie_codes_raw[16] = {'A', 'P', 'Z', 'L', 'G', 'I', 'T', 'Y', 'E', 'O', 'X', 'U', 'K', 'S', 'V', 'N'};
static UINT8 game_genie_decode_tok(char c) {
    for(int i=0; i<16; i++) {
        if(game_genie_codes_raw[i] == c) return i;
    }
    throw AddressFault("Incorrect Game Genie character");
}

void NESMemory::set_game_genie(std::string &genie_code) {
    // upper case
    for (auto & c: genie_code) c = toupper(c);

    if(genie_code.size() == 6) {
        UINT16 decoded[6] = {0, 0, 0, 0, 0, 0};
        try
        {
            for(int i=0; i<6; i++) {
                decoded[i] = (UINT16)game_genie_decode_tok(genie_code[i]);
            }
        }
        catch(const AddressFault& e)
        {
            std::printf("Incorrect Game Genie character. Skipping it\n");
            return;
        }
        MEMADDR addr = 0x8000 +
                          ((decoded[3] & 7) << 12)
                        | ((decoded[5] & 7) << 8) | ((decoded[4] & 8) << 8)
                        | ((decoded[2] & 7) << 4) | ((decoded[1] & 8) << 4)
                        | ((decoded[4] & 7))      | ((decoded[3] & 8));

        UINT8 data = ((decoded[1] & 7) << 4) | ((decoded[0] & 8) << 4)
                       | ((decoded[0] & 7))   | (decoded[5] << 8);

        game_genie.addr = addr;
        game_genie.data = data;
        std::printf("Game Genie : addr : 0x%04X data : 0x%02X\n", addr, data);
        game_genie_active = 1;
    }
    else if(genie_code.size() == 8) {
        throw NotImplemented("8-bits Game Genie Code");
        game_genie_active = 1;
    }
}

void NESMemory::unset_game_genie() {
    game_genie_active = 0;
}

void NESMemory::check_ppu_sync() {
    if(cpu->get_return_on_ppu()) {
            
        if(!cpu->get_skip_next_op_ppu()) {
            // ok we got to sync cpu and ppu !
            throw PpuSync();
        } else {
            cpu->set_skip_next_op_ppu(0);
        }
    }
}

CPUMemoryManager *NESMemory::save_state() {
    NESMemory *cloned = new NESMemory(memROM);
    // we copy the reference to the device object ?
    cloned->devices = devices;
    for(int i=0; i<0x0800; i++) {
        // we copy RAM
        cloned->memRAM[i] = memRAM[i];
    }
    for(int i=0; i<0x17; i++) {
        cloned->APUJoypads[i] = APUJoypads[i];
    }
    return cloned;
}

UINT8 NESMemory::read(MEMADDR a) {
    // TODO : check if faster bitwise or with <

    if(game_genie_active && a == game_genie.addr) {
        return game_genie.data;
    }

    if(!(a & 0xF800)) {
        //if(a == 0x06FC) std::printf("Reading 0x06FC from 0x%04X : 0x%02X\n", cpu->get_pc(), memRAM[a]);
        return memRAM[a]; // main ram
    }
    else if(a < 0x1000)
        return memRAM[a & 0x07FF]; // first mirror
    else if(a < 0x1800)
        return memRAM[a & 0x07FF];
    else if(!(a & 0xE000))
        return memRAM[a & 0x07FF];
    
    // then, PPU regs
    else if(a < 0x2008)
    {
        
        check_ppu_sync();

        switch (a)
        {
        case 0x2000:
            return 0x00; // write only
        case 0x2001:
            return 0x00; // write only
        case 0x2002:
            return ppu_mem->read_PPUSTATUS();

        case 0x2003:
            return 0x00; // id
        case 0x2004:
            return ppu_mem->read_OAMDATA();

        case 0x2005:
            return 0x00;
        case 0x2006:
            return 0x00;
        case 0x2007:
            return ppu_mem->read_PPUDATA();
        
        default:
            return 0x00; //shouldn't happen
        }
    }

    else if(!(a & 0xC000)) {
        UINT8 nr_reg = (a - 0x2000) % 8;
        return read(0x2000+((MEMADDR)nr_reg));
    }

    else if(a < 0x4020) {
        // APU and joypads
        switch (a)
        {
        case 0x4014:
            // DMA, write only
            return 0x00;
        case 0x4016:
            return devices->read_4016();

        case 0x4017:
            return devices->read_4017(); // APU ?
        // TODO : rest !
        
        default:
            break;
        }
    }

    else {
        return memROM->read(a);
    }
}

// =========

void NESMemory::write(MEMADDR a, UINT8 val) {
    if(!(a & 0xF800)) {
        memRAM[a] = val; // main ram
    }
    else if(a < 0x1000) {
        memRAM[a & 0x07FF] = val; // first mirror
    }
    else if(a < 0x1800) {
        memRAM[a & 0x07FF] = val; // second mirror
    }
    else if(!(a & 0xE000)) {
        memRAM[a & 0x07FF] = val; // third mirror
    }
    
    // then, PPU regs
    else if(a < 0x2008)
    {

        check_ppu_sync();

        switch (a)
        {
        case 0x2000:
            ppu_mem->write_PPUCTRL(val);
            return;
        case 0x2001:
            ppu_mem->write_PPUMASK(val);
            return;
        case 0x2002:
            return; // read only

        case 0x2003:
            ppu_mem->write_OAMADDR(val);
            return;
        case 0x2004:
            ppu_mem->write_OAMDATA(val);
            return;
        case 0x2005:
            ppu_mem->write_PPUSCROLL(val);
            return;
        case 0x2006:
            ppu_mem->write_PPUADDR(val);
            return;
        case 0x2007:
            ppu_mem->write_PPUDATA(val);
            return;
        default:
            return; //shouldn't happen
        }
    }

    else if(!(a & 0xC000)) {
        UINT8 nr_reg = (a - 0x2000) % 8;
        write(0x2000+((MEMADDR)nr_reg), val); return;
    }

    else if(a < 0x4020) {
        // APU and joypads
        switch (a)
        {
        case 0x4014:
            // DMA
            ppu_mem->write_OAMDMA(val);
            cpu->wait_cycles(512);
            return;
        case 0x4016:
            // device
            devices->write_4016(val);
            return;

        case 0x4017:
            devices->write_4017(val); // TODO : probably change it with APU
            return;
        
        default:
            break;
        }
    }

    else {
        memROM->write(a, val);
    }
}

UINT8 NESMemory::read_stack(ZPADDR offset) {
    //std::printf("Reading stack @0x%02X\n", offset);
    return memRAM[STACK_PAGE_START+(MEMADDR)offset];
}
void NESMemory::write_stack(ZPADDR offset, UINT8 val) {
    //std::printf("Writing %02X stack @0x%02X\n", val, offset);
    memRAM[STACK_PAGE_START+(MEMADDR)offset] = val;
}

// ************* ROM MANAGER

ROMDefault::ROMDefault(struct nes_data *nesdata) {
    PRG_ROM_SIZE = nesdata->PRG_ROM_size;
    PRG_ROM_DATA = nesdata->PRG_ROM_data;

    PRG_ROM_RESOLUTION[0] = 0;

    if(nesdata->header.nb_PRG_ROM == 1) {
        PRG_ROM_RESOLUTION[1] = 0;
    } else if (nesdata->header.nb_PRG_ROM == 2) {
        PRG_ROM_RESOLUTION[1] = 0x4000;
    }

    // TODO : handle it
    // SRAM_SIZE = nesdata->header.BB_PRG_RAM;

    // by default, we copy what we can into pattern table
    if(nesdata->CHR_ROM_size / 8192 != 1) {
        std::printf("Mapper 0 used with %d CHR_ROM 8kb chunks.\n", nesdata->CHR_ROM_size / 8192);
        throw IncorrectFileFormat("Mapper 0 with != 1x8kb of CHR_ROM");
    }

    for(int nr_table = 0; nr_table<2; nr_table++) {
        for(int i=0; i<4096; i++) {
            pt[nr_table][i] = nesdata->CHR_ROM_data[i + (nr_table << 12)];
        }
    }
}

ROMMemManager *ROMDefault::save_state() {
    ROMDefault *cloned = new ROMDefault();
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

ROMDefault::~ROMDefault() {}

void ROMDefault::write(MEMADDR a, UINT8 val) {
    // default : no behaviour in case of write !
    return;
}

UINT8 ROMDefault::read(MEMADDR a) {
    
    if(a < 0x8000) return 0x00; // TODO : change it in case of SRAM

    // 2nd highest bit of a : number of 16kb chunk of rom
    // after putting down the highest bit, we select the second one (>>14), which gives 0 or 1, and we read the ROM using the rest of the address (a & 0x3FFF)

    a &= 0x7FFF;
    
    return PRG_ROM_DATA[PRG_ROM_RESOLUTION[a >> 14]+(a & 0x3FFF)];
}

UINT8 ROMDefault::read_pt(MEMADDR a) {
    int nr_table = !!(a & 0x1000);
    return pt[nr_table][a & 0x0FFF];
}

void ROMDefault::write_pt(MEMADDR a, UINT8 val) {
    // mapper 0, no !
    return;
}


void CPUMemoryManager::dump_stack(FILE *s) {
    for(UINT16 i=0; i<0x100; i++) {
        fprintf(s, "0x%02X : %02X\n", i, read_stack(i));
    }
}