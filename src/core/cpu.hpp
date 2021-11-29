/*
This file implements an interpreter to emulate 6502 CPUs.
author : gthev
*/

#ifndef CPU_GAYANES_HPP
#define CPU_GAYANES_HPP
#include <string>
#include <queue>
#include <stack>
#include "exceptions.hpp"
#include "types.hpp"
#include "mem.hpp"

/* ==================================== */
/*       DEFINITION OF THE 6502         */

class cpu6502
{
public:

    struct cpu6502regs
    {
        /* we define the different regs of the 6502 */
        UINT16      PC;
        UINT8       A;
        UINT8       X;
        UINT8       Y;
        UINT8       S;

        /* flags will be defined in a different structure for efficiency */
    };

    struct cpu6502flags
    {
        /* Different flags of the CPU state.
        Normally, those information are present in the P register, but for efficiency (as we don't program in assembly),
        we define them with a byte for each (yay what a waste) 
        A flag is set whenever it's value is != 0
        */

        BOOL        C;
        BOOL        D;
        BOOL        Z;
        BOOL        I;
        BOOL        V;
        BOOL        N;
    };

    struct cpu6502logentry {
        struct cpu6502regs r;
        struct cpu6502flags f;
    };

    struct cpu6502savestate {
        // Only the internal state, the memory will be handled in the adapted class
        cpu6502regs regs;
        cpu6502flags flags;
        UINT32 cycles;
        BOOL return_on_ppu, skip_next_op;
    };

    typedef struct {
        UINT32 cycles;
        BOOL   ppu_dirty;
    } execute_cycles_res;

    cpu6502(){ throw MemNotFound("Trying to create cpu6502 without memory"); };
    cpu6502(CPUMemoryManager *mem_handl);
    int                         init_cpu(MEMADDR pc);
    int                         reset_cpu();
    UINT8                       von_neumann_cycle();
    UINT8                       execute_op(UINT8 op);
    execute_cycles_res          execute_cycles(UINT32 nr_cycles);
    execute_cycles_res          execute_cycles_debug(UINT32 nr_cycles, FILE *debug_s, UINT32 cycle_min);

    /* get status */
    struct cpu6502regs          get_cpu_regs() const { return regs; };
    struct cpu6502flags         get_cpu_flags() const { return flags; };

    void                        set_cpu_mem(CPUMemoryManager *cpu_mem){mem_handl = cpu_mem;};

    /* ================ INTERRUPTIONS ==================== */
    void                        enter_irq(bool from_brk);
    void                        enter_nmi();
    void                        enter_reset();

    /* ================ CPU HANDLING ===================== */
    UINT8                       read_mem(MEMADDR addr) { return mem_handl->read(addr); };
    void                        write_mem(MEMADDR addr, UINT8 val) { mem_handl->write(addr, val); };
    void                        reset_cycles(){elapsed_cycles = 0;};
    void                        wait_cycles(UINT16 nr_cycles){elapsed_cycles+=nr_cycles;};
    UINT32                      get_cycles(){return elapsed_cycles;};
    MEMADDR                     get_pc(){return regs.PC;};


    void                        set_return_on_ppu(BOOL new_val){return_on_ppu_op = new_val;};
    BOOL                        get_return_on_ppu(){return return_on_ppu_op;};
    BOOL                        get_skip_next_op_ppu(){return skip_next_op_ppu;};
    void                        set_skip_next_op_ppu(BOOL new_val){skip_next_op_ppu = new_val;};    

/* ================= SAVE STATES =================== */
    cpu6502savestate get_state();
    void             restore_state(cpu6502savestate s);

private:
    /* information about the cpu state */
    struct cpu6502regs          regs;
    struct cpu6502flags         flags;


    /* memory handlers */

    CPUMemoryManager            *mem_handl;
    // functions to read/write from stack don't modify regs.S !
    UINT8                       read_stack() { return mem_handl->read_stack(regs.S); };
    void                        write_stack(UINT8 val) { mem_handl->write_stack(regs.S, val); };

    UINT8                       pop_stack() {
        regs.S = (regs.S + 1) & 0xFF;
        return read_stack();
    };

    void                        push_stack(UINT8 val) {
        write_stack(val);
        regs.S = (regs.S - 1) & 0xFF;
    }

    // fetches the byte pointed to by PC, and increments PC
    UINT8                       fetch_from_pc();
    MEMADDR                     fetch_addr_from_pc();

    /* cycles of the cpu since last call of reset cycles */
    UINT32                      elapsed_cycles;

    // Should return on a read/write to PPU ? (for synchronization)
    BOOL                        return_on_ppu_op;
    BOOL                        skip_next_op_ppu;

/* ================ OPCODES HANDLING ================= */
/* First, a few routines to handle opcodes */

    struct addrmem_res {
        MEMADDR val;
        UINT8 nr_cycles; // if any additional clicks must be counted (eg in case of page boundary crossed)
    };

    MEMADDR                     addr_absolute           (MEMADDR addr);
    struct addrmem_res          addr_absolute_x         (MEMADDR addr);
    struct addrmem_res          addr_absolute_y         (MEMADDR addr);
    // particular case : an absolute indirect read returns an address, and never results in an additional cycle
    MEMADDR                     addr_absolute_indirect  (MEMADDR addr); 
    MEMADDR                     addr_zp                 (ZPADDR addr);
    MEMADDR                     addr_zp_x               (ZPADDR addr);
    MEMADDR                     addr_zp_y               (ZPADDR addr);
    struct addrmem_res          addr_indirect_y         (ZPADDR addr);
    MEMADDR                     addr_x_indirect         (ZPADDR addr);
    MEMADDR                     addr_y_indirect         (ZPADDR addr);

    // an array containing the value of N flag for all 8bits numbers
    UINT8                flagNtable[256];
    void                 setNZflags(UINT8 n);

    UINT8                doADC(UINT8 n);
    UINT8                doSBC(UINT8 n);
    void                 doCMP(UINT8 a, UINT8 b);
    UINT8                doBRANCH(INT8 r);
    UINT8                doSLO(UINT8 val);
    UINT8                doSRE(UINT8 val);
    UINT8                doRRA(UINT8 val);
    UINT8                doRLA(UINT8 val);

/*
-------------- List of handlers
*/
/*
addressing : 
nothing : implicit, or relative
a       : absolute
i       : immediate
d       : zero paging
ax      : absolute indexed by x
dx      : zp indexed by x
ay      : absolute indexed by y
dy      : zp indexed by y
ab      : (a) indirect absolute
xb/yb   : indirect indexed (,x) or (,y) (x/y and bang '!')
by      : indexed indirect (),y (bang '!' and y)
*/

/*
As defined in types.hpp, opcode handlers do not take any argument (they use and modify directly the cpu),
and they return the number of cycles the instruction took. This list also includes illegal ops
*/

    UINT8 BRK();
    UINT8 ORA_xb();
    UINT8 KIL(); // also called STP
    UINT8 SLO_xb();
    UINT8 DOP_d(); // also called (illegal) double NOP
    UINT8 ORA_d();
    UINT8 ASL_d();
    UINT8 SLO_d();
    UINT8 PHP();
    UINT8 ORA_i();
    UINT8 ASL();
    UINT8 AAC_i(); // or ANC
    UINT8 TOP_a(); // also called (illegal) triple NOP
    UINT8 ORA_a();
    UINT8 ASL_a();
    UINT8 SLO_a();
    UINT8 BPL();
    UINT8 ORA_by();
    UINT8 SLO_by();
    UINT8 NOP_dx();
    UINT8 ORA_dx();
    UINT8 ASL_dx();
    UINT8 SLO_dx();
    UINT8 CLC();
    UINT8 ORA_ay();
    UINT8 NOP();
    UINT8 SLO_ay();
    UINT8 NOP_ax();
    UINT8 ORA_ax();
    UINT8 ASL_ax();
    UINT8 SLO_ax();
    UINT8 JSR_a();
    UINT8 AND_xb();
    UINT8 RLA_xb();
    UINT8 BIT_d();
    UINT8 AND_d();
    UINT8 ROL_d();
    UINT8 RLA_d();
    UINT8 PLP();
    UINT8 AND_i();
    UINT8 ROL();
    UINT8 BIT_a();
    UINT8 AND_a();
    UINT8 ROL_a();
    UINT8 RLA_a();
    UINT8 BMI();
    UINT8 AND_by();
    UINT8 RLA_by();
    UINT8 AND_dx();
    UINT8 ROL_dx();
    UINT8 RLA_dx();
    UINT8 SEC();
    UINT8 AND_ay();
    UINT8 RLA_ay();
    UINT8 AND_ax();
    UINT8 ROL_ax();
    UINT8 RLA_ax();
    UINT8 RTI();
    UINT8 EOR_xb();
    UINT8 SRE_xb();
    UINT8 EOR_d();
    UINT8 LSR_d();
    UINT8 SRE_d();
    UINT8 PHA();
    UINT8 EOR_i();
    UINT8 LSR();
    UINT8 ALR_i();
    UINT8 JMP_a();
    UINT8 EOR_a();
    UINT8 LSR_a();
    UINT8 SRE_a();
    UINT8 BVC();
    UINT8 EOR_by();
    UINT8 SRE_by();
    UINT8 EOR_dx();
    UINT8 LSR_dx();
    UINT8 SRE_dx();
    UINT8 CLI();
    UINT8 EOR_ay();
    UINT8 SRE_ay();
    UINT8 EOR_ax();
    UINT8 LSR_ax();
    UINT8 SRE_ax();
    UINT8 RTS();
    UINT8 ADC_xb();
    UINT8 RRA_xb();
    UINT8 ADC_d();
    UINT8 ROR_d();
    UINT8 RRA_d();
    UINT8 PLA();
    UINT8 ADC_i();
    UINT8 ROR();
    UINT8 ARR_i();
    UINT8 JMP_ab();
    UINT8 ADC_a();
    UINT8 ROR_a();
    UINT8 RRA_a();
    UINT8 BVS();
    UINT8 ADC_by();
    UINT8 RRA_by();
    UINT8 ADC_dx();
    UINT8 ROR_dx();
    UINT8 RRA_dx();
    UINT8 SEI();
    UINT8 ADC_ay();
    UINT8 RRA_ay();
    UINT8 ADC_ax();
    UINT8 ROR_ax();
    UINT8 RRA_ax();
    UINT8 DOP_i();
    UINT8 STA_xb();
    UINT8 SAX_xb();
    UINT8 STY_d();
    UINT8 STA_d();
    UINT8 STX_d();
    UINT8 SAX_d();
    UINT8 DEY();
    UINT8 TXA();
    UINT8 XAA_i();
    UINT8 STY_a();
    UINT8 STA_a();
    UINT8 STX_a();
    UINT8 SAX_a();
    UINT8 BCC();
    UINT8 STA_by();
    UINT8 AHX_by();
    UINT8 STY_dx();
    UINT8 STA_dx();
    UINT8 STX_dy();
    UINT8 SAX_dy();
    UINT8 TYA();
    UINT8 STA_ay();
    UINT8 TXS();
    UINT8 TAS_ay();
    UINT8 SHY_ax();
    UINT8 STA_ax();
    UINT8 SHX_ay();
    UINT8 AHX_ay();
    UINT8 LDY_i();
    UINT8 LDA_xb();
    UINT8 LDX_i();
    UINT8 LAX_xb();
    UINT8 LDY_d();
    UINT8 LDA_d();
    UINT8 LDX_d();
    UINT8 LAX_d();
    UINT8 TAY();
    UINT8 LDA_i();
    UINT8 TAX();
    UINT8 LAX_i();
    UINT8 LDY_a();
    UINT8 LDA_a();
    UINT8 LDX_a();
    UINT8 LAX_a();
    UINT8 BCS();
    UINT8 LDA_by();
    UINT8 LAX_by();
    UINT8 LDY_dx();
    UINT8 LDA_dx();
    UINT8 LDX_dy();
    UINT8 LAX_dy();
    UINT8 CLV();
    UINT8 LDA_ay();
    UINT8 TSX();
    UINT8 LAS_ay();
    UINT8 LDY_ax();
    UINT8 LDA_ax();
    UINT8 LDX_ay();
    UINT8 LAX_ay();
    UINT8 CPY_i();
    UINT8 CMP_xb();
    UINT8 DCP_xb();
    UINT8 CPY_d();
    UINT8 CMP_d();
    UINT8 DEC_d();
    UINT8 DCP_d();
    UINT8 INY();
    UINT8 CMP_i();
    UINT8 DEX();
    UINT8 AXS_i();
    UINT8 CPY_a();
    UINT8 CMP_a();
    UINT8 DEC_a();
    UINT8 DCP_a();
    UINT8 BNE();
    UINT8 CMP_by();
    UINT8 DCP_by();
    UINT8 CMP_dx();
    UINT8 DEC_dx();
    UINT8 DCP_dx();
    UINT8 CLD();
    UINT8 CMP_ay();
    UINT8 DCP_ay();
    UINT8 CMP_ax();
    UINT8 DEC_ax();
    UINT8 DCP_ax();
    UINT8 CPX_i();
    UINT8 SBC_xb();
    UINT8 ISC_xb();
    UINT8 CPX_d();
    UINT8 SBC_d();
    UINT8 INC_d();
    UINT8 ISC_d();
    UINT8 INX();
    UINT8 SBC_i();
    UINT8 CPX_a();
    UINT8 SBC_a();
    UINT8 INC_a();
    UINT8 ISC_a();
    UINT8 BEQ();
    UINT8 SBC_by();
    UINT8 ISC_by();
    UINT8 SBC_dx();
    UINT8 INC_dx();
    UINT8 ISC_dx();
    UINT8 SED();
    UINT8 SBC_ay();
    UINT8 ISC_ay();
    UINT8 SBC_ax();
    UINT8 INC_ax();
    UINT8 ISC_ax();
    UINT8 INC_ay();

    UINT8 (cpu6502::*handlers_ptrs[256])();
};

UINT8 flags_to_byte(struct cpu6502flags f);

#endif