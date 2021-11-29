#ifndef MEM_GAYANES_HPP
#define MEM_GAYANES_HPP
#include <memory>
#include "types.hpp"
#include "input_devices/device.hpp"

class PPU_mem;
class cpu6502;
struct nes_header;

#define ADDR_SPACE_SIZE         65536
#define STACK_PAGE_START        0x0100
// yay 16-bit addresses

// ============== ROM HANDLING

// 2 * 8 bytes per tile
typedef UINT8 PATTERN_TABLE[4096];

class ROMMemManager
{
public:
    ROMMemManager(){};
    virtual ~ROMMemManager(){};
    virtual UINT8               read(MEMADDR a) = 0;
    virtual void                write(MEMADDR a, UINT8 val) = 0;

    virtual void                write_pt(MEMADDR in_addr, UINT8 val) = 0;
    virtual UINT8               read_pt(MEMADDR in_addr) = 0;

    virtual ROMMemManager       *save_state() = 0;
};


class ROMDefault : public ROMMemManager
{
protected:
    unsigned int  PRG_ROM_SIZE;
    UINT8         *PRG_ROM_DATA;

    UINT32       PRG_ROM_RESOLUTION[2]; // array : number of 16kb chunk -> addr in PRG_ROM_DATA

    PATTERN_TABLE pt[2];

public:
    ROMDefault(){};
    ROMDefault(struct nes_data *nes_data);
    virtual ~ROMDefault();
    virtual UINT8               read(MEMADDR a);
    virtual void                write(MEMADDR a, UINT8 val);
    virtual void                write_pt(MEMADDR in_addr, UINT8 val);
    virtual UINT8               read_pt(MEMADDR in_addr);

    virtual ROMMemManager       *save_state();
};

// Abstract class of memory manager for the CPU
class CPUMemoryManager
{
protected:

    struct GameGenie {
        MEMADDR addr;
        UINT8   data;
    };
    BOOL                        game_genie_active;
    struct GameGenie            game_genie; 

public:
    CPUMemoryManager(){};
    virtual ~CPUMemoryManager(){};
    cpu6502             *cpu;
    std::shared_ptr<DevicesManager>
                        devices;
    PPU_mem             *ppu_mem;
    ROMMemManager       *memROM; 
    virtual UINT8       read_stack(ZPADDR offset) = 0;
    virtual void        write_stack(ZPADDR offset, UINT8 val) = 0;
    virtual void        set_ppu_mem(PPU_mem *ppu_m){ppu_mem = ppu_m;};
    void                dump_stack(FILE *s);
    virtual void        set_game_genie(std::string &genie_code) = 0;
    virtual void        unset_game_genie() = 0;
    virtual void        write(MEMADDR a, UINT8 val) = 0;
    virtual UINT8       read(MEMADDR a) = 0;

    virtual CPUMemoryManager
                        *save_state() = 0;
}; 

// =========== REAL NES

//



class NESMemory : public CPUMemoryManager
{
protected:

    UINT8                       memRAM[0x0800];
    UINT8                       APUJoypads[0x17];    

    void                        check_ppu_sync(); 

public:
    NESMemory(ROMMemManager *memrom) {
        memROM = memrom;
        game_genie_active = 0;
        for(int i=0; i<0x0800; i++) memRAM[i] = 0;
    };
    virtual ~NESMemory(){};
    virtual UINT8               read(MEMADDR a);
    virtual void                write(MEMADDR a, UINT8 val);
    virtual UINT8               read_stack(ZPADDR offset);
    virtual void                write_stack(ZPADDR offset, UINT8 val);

    virtual void        set_game_genie(std::string &genie_code);
    virtual void        unset_game_genie();

    virtual CPUMemoryManager
                        *save_state();

};
#endif