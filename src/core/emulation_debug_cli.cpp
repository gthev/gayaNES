#include <string>
#include "emulation_manager.hpp"

#define CHECK_BOUNDS(val, min_, max_, val_name)                         \
if(val < min_ || val > max_) {                                          \
    printf("%s should be between %d and %d\n", val_name, min_, max_);   \
    continue;                                                           \
}

using namespace std;

vector<string> tokenise(const string &str){
    vector<string> tokens;
    int first = 0;
    while(first<str.size()){
        int second = str.find_first_of(' ', first);
        if(second==string::npos){
            second = str.size();
        }
        string token = str.substr(first, second-first);
        tokens.push_back(token);
        first = second + 1;
    }
    return tokens;
}

void print_oam_entry(struct OAMentry &sprite) {
    printf("Sprite offset : [%d, %d]\n", sprite.X_off, sprite.Y_off);
    printf("Sprite tile idx (beware, 8x16 niy) : %d = PT[%d, %d]\n", sprite.tile_idx, sprite.tile_idx % 16, sprite.tile_idx / 16);
    printf("Sprite palette : %d\n", sprite.attr & 0x03);
    printf("Sprite priority : %s\n", (sprite.attr & 0x20)?"Behind":"Front");
    if(sprite.attr & 0x40) printf("Flip horizontally\n");
    if(sprite.attr & 0x80) printf("Flip vertically\n");
}

void EmulationManager::enter_debug_cli() {
    bool continuer = true;
    string user_input = "";
    string command;
    int arg1, arg2, arg3;
    vector<string> tokens;

    printf("=-=-=-=-=-=-=-=-=-=-=-=-=\n");
    printf("   EMULATION MANAGER\n");
    printf("       DEBUG CLI\n");

    while(continuer) {
        cout << "\n>>> ";
        getline(cin, user_input);
        cout << endl;
        tokens = tokenise(user_input);

        if(!tokens.size()) continue;
        command = tokens[0];

        // quit
        if(!command.compare("q") || !command.compare("quit")) continuer = false;

        // fetch NT table idx in PT
        else if(!command.compare("ntidx")) {
            
            if(tokens.size() != 4) {
                printf("Bad usage of ntidx : ntidx NT_nr tile_x tile_y\n");
                continue;
            }

            arg1 = stoi(tokens[1]); // number of NT table
            CHECK_BOUNDS(arg1, 0, 3, "NT table number\0")

            arg2 = stoi(tokens[2]); // tile_nr_ x
            CHECK_BOUNDS(arg2, 0, 31, "tile_nr_x\0");

            arg3 = stoi(tokens[3]); // tile_nr_ y
            CHECK_BOUNDS(arg3, 0, 29, "tile_nr_y\0");

            unsigned int nt_tile_nr = ppu_mem->get_nt()[ppu_mem->get_real_nt(arg1)][arg3*32+arg2];
            printf("NT[%d][%d, %d] = %d = PT[%d][%d, %d]\n", arg1, arg2, arg3, nt_tile_nr, ppu_state->BACKGROUND_TABLE, nt_tile_nr % 16, nt_tile_nr / 16);

        }

        // fetch NT attribute of a tile
        else if(!command.compare("ntattr")) {

            if(tokens.size() != 4) {
                printf("Bad usage of ntattr : ntattr NT_nr tile_x tile_y\n");
                continue;
            }

            arg1 = stoi(tokens[1]); // number of NT table
            CHECK_BOUNDS(arg1, 0, 3, "NT table number\0")

            arg2 = stoi(tokens[2]); // tile_nr_ x
            CHECK_BOUNDS(arg2, 0, 31, "tile_nr_x\0");

            arg3 = stoi(tokens[3]); // tile_nr_ y
            CHECK_BOUNDS(arg3, 0, 29, "tile_nr_y\0");

            UINT16 att_block_nr = (arg3 >> 2)*8+(arg2 >> 2);
            UINT8 sub_block_idx = ((arg2 & 0x03) >> 1) | (arg3 & 0x02);

            UINT8 full_att = ppu_mem->get_nt()[ppu_mem->get_real_nt(arg1)][0x03C0+att_block_nr];
            UINT8 att = (full_att >> (sub_block_idx << 1)) & 0x03;

            printf("NT[%d][%d, %d] attribute = %d\n", arg1, arg2, arg3, att);

        }

        // Display information about Background status
        else if(!command.compare("bkstatus")) {

            printf("BACKGROUND STATUS\n");
            printf("Displayed : %d\n", ppu_state->SHOW_BACKGROUND);
            printf("Left displayed (/!\\ not yet implemented !): %d\n", ppu_state->BACKGROUND_CORNER);
            printf("PT table : %d\n", ppu_state->BACKGROUND_TABLE);
            printf("Temp vram address: 0x%04X\n", ppu_state->T_VRAM_ADDRESS);
            printf("VRAM address : 0x%04X\n", ppu_state->VRAM_ADDRESS);
            for(int i=0; i<4; i++) {
                printf("NT[%d]->PT[%d]\n", i, ppu_mem->get_real_nt(i));
            }

        }

        else if(!command.compare("spstatus")) {

            printf("SPRITE STATUS\n");
            printf("Displayed : %d\n", ppu_state->SHOW_SPRITE);
            printf("Left displayed (/!\\ not yet implemented !): %d\n", ppu_state->SPRITE_CORNER);
            printf("PT table : %d\n", ppu_state->SPRITE_TABLE);
            printf("Last sprite 0 hit : %d\n", ppu_state->SPRITE0HIT);
            printf("Sprite size : %s\n", (ppu_state->SPRITE_SIZE)? "8x16" : "8x8"); 

        }

        else if(!command.compare("sprite")) {
            if(tokens.size() != 2) {
                printf("Bad usage : sprite [all | sprite_nr]\n");
                continue;
            }
            if(!tokens[1].compare("all")) {
                for(int i=0; i<64; i++) {
                    printf("--- SPRITE %d\n", i);
                    print_oam_entry(*(ppu_mem->get_oam_entry(i)));
                }
            } else {
                arg1 = stoi(tokens[1]);
                CHECK_BOUNDS(arg1, 0, 63, "OAM entry");

                printf("--- SPRITE %d\n", arg1);
                print_oam_entry(*(ppu_mem->get_oam_entry(arg1)));
            }
        }

        else if(!command.compare("cpustatus")) {
            if(tokens.size() != 1) {
                printf("Bad usage : cpustatus\n");
                continue;
            }
            struct cpu6502::cpu6502regs r = cpu->get_cpu_regs();
            struct cpu6502::cpu6502flags f = cpu->get_cpu_flags();
            printf("CPU Status\n");
            printf("PC: 0x%04X A: 0x%02X X:0x%02X Y:0x%02X SP:0x%02X\n", r.PC, r.A, r.X, r.Y, r.S);
            printf("NV--DIZC\n%d%d--%d%d%d%d\n", !!(f.N), !!(f.V), !!(f.D), !!(f.I), !!(f.Z), !!(f.C));
        }

        else if(!command.compare("cpumem")) {
            if(tokens.size() != 2) {
                printf("Bad usage : cpumem addr(hex)\n");
                continue;
            }
            arg1 = stoi(tokens[1], 0, 16);
            CHECK_BOUNDS(arg1, 0, 0xFFFF, "addr");
            UINT8 val = cpu_mem->read(arg1);
            printf("cpu_mem[0x%04X]=%02X\n", arg1, val);
        }

        else if(!command.compare("ppumem")) {
            if(tokens.size() != 2) {
                printf("Bad usage : ppumem addr(hex)\n");
                continue;
            }
            arg1 = stoi(tokens[1], 0, 16);
            CHECK_BOUNDS(arg1, 0, 0xFFFF, "addr");
            UINT8 val = ppu_mem->read_ppu_mem(arg1);
            printf("ppu_mem[0x%04X]=%02X\n", arg1, val);
        }

        else if(!command.compare("cpustep")) {
            if(tokens.size() > 2) {
                 printf("Bad usage : cpustep [nr_steps]?\n");
                continue;
            }
            arg1 = (tokens.size() > 1)? stoi(tokens[1]) : 1;
            CHECK_BOUNDS(arg1, 0, 1000000, "nr_steps"); // arbitrary max
            for(int i=0; i<arg1; i++) {
                cpu->execute_cycles(1);
            }
        }

        else if(!command.compare("ppurender")) {
            ppu_render->render();
            ppu_render->draw_debug_tiles_grid(1);
        }

        // Default : unrecognized command
        else {
            printf("Unrecognized command\n");
        }
    }

    printf("Quitting EmulationManager Debug CLI\n");
}