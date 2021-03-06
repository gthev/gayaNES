#include <iostream>
#include <chrono>
#include <thread>
#include <future>
#include "exceptions.hpp"
#include "emulation_manager.hpp"

EmulationManager::EmulationManager(sdl_context *ctx) : sdl_ctx(ctx), cpu(nullptr), cpu_mem(nullptr), ppu_mem(nullptr), ppu_state(nullptr), 
                                                        ppu_render(nullptr), rom_mem(nullptr), devices(nullptr), loop_duration(0.026)
{
    current_save_state.cpu_mem = nullptr; current_save_state.ppu_mem = nullptr; current_save_state.ppu_state = nullptr; current_save_state.rom_mem = nullptr;
}

EmulationManager::~EmulationManager()
{
    // probably destroy things ?
    delete cpu;
    delete cpu_mem;
    delete ppu_state;
    delete ppu_mem;
    delete ppu_render;
}

int EmulationManager::open_nes(FILE *fnes) {
    if(!fnes) {
        std::printf("Problem during rom fopen\n");
        return -1;
    }

    try
    {
        nes_header = read_nes_header(fnes);
    }
    catch(const IncorrectFileFormat& e)
    {
        std::printf("FileFormat error on constructing game header: %s\n", e.what()); return -1;
    }
    catch(const FileReadingError& e)
    {
        std::printf("Reading error on constructing game header: %s\n", e.what()); return -1;
    }

    print_ines_header_info(nes_header);

    try {
        nes_data = read_nes_data(fnes, nes_header);
    }
    catch(const FileReadingError& e) {
        std::printf("Reading error on constructing game data: %s\n", e.what()); return -1;
    }
    catch(const IncorrectFileFormat& e) {
        std::printf("FileFormat error on constructing game data: %s\n", e.what()); return -1;
    }
    catch(const MemAllocFailed& e) {
        std::printf("Memory allocation error on constructing game data: %s\n", e.what()); return -1;
    }

    return 0;
}

int EmulationManager::init_devices() {
    if(devices) delete devices.get();
    devices = std::make_shared<DevicesManager>();
    devices->set_automatic_poll_empty(true);
    keys_manager = std::make_shared<SDL_Events_Manager>(devices);
    devices->keyboards_manager = keys_manager;
}

int EmulationManager::init_rom() {
    if(rom_mem) delete rom_mem;
    rom_mem = mapper_resolve(&nes_header, &nes_data);
    if(!rom_mem) {
        throw MemAllocFailed("Rom memory initialization failed\n");
    }
    return 0;
}

int EmulationManager::init_cpu(MEMADDR init_pc) {
    if(cpu_mem) delete cpu_mem;
    cpu_mem = new NESMemory(rom_mem);
    if(cpu) delete cpu;
    cpu     = new cpu6502(cpu_mem);
    cpu->init_cpu(init_pc);
    cpu_mem->cpu = cpu;
    cpu_mem->devices = devices;

    return 0;
}

int EmulationManager::init_ppu() {
    if(ppu_state) delete ppu_state;
    ppu_state = new PPU_state;
    if(ppu_mem) delete ppu_mem;
    ppu_mem = new PPU_mem(ppu_state);
    ppu_mem->rom = rom_mem;
    if(ppu_render) delete ppu_render;
    ppu_render = new PPU_Render(sdl_ctx, ppu_mem, ppu_state, cpu);

    cpu_mem->ppu_mem = ppu_mem;

    ppu_state->NMI_VBLANK = 0;
    ppu_state->SPRITE_SIZE = 0;
    ppu_state->SPRITE_TABLE = 0; // change it?
    ppu_state->BACKGROUND_TABLE = 0;
    ppu_state->SHOW_SPRITE = 0; // also change those two ?
    ppu_state->SHOW_BACKGROUND = 0;
    ppu_state->GREYSCALE = 0;
    ppu_state->SPRITE_CORNER = 0;
    ppu_state->BACKGROUND_CORNER = 0;
    ppu_state->IN_VBLANK = 0;

    ppu_state->SPRITE0HIT = 0;

    // now ppu mem

    ppu_mem->set_screen_miroring((screen_miroring)(nes_header.SCREEN_MIRRORING));
    // TODO : if(nes_header->FOUR_SCREENS) ppu_mem->set_screen_miroring(MIRORING_FOUR);
    ppu_mem->cpu = cpu;

    ppu_render->set_debug_mode(PPU_DEBUG_MODE::NONE);


    return 0;
}

BOOL EmulationManager::execute_cpu_cycles(UINT32 nr_cycles) {
    cpu6502::execute_cycles_res res = cpu->execute_cycles(nr_cycles);
    cpu->wait_cycles(res.cycles);
    return res.ppu_dirty;
}

int EmulationManager::reset_emulation_loop() {
    cpu->enter_reset();
    cpu->reset_cycles();
    execute_cpu_cycles(30000);
    return 0;
}

/* This function was totally stolen from Blat Blatnik on blat-blatnik.github.io */
void preciseSleep(double seconds) {

    static double estimate = 5e-3;
    static double mean = 5e-3;
    static double m2 = 0;
    static int64_t count = 1;

    while (seconds > estimate) {
        auto start = std::chrono::high_resolution_clock::now();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        auto end = std::chrono::high_resolution_clock::now();

        double observed = (end - start).count() / 1e9;
        seconds -= observed;

        ++count;
        double delta = observed - mean;
        mean += delta / count;
        m2   += delta * (observed - mean);
        double stddev = sqrt(m2 / (count - 1));
        estimate = mean + stddev;
    }

    // spin lock
    auto start = std::chrono::high_resolution_clock::now();
    while ((std::chrono::high_resolution_clock::now() - start).count() / 1e9 < seconds);
}

/*
===========================
NES Execution Functions
===========================
*/

void EmulationManager::BeginFrame() {
    cpu->reset_cycles();
    cpu->set_return_on_ppu(1);
    ppu_render->ppu_begin_frame();
    frame_start_time = std::chrono::high_resolution_clock::now();
}

void EmulationManager::BeginVBlank() {
    cpu->set_return_on_ppu(0);
}

void EmulationManager::EndFrame() {
    // real time synchronization
    double sec_elapsed = (std::chrono::high_resolution_clock::now() - frame_start_time).count() / 1e9;
    double to_wait = loop_duration - sec_elapsed;
    if(to_wait > 0.002) preciseSleep(to_wait);
}

int EmulationManager::one_emulation_loop() {
    BOOL should_sync_ppu;

    keys_manager->sdl_handle_poll();

    BeginFrame();

    // rendering part !
    while(cpu->get_cycles() < 27508) {
        should_sync_ppu = execute_cpu_cycles(27508); // try to execute all cpu cycles (will probably end before)
        if(should_sync_ppu) {
            ppu_render->ppu_execute_up_to(cpu->get_cycles() * 3);
        }
    }

    ppu_render->ppu_execute_up_to(82524); // 3 * 27508

    BeginVBlank();

    // only execute cpu, no pressure for cpu/ppu sync
    execute_cpu_cycles(2272);

    EndFrame();
}

/* ============ SAVE STATE ============== */

void EmulationManager::save_state() {
    std::printf("Saving state...");
    current_save_state.cpu = cpu->get_state();

    std::printf(" pc->0x%02X\n", cpu->read_mem(current_save_state.cpu.regs.PC));

    current_save_state.ppu_render = ppu_render->ppu_render_save_state();
    if(current_save_state.cpu_mem) delete current_save_state.cpu_mem;
    current_save_state.cpu_mem = cpu_mem->save_state();

    if(current_save_state.rom_mem) delete current_save_state.rom_mem;
    current_save_state.rom_mem = rom_mem->save_state();

    if(current_save_state.ppu_state) delete current_save_state.ppu_state;
    current_save_state.ppu_state = ppu_state_save(*ppu_state);

    if(current_save_state.ppu_mem) delete current_save_state.ppu_mem;
    current_save_state.ppu_mem = new PPU_mem(*ppu_mem);
    std::printf("OK\n");
}

void EmulationManager::restore_state() {
    std::printf("Restoring state...");
    delete rom_mem;

    // we copy rom mem from the save state
    rom_mem = current_save_state.rom_mem->save_state();

    delete cpu_mem;
    cpu_mem = current_save_state.cpu_mem->save_state();
    cpu_mem->memROM = rom_mem;
    cpu->restore_state(current_save_state.cpu);

    cpu->set_cpu_mem(cpu_mem);
    cpu_mem->cpu = cpu; // I think it's useless

    std::printf(" pc->0x%02X\n", cpu->read_mem(current_save_state.cpu.regs.PC));

    delete ppu_state;
    ppu_state = ppu_state_save(*current_save_state.ppu_state);

    delete ppu_mem;
    ppu_mem = new PPU_mem(*(current_save_state.ppu_mem));

    ppu_mem->ppu_state = ppu_state;
    ppu_mem->rom = rom_mem;
    cpu_mem->ppu_mem = ppu_mem;
    
    delete ppu_render;
    ppu_render = new PPU_Render(sdl_ctx, ppu_mem, ppu_state, cpu);
    ppu_render->ppu_render_restore_state(current_save_state.ppu_render);
    ppu_mem->cpu = cpu;
    std::printf("OK\n");
}


/* DEBUG STUFF ======================================*/



int EmulationManager::draw_visual_debug_information() {
    std::printf("*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\n");
    std::printf("DEBUG INFORMATION EMULATION MANAGER\n");
    std::printf("*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\n");
    ppu_render->set_debug_mode(PPU_DEBUG_MODE::SPRITE);
    one_emulation_loop();
    ppu_render->draw_debug_tiles_grid(1);
    SDL_Window *tiles_window = ppu_render->open_pattern_table_window();
    SDL_Window *palettes_window = open_palette_window(ppu_mem->BACKGROUND_palette, ppu_mem->SPRITE_palette, 50);
    ppu_mem->print_debug();
    std::printf("*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\n");
}

int EmulationManager::dump_cpu_history(FILE *s) {
    fprintf(s, "-------------------\n------ STACK DUMP -------\n");
    cpu_mem->dump_stack(s);
}




// *******

/* void EmulationManager::debug_donkey_kong() {
    // first, palettes
    ppu_mem->BACKGROUND_palette[0][0] = 0x0F; ppu_mem->BACKGROUND_palette[0][1] = 0x2C; ppu_mem->BACKGROUND_palette[0][2] = 0x15; ppu_mem->BACKGROUND_palette[0][3] = 0x12;
    ppu_mem->BACKGROUND_palette[1][0] = 0x0F; ppu_mem->BACKGROUND_palette[1][1] = 0x27; ppu_mem->BACKGROUND_palette[1][2] = 0x02; ppu_mem->BACKGROUND_palette[1][3] = 0x17;
    ppu_mem->BACKGROUND_palette[2][0] = 0x0F; ppu_mem->BACKGROUND_palette[2][1] = 0x30; ppu_mem->BACKGROUND_palette[2][2] = 0x36; ppu_mem->BACKGROUND_palette[2][3] = 0x06;
    ppu_mem->BACKGROUND_palette[3][0] = 0x0F; ppu_mem->BACKGROUND_palette[3][1] = 0x30; ppu_mem->BACKGROUND_palette[3][2] = 0x2C; ppu_mem->BACKGROUND_palette[3][3] = 0x24; 


    ppu_mem->SPRITE_palette[0][0] = 0x0F; ppu_mem->SPRITE_palette[0][1] = 0x02; ppu_mem->SPRITE_palette[0][2] = 0x36; ppu_mem->SPRITE_palette[0][3] = 0x16;
    ppu_mem->SPRITE_palette[1][0] = 0x0F; ppu_mem->SPRITE_palette[1][1] = 0x30; ppu_mem->SPRITE_palette[1][2] = 0x27; ppu_mem->SPRITE_palette[1][3] = 0x24;
    ppu_mem->SPRITE_palette[2][0] = 0x0F; ppu_mem->SPRITE_palette[2][1] = 0x16; ppu_mem->SPRITE_palette[2][2] = 0x30; ppu_mem->SPRITE_palette[2][3] = 0x37;
    ppu_mem->SPRITE_palette[3][0] = 0x0F; ppu_mem->SPRITE_palette[3][1] = 0x06; ppu_mem->SPRITE_palette[3][2] = 0x27; ppu_mem->SPRITE_palette[3][3] = 0x02;

    // now the nametable
    PPU_NT title_screen_dk_nt =
    // first, tiles
   {0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
    0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
    0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
    0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
    0x24, 0x24, 0x24, 0x62, 0x62, 0x62, 0x24, 0x24, 0x62, 0x62, 0x62, 0x24, 0x62, 0x24, 0x24, 0x62, 0x24, 0x62, 0x24, 0x24, 0x62, 0x24, 0x62, 0x62, 0x62, 0x24, 0x62, 0x24, 0x62, 0x24, 0x24, 0x24,
    0x24, 0x24, 0x24, 0x62, 0x62, 0x24, 0x62, 0x24, 0x62, 0x24, 0x62, 0x24, 0x62, 0x62, 0x24, 0x62, 0x24, 0x62, 0x62, 0x62, 0x24, 0x24, 0x62, 0x24, 0x24, 0x24, 0x62, 0x24, 0x62, 0x24, 0x24, 0x24,
    0x24, 0x24, 0x24, 0x62, 0x62, 0x24, 0x62, 0x24, 0x62, 0x24, 0x62, 0x24, 0x62, 0x62, 0x62, 0x62, 0x24, 0x62, 0x62, 0x24, 0x24, 0x24, 0x62, 0x62, 0x62, 0x24, 0x62, 0x62, 0x62, 0x24, 0x24, 0x24,
    0x24, 0x24, 0x24, 0x62, 0x62, 0x24, 0x62, 0x24, 0x62, 0x24, 0x62, 0x24, 0x62, 0x24, 0x62, 0x62, 0x24, 0x62, 0x24, 0x62, 0x24, 0x24, 0x62, 0x24, 0x24, 0x24, 0x24, 0x62, 0x24, 0x24, 0x24, 0x24,
    0x24, 0x24, 0x24, 0x62, 0x62, 0x62, 0x24, 0x24, 0x62, 0x62, 0x62, 0x24, 0x62, 0x24, 0x24, 0x62, 0x24, 0x62, 0x24, 0x24, 0x62, 0x24, 0x62, 0x62, 0x62, 0x24, 0x24, 0x62, 0x24, 0x24, 0x24, 0x24,
    0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
    0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x62, 0x24, 0x24, 0x62, 0x24, 0x62, 0x62, 0x62, 0x24, 0x62, 0x24, 0x24, 0x62, 0x24, 0x62, 0x62, 0x62, 0x62, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
    0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x62, 0x62, 0x62, 0x24, 0x24, 0x62, 0x24, 0x62, 0x24, 0x62, 0x62, 0x24, 0x62, 0x24, 0x62, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
    0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x62, 0x62, 0x24, 0x24, 0x24, 0x62, 0x24, 0x62, 0x24, 0x62, 0x62, 0x62, 0x62, 0x24, 0x62, 0x24, 0x62, 0x62, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
    0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x62, 0x24, 0x62, 0x24, 0x24, 0x62, 0x24, 0x62, 0x24, 0x62, 0x24, 0x62, 0x62, 0x24, 0x62, 0x24, 0x24, 0x62, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
    0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x62, 0x24, 0x24, 0x62, 0x24, 0x62, 0x62, 0x62, 0x24, 0x62, 0x24, 0x24, 0x62, 0x24, 0x62, 0x62, 0x62, 0x62, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
    0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
    0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x01, 0x24, 0x19, 0x15, 0x0A, 0x22, 0x0E, 0x1B, 0x24, 0x10, 0x0A, 0x16, 0x0E, 0x24, 0x0A, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
    0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
    0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x01, 0x24, 0x19, 0x15, 0x0A, 0x22, 0x0E, 0x1B, 0x24, 0x10, 0x0A, 0x16, 0x0E, 0x24, 0x0B, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
    0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
    0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x02, 0x24, 0x19, 0x15, 0x0A, 0x22, 0x0E, 0x1B, 0x24, 0x10, 0x0A, 0x16, 0x0E, 0x24, 0x0A, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
    0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
    0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x02, 0x24, 0x19, 0x15, 0x0A, 0x22, 0x0E, 0x1B, 0x24, 0x10, 0x0A, 0x16, 0x0E, 0x24, 0x0B, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
    0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
    0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x01, 0x09, 0x08, 0x02, 0x24, 0x17, 0x12, 0x17, 0x1D, 0x0E, 0x17, 0x0D, 0x18, 0x24, 0x0C, 0x18, 0x65, 0x15, 0x10, 0x0D, 0x64, 0x24, 0x24, 0x24, 0x24, 0x24,
    0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
    0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x16, 0x0A, 0x0D, 0x0E, 0x24, 0x12, 0x17, 0x24, 0x13, 0x0A, 0x19, 0x0A, 0x17, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
    0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
    0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
    0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
    // now the attributes
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};

    ppu_state->BACKGROUND_TABLE = 1;
    ppu_state->SHOW_SPRITE = 1;
    ppu_state->SHOW_BACKGROUND = 1;
    ppu_state->SPRITE_TABLE = 0;

    for(int i=0; i<1024; i++) {
        ppu_mem->write_nt(0x2000 + i, title_screen_dk_nt[i]);
    }

    struct OAMentry *oame;

    for(int n=0; n<64; n++) {
        oame = ppu_mem->get_oam_entry(n);
        oame->Y_off = 0xFF;
    }

    // the sprite
    oame = ppu_mem->get_oam_entry(0);
    oame->X_off = 0x38;
    oame->Y_off = 0x7F;
    oame->tile_idx = 0x02;
    oame->attr = 0x01;
} */