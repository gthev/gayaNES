#include <SDL2/SDL.h>
#include <iostream>
#include <fstream>
#include <getopt.h>
#include "emulation_manager.hpp"
#include "sdl_utils.hpp"

/*
compile with
g++ -o emul_test emul_test.cpp nes_loaders/ines.cpp emulation_manager.cpp emulation_debug_cli.cpp ppu_render/ppu_render.cpp ppu_render/draw_tile.cpp cpu.cpp mem.cpp ppu_mem.cpp input_devices/device.cpp input_devices/nesjoypad.cpp sdl_utils.cpp -lSDL2
*/

const int block_size = 4;
const int width_screen = block_size*256;
const int height_screen = block_size*240;

typedef struct {
    char *rom_path = NULL, *game_genie = NULL;
    bool cli_debug = false;
    bool help = false;
    bool should_stop = false;
} cli_args_result;

cli_args_result parse_args(int argc, char *argv[]) {
    cli_args_result res;
    int option;
    while((option = getopt(argc, argv, ":g:dh")) != -1) {
        switch (option)
        {
        case 'h':
            res.help = true;
            res.should_stop = true;
            break;

        case 'd':
            res.cli_debug = true;
            break;

        case 'g':
            res.game_genie = optarg;
            break;

        case ':':
            printf("Missing argument for %c\n", optopt);
            break;  
        
        case '?': // i know, could be removed
        default:
            std::printf("Unknown argument : %c\n", optopt);
            res.should_stop = true;
            break;
        }
        if(res.should_stop) return res;
    }

    if(optind < argc)
    {
        // non-option arg, should be the filename
        res.rom_path = argv[optind++];
        if(optind < argc) {
            // weird, several non option args ?
            std::printf("Only one non-option argument was expected\n");
            res.should_stop = true;
        }
    }
    return res;
}

void show_cli_help() {
    std::printf("GayaNES CLI HELP\n");
    std::printf("Gaspard Thévenon\n\n");
    std::printf("Usage : ./emul_core [OPTIONS] rom_path\n");
    std::printf("\nOptions:\n\t-g CODE : use a game-genie code\n");
    std::printf("\t-d : enable debug cli when exiting the game\n");
    std::printf("\t-h : shows this message\n\n");
}

void show_options(cli_args_result res) {
    std::printf("--- CLI arguments :\n");
    std::printf("ROM path : %s\n", res.rom_path);
    std::printf("Game Genie : %s\n", (res.game_genie)? res.game_genie : "[NO]");
    std::printf("Debug CLI : %d\n", res.cli_debug);
}

void check_keys(EmulationManager *em) {
    SDL_Event ev;
    SDL_Keycode k;
    while(SDL_PollEvent(&ev))
        switch (ev.type)
        {
        case SDL_KEYDOWN:
            k = ev.key.keysym.sym;
            switch (k)
            {
            case SDLK_s:
                em->save_state();
                break;

            case SDLK_r:
                em->restore_state();
                break;
            
            default:
                em->push_sdl_event(ev);
                break;
            }
            break;
        
        default:
            em->push_sdl_event(ev);
            break;
        }
}

int main(int argc, char *argv[]) {
    sdl_context sdl_ctx;
    SDL_Window *window;
    SDL_Renderer *renderer;
    
    cli_args_result args = parse_args(argc, argv);
    if(args.help) {
        show_cli_help();
    }
    if(args.should_stop) return 0; // abort

    show_options(args);

    if(init_window_renderer("Test gayaNES", &window, &renderer, width_screen, height_screen) < 0) {
        std::printf("Initialization of SDL window failed: %s\n", SDL_GetError());
        return 1;
    }

    sdl_ctx.block_size = block_size;
    sdl_ctx.renderer = renderer;
    sdl_ctx.format = SDL_AllocFormat(SDL_GetWindowPixelFormat(window));

    std::string game_genie_code("");

    EmulationManager *emul_manager = new EmulationManager(&sdl_ctx);
    FILE *fnes = fopen(args.rom_path, "r");
    if(!fnes) {
        std::printf("Error while opening nes file.\n");
        return 1;
    }
    emul_manager->open_nes(fnes);

    emul_manager->init_rom();

    emul_manager->init_devices();

    std::printf("File opening : OK\n");
    std::fflush(NULL);

    emul_manager->init_cpu(0xC000);

    if(args.game_genie) {
        game_genie_code = std::string(args.game_genie);
        emul_manager->set_game_genie(game_genie_code);
    }

    std::printf("CPU initialization : OK\n");
    std::fflush(NULL);

    emul_manager->init_ppu();

    std::printf("Emulation initialization : OK\n");
    std::fflush(NULL);

    emul_manager->reset_emulation_loop();
    
        // main emulation loop

        //std::printf("loop : %d\n", nr_loops);
        //std::fflush(NULL);
    do {
        try
        {
            check_keys(emul_manager);
            emul_manager->one_emulation_loop();
        }
        catch(const ExitedGame& e)
        {
            emul_manager->draw_visual_debug_information();
            if(args.cli_debug) emul_manager->enter_debug_cli();
            break;
        }
        catch(const CPUHalted& e)
        {
            std::printf("CPU Halted after %d cycles\n", emul_manager->get_cpu_cycles());
            break;
        }
    } while(true);

    
    SDL_pause();

    delete emul_manager;
    SDL_DestroyWindow(window);
    return 0;
}