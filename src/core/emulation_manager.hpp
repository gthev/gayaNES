#ifndef GAYA_EMULATION_MANAGER_HPP
#define GAYA_EMULATION_MANAGER_HPP

#include <chrono>
#include <memory>
#include "types.hpp"
#include "cpu.hpp"
#include "ppu_info.hpp"
#include "ppu_render/ppu_render.hpp"
#include "mappers/mapper_resolve.hpp"


class EmulationManager
{
private:

    struct save_state {
        // TODO : input managers ?
        cpu6502::cpu6502savestate cpu;
        ROMMemManager             *rom_mem;
        CPUMemoryManager          *cpu_mem;
        PPU_state                 *ppu_state;
        PPU_mem                   *ppu_mem;
        PPU_Render::save_state    ppu_render;
    };

    save_state                  current_save_state;

    struct nes_header           nes_header;
    struct nes_data             nes_data;

    cpu6502                     *cpu;
    ROMMemManager               *rom_mem;
    CPUMemoryManager            *cpu_mem;
    sdl_context                 *sdl_ctx;
    PPU_mem                     *ppu_mem;
    PPU_state                   *ppu_state;
    PPU_Render                  *ppu_render;

    std::shared_ptr<DevicesManager>
                                devices;
    std::shared_ptr<SDL_Events_Manager>
                                keys_manager;

    FILE                        *debug_output;

    double                      loop_duration;

    std::chrono::_V2::
        system_clock::
        time_point              frame_start_time;

public:

    EmulationManager(sdl_context *ctx);
    ~EmulationManager();

    std::shared_ptr<DevicesManager>  get_devices_manager(){return devices;}

    int open_nes(FILE *fnes);
    int init_devices(); // TODO : ways to customize it
    int init_rom();
    int init_cpu(MEMADDR init_pc = 0);
    // should be called once init_cpu() is done
    int init_ppu();
    /*
    Returns whether ppu should be put up to date
    */
    BOOL execute_cpu_cycles(UINT32 nr_cycles);
    int reset_emulation_loop();
    int draw_visual_debug_information();
    void set_ppu_render_debug_mode(PPU_DEBUG_MODE m) {ppu_render->set_debug_mode(m);};
    int dump_cpu_history(FILE *s);

    void set_debug_output(FILE *foutput){debug_output = foutput;};

    void set_game_genie(std::string &genie_code) {cpu_mem->set_game_genie(genie_code);};
    void unset_game_genie() {cpu_mem->unset_game_genie();};
    
    UINT32 get_cpu_cycles(){return cpu->get_cycles();};


    /* Functions most users will use are below */

    void BeginFrame();
    void BeginVBlank();
    void EndFrame();

    int one_emulation_loop();
    void push_sdl_event(SDL_Event ev) {keys_manager->push_event(ev);};

    void save_state();
    void restore_state();

    void enter_debug_cli();


    // void debug_donkey_kong();
};

#endif