#include <iostream>
#include <bitset>
#include <string>
#include <sstream>
#include <boost/algorithm/string.hpp>
#include <vector>
#include "cpu.hpp"

#define INSTR_START   0x00C0

/* CLI to debug CPU/CPUMEM 
-> s : step
-> r : read memory, 'r 1234' to read $1234

compile with
g++ -o cputest cpu_debug.cpp cpu.cpp ppu_mem.cpp mem.cpp -lSDL2 -g
*/

void print_cpu_state(cpu6502 const& cpu, FullRamMemory *mem) {
    cpu6502flags f = cpu.get_cpu_flags();
    cpu6502regs  r = cpu.get_cpu_regs();
    cpu6502interrupts intps = cpu.get_cpu_intps();

    UINT8 flags = flags_to_byte(f);
    UINT8 next_op = mem->read(r.PC);
    std::bitset<8> fb(flags);

    std::printf("PC : $%04X [pointing to $%02X]\n", r.PC, next_op);
    std::printf("A  : $%02X\n", r.A);
    std::printf("X  : $%02X\n", r.X);
    std::printf("Y  : $%02X\n", r.Y);
    std::printf("S  : $%02X\n", r.S);
    std::cout << "NV---IZC" << std::endl;
    if(intps.CPUhalt) std::cout << "*** CPU HALTED ***\n";
    if(intps.NMIPending) std::cout << "* NMI PENDING *\n";
    if(intps.IRQPending) std::cout << "* IRQ PENDING *\n";
    std::cout << fb << std::endl;
}

void print_mem_location(MEMADDR a, FullRamMemory *mem) {
    UINT8 val = mem->read(a);
    std::printf("$%04X : $%02X\n", a, val);
}

int main(int argc, char *argv[]) {
    std::string s("CPU 6502 DEBUGGER. Enter instructions (in hex form), it will loaded"
                  " in memory at 0x0C00, and an instruction will be executed at every ENTER pressed."
    );
    std::string input;

    bool cont = true;
    FullRamMemory *mem = new FullRamMemory();
    cpu6502 cpu = cpu6502(mem);
    std::cout << s << std::endl;

    do {
        // interpret it as instructions
        std::cout << ">>> ";
        std::getline(std::cin, input);

        int i = 0;
        MEMADDR next_instr = INSTR_START;

        try
        {
            while(i < input.length()) {

                if(input[i] == ' ') {i++; continue;}
                std::string byte_s = input.substr(i, (i+1 == input.length())?1:2);
                UINT8 val = std::stoul(byte_s, 0, 16);
                mem->write(next_instr++, val);
                i+=2;
                cont = false;
            }
        }
        catch(const std::exception& e)
        {
            std::cerr << "An error occured : " << e.what() << '\n';
            std::cout << "Please try again\n";
        }
        
    } while(cont);

    cpu.init_cpu(INSTR_START);
    
    cont = true;
    while(cont) {
        std::cout << "===================\n";
        print_cpu_state(cpu, mem);
        std::cout << ">>> ";
        std::getline(std::cin, input);

        std::vector<std::string> strs;
        boost::split(strs, input, boost::is_any_of(" "));

        std::string command = strs[0];

        // ------------- step

        if(input == "" || command == "s") {
            if(!cpu.get_cpu_intps().CPUhalt) {
                UINT8 nr_cycles = cpu.von_neumann_cycle();
                std::printf("%d cycles\n", 
                    (unsigned int)nr_cycles);
            }

        // -------------- read memory
        } else if(command == "r") {
            if(strs.size() != 2) {
                std::cerr << "Command read used badly : r 1234\n";
                continue;
            }
            MEMADDR val = std::stoul(strs[1], 0, 16);
            std::printf("$%04X : $%02X\n", val, mem->read(val));
        }

        // ------------- default case
        else {
            
            //throw NotImplemented(input.c_str()); // hmm a bit too extrem
            std::cerr << "Command unknown : " << command << std::endl;
            continue;
        }
    }
    return 0;
}