#include <sstream>
#include <iostream>
#include <iomanip>
#include "cpu.hpp"

/* ================ DIFFERENT ADDRESSING MODES ================= */

/*
There can be an additional cycle to be taken into account
for example in case of page boundary crossed, there is one additional cycle
Page boundary crossed : 
    -> during an indexed absolute read, if the addition of the low byte gives a carry out, the page boundary is crossed
*/

// yay I know pretty useless
MEMADDR  cpu6502::addr_absolute(MEMADDR addr) 
{
    return addr;
}

cpu6502::addrmem_res  cpu6502::addr_absolute_x(MEMADDR addr) {
    struct addrmem_res res;
    UINT16 page = addr & 0xFF00;
    UINT16 new_addr = addr + (UINT16) regs.X;
    res.val = new_addr; res.nr_cycles = !(page == (new_addr & 0xFF00));
    return res;
}

cpu6502::addrmem_res  cpu6502::addr_absolute_y(MEMADDR addr) {
    struct addrmem_res res;
    UINT16 page = addr & 0xFF00;
    UINT16 new_addr = addr + (UINT16) regs.Y;
    res.val = new_addr; res.nr_cycles = !(page == (new_addr & 0xFF00));
    return res;
}

MEMADDR  cpu6502::addr_absolute_indirect(MEMADDR addr) {
    MEMADDR resaddr = read_mem(addr); // we load the low byte at addr
    // then we have to do this to emulate the wrap around done 
    // when the addr is the last of a page
    UINT8 low_byte = (UINT8) (addr & 0x00FF);
    UINT8 low_byte_next = low_byte+1;
    MEMADDR next_addr = (addr & 0xFF00) | ((UINT16)(low_byte_next));
    UINT8 high_byte = read_mem(next_addr);

    return resaddr | (((UINT16)high_byte) << 8);
}

MEMADDR cpu6502::addr_zp(ZPADDR addr) {
    return (UINT16)addr;
}

MEMADDR cpu6502::addr_zp_x(ZPADDR addr) {
    UINT8 new_addr = addr + regs.X; // the fact that it can result in an "overflow" on the byte is intended
    return (MEMADDR)new_addr;
}

MEMADDR cpu6502::addr_zp_y(ZPADDR addr) {
    UINT8 new_addr = addr + regs.Y; // the fact that it can result in an "overflow" on the byte is intended
    return (MEMADDR)new_addr;
}

cpu6502::addrmem_res cpu6502::addr_indirect_y(ZPADDR addr){
    struct addrmem_res res;
    ZPADDR addr_next = addr+1;
    MEMADDR new_addr = (MEMADDR) read_mem((MEMADDR)addr); // we load the low byte
    new_addr |= ((UINT16)read_mem(addr_next)) << 8; // and the high byte
    UINT16 page = new_addr & 0xFF00; 
    MEMADDR final_addr = new_addr + regs.Y;
    res.val = final_addr; res.nr_cycles = !(page == (final_addr & 0xFF00));

    //std::printf("Y: 0x%02X, offset: 0x%02X, new_addr: 0x%04X, final_addr: 0x%04X, res: 0x%02X, cycles: %d\n", regs.Y, addr, new_addr, final_addr, res.val, res.nr_cycles);
    return res;
}

MEMADDR cpu6502::addr_x_indirect(ZPADDR addr) {
    ZPADDR new_addr = addr + regs.X;
    ZPADDR new_addr_next = new_addr+1;
    MEMADDR final_addr = (MEMADDR)read_mem(new_addr);
    final_addr |= ((UINT16)read_mem(new_addr_next)) << 8;
    return final_addr;
}

MEMADDR cpu6502::addr_y_indirect(ZPADDR addr) {
    ZPADDR new_addr = addr + regs.Y;
    ZPADDR new_addr_next = new_addr+1;
    MEMADDR final_addr = (MEMADDR)read_mem(new_addr);
    final_addr |= ((UINT16)read_mem(new_addr_next)) << 8;
    return final_addr;
}

/* ===================== UTILITIES ========================== */

void cpu6502::setNZflags(UINT8 n) {
    flags.Z = !n;
    flags.N = flagNtable[n];
}

static MEMADDR two_bytes_into_addr(UINT8 low, UINT8 high) {
    return ((MEMADDR) low) | (((MEMADDR) high) << 8);
}

MEMADDR cpu6502::fetch_addr_from_pc() {
    UINT8 low = fetch_from_pc();
    UINT8 high = fetch_from_pc();
    return two_bytes_into_addr(low, high);
}

UINT8 flags_to_byte(cpu6502::cpu6502flags f, UINT8 bit4) {
    UINT8 b = 0;
    if(f.C) b |= 0x01;
    if(f.Z) b |= 0x02;
    if(f.I) b |= 0x04;
    if(f.D) b |= 0x08;
    if(bit4) b |= 0x01;
    b |= 0x20;
    if(f.V) b |= 0x40;
    if(f.N) b |= 0x80;
    return b; // apparently, we put the 2 useless bits to 1 ?
}

static cpu6502::cpu6502flags byte_to_flags(UINT8 b) {
    cpu6502::cpu6502flags f;
    f.C = b & 0x01;
    f.Z = b & 0x02;
    f.I = b & 0x04;
    f.D = b & 0x08;
    f.V = b & 0x40;
    f.N = b & 0x80;
    return f;
}

/* ===================== SAVE STATES ===================== */
cpu6502::cpu6502savestate cpu6502::get_state() {
    cpu6502savestate s;
    // copy regs and flags
    s.cycles = elapsed_cycles;
    s.return_on_ppu = return_on_ppu_op;
    s.skip_next_op = skip_next_op_ppu;
    s.regs = regs;
    s.flags = flags;
    return s;
}

void cpu6502::restore_state(cpu6502savestate s) {
    elapsed_cycles = s.cycles;
    return_on_ppu_op = s.return_on_ppu;
    skip_next_op_ppu = s.skip_next_op;
    regs = s.regs;
    flags = s.flags;
}

/* ================== OPCODES HANDLING ====================== */
// Then, proper opcodes handling

// ---------- FLAG HANDLING

UINT8 cpu6502::CLC() {
    flags.C = 0;
    return 2;
}

UINT8 cpu6502::CLI() {
    flags.I = 0;
    return 2;
}

UINT8 cpu6502::CLD() {
    flags.D = 0; // not used anyway
    return 2;
}

UINT8 cpu6502::CLV() {
    flags.V = 0;
    return 2;
}

UINT8 cpu6502::SEC() {
    flags.C = 1;
    return 2;
}

UINT8 cpu6502::SEI() {
    flags.I = 1;
    return 2;
}

UINT8 cpu6502::SED() {
    flags.D = 1;
    return 2;
}

// ----------- TRANSFER BETWEEN REGS

UINT8 cpu6502::TAX() {
    regs.X = regs.A;
    setNZflags(regs.X);
    return 2;
}

UINT8 cpu6502::TXA() {
    regs.A = regs.X;
    setNZflags(regs.A);
    return 2;
}

UINT8 cpu6502::TAY() {
    regs.Y = regs.A;
    setNZflags(regs.Y);
    return 2;
}

UINT8 cpu6502::TYA() {
    regs.A = regs.Y;
    setNZflags(regs.A);
    return 2;
}

UINT8 cpu6502::INX() {
    setNZflags(++regs.X);
    return 2;
}

UINT8 cpu6502::DEX() {
    setNZflags(--regs.X);
    return 2;
}

UINT8 cpu6502::INY() {
    setNZflags(++regs.Y);
    return 2;
}

UINT8 cpu6502::DEY() {
    setNZflags(--regs.Y);
    return 2;
}

// ---------------- NOP INSTR

UINT8 cpu6502::NOP() {
    return 2;
}

UINT8 cpu6502::DOP_i() {
    fetch_from_pc();
    return 2;
}

UINT8 cpu6502::DOP_d() {
    fetch_from_pc();
    return 3;
}

UINT8 cpu6502::NOP_dx() {
    fetch_from_pc();
    return 4;
}

UINT8 cpu6502::TOP_a() {
    fetch_from_pc();
    fetch_from_pc();
    return 4;
}

UINT8 cpu6502::NOP_ax() {
    struct addrmem_res res;
    MEMADDR a = fetch_addr_from_pc();
    res = addr_absolute_x(a);
    return 4 + res.nr_cycles;
}

/*--------------- LOAD INSTR */

// A
// ----

UINT8 cpu6502::LDA_i() {
    regs.A = fetch_from_pc();
    setNZflags(regs.A);
    return 2;
}

UINT8 cpu6502::LDA_d() {
    ZPADDR zp = fetch_from_pc();
    regs.A = read_mem((MEMADDR) zp); // we don't really need addr_zp...
    setNZflags(regs.A);
    return 3;
}

UINT8 cpu6502::LDA_dx() {
    ZPADDR zp = fetch_from_pc();
    regs.A = read_mem(addr_zp_x(zp));
    setNZflags(regs.A);
    return 4;
}

UINT8 cpu6502::LDA_a() {
    MEMADDR a = fetch_addr_from_pc();
    regs.A = read_mem(a);
    setNZflags(regs.A);
    return 4;
}

UINT8 cpu6502::LDA_ax() {
    struct addrmem_res res;
    MEMADDR a = fetch_addr_from_pc();
    res = addr_absolute_x(a);
    regs.A = read_mem(res.val);
    setNZflags(regs.A);
    return 4+res.nr_cycles;
}

UINT8 cpu6502::LDA_ay() {
    struct addrmem_res res;
    MEMADDR a = fetch_addr_from_pc();
    res = addr_absolute_y(a);
    regs.A = read_mem(res.val);
    setNZflags(regs.A);
    return 4+res.nr_cycles;
}

UINT8 cpu6502::LDA_xb() {
    ZPADDR zp = fetch_from_pc();
    regs.A = read_mem(addr_x_indirect(zp));
    setNZflags(regs.A);
    return 6;
}

UINT8 cpu6502::LDA_by() {
    struct addrmem_res res;
    ZPADDR zp = fetch_from_pc();
    res = addr_indirect_y(zp);
    regs.A = read_mem(res.val);
    setNZflags(regs.A);
    return 5+res.nr_cycles;
}

// X
// ---

UINT8 cpu6502::LDX_i() {
    regs.X = fetch_from_pc();
    setNZflags(regs.X);
    return 2;
}

UINT8 cpu6502::LDX_d() {
    ZPADDR zp = fetch_from_pc();
    regs.X = read_mem((MEMADDR) zp); // we don't really need addr_zp...
    setNZflags(regs.X);
    return 3;
}

UINT8 cpu6502::LDX_dy() {
    ZPADDR zp = fetch_from_pc();
    regs.X = read_mem(addr_zp_y(zp));
    setNZflags(regs.X);
    return 4;
}

UINT8 cpu6502::LDX_a() {
    MEMADDR a = fetch_addr_from_pc();
    regs.X = read_mem(a);
    setNZflags(regs.X);
    return 4;
}

UINT8 cpu6502::LDX_ay() {
    struct addrmem_res res;
    MEMADDR a = fetch_addr_from_pc();
    res = addr_absolute_y(a);
    regs.X = read_mem(res.val);
    setNZflags(regs.X);
    return 4+res.nr_cycles;
}

// Y
// ---

UINT8 cpu6502::LDY_i() {
    regs.Y = fetch_from_pc();
    setNZflags(regs.Y);
    return 2;
}

UINT8 cpu6502::LDY_d() {
    ZPADDR zp = fetch_from_pc();
    regs.Y = read_mem((MEMADDR) zp); // we don't really need addr_zp...
    setNZflags(regs.Y);
    return 3;
}

UINT8 cpu6502::LDY_dx() {
    ZPADDR zp = fetch_from_pc();
    regs.Y = read_mem(addr_zp_x(zp));
    setNZflags(regs.Y);
    return 4;
}

UINT8 cpu6502::LDY_a() {
    MEMADDR a = fetch_addr_from_pc();
    regs.Y = read_mem(a);
    setNZflags(regs.Y);
    return 4;
}

UINT8 cpu6502::LDY_ax() {
    struct addrmem_res res;
    MEMADDR a = fetch_addr_from_pc();
    res = addr_absolute_x(a);
    regs.Y = read_mem(res.val);
    setNZflags(regs.Y);
    return 4+res.nr_cycles;
}

// ------------------ STORE INSTR

// A
// ---

UINT8 cpu6502::STA_d() {
    ZPADDR zp = fetch_from_pc();
    write_mem((MEMADDR) zp, regs.A);
    return 3;
}

UINT8 cpu6502::STA_dx() {
    ZPADDR zp = fetch_from_pc();
    MEMADDR a = addr_zp_x(zp);
    //std::printf("STA_dx PC=0x%04X addr=0x%04X A=0x%02X X=0x%02X, C=%d\n", regs.PC, a, regs.A, regs.X, !!(flags.C));
    write_mem(a, regs.A);
    return 4;
}

UINT8 cpu6502::STA_a() {
    MEMADDR a = fetch_addr_from_pc();
    write_mem(a, regs.A);
    return 4;
}

UINT8 cpu6502::STA_ax() {
    struct addrmem_res res;
    MEMADDR a = fetch_addr_from_pc();
    res = addr_absolute_x(a);
    write_mem(res.val, regs.A);
    return 5; // no additional cycle, apparently
}

UINT8 cpu6502::STA_ay() {
    struct addrmem_res res;
    MEMADDR a = fetch_addr_from_pc();
    res = addr_absolute_y(a);
    write_mem(res.val, regs.A);
    return 5; // idem
}

UINT8 cpu6502::STA_xb() {
    ZPADDR zp = fetch_from_pc();
    MEMADDR a = addr_x_indirect(zp);
    write_mem(a, regs.A);
    return 6;
}

UINT8 cpu6502::STA_by() {
    struct addrmem_res res;
    ZPADDR zp = fetch_from_pc();
    res = addr_indirect_y(zp);
    write_mem(res.val, regs.A);
    return 6; // no additional cycle
}

// X
// ---

UINT8 cpu6502::STX_d() {
    ZPADDR zp = fetch_from_pc();
    write_mem((MEMADDR) zp, regs.X);
    return 3;
}

UINT8 cpu6502::STX_dy() {
    ZPADDR zp = fetch_from_pc();
    MEMADDR a = addr_zp_y(zp);
    write_mem(a, regs.X);
    return 4;
}

UINT8 cpu6502::STX_a() {
    MEMADDR a = fetch_addr_from_pc();
    write_mem(a, regs.X);
    return 4;
}

// Y
// ---

UINT8 cpu6502::STY_d() {
    ZPADDR zp = fetch_from_pc();
    write_mem((MEMADDR) zp, regs.Y);
    return 3;
}

UINT8 cpu6502::STY_dx() {
    ZPADDR zp = fetch_from_pc();
    MEMADDR a = addr_zp_x(zp);
    write_mem(a, regs.Y);
    return 4;
}

UINT8 cpu6502::STY_a() {
    MEMADDR a = fetch_addr_from_pc();
    write_mem(a, regs.Y);
    return 4;
}

// --------- INC / DEC OPS

// dec
// ---

UINT8 cpu6502::DEC_d() {
    MEMADDR zp = (MEMADDR) fetch_from_pc();
    UINT8 val = read_mem(zp);
    write_mem(zp, --val);
    setNZflags(val);
    return 5;
}

UINT8 cpu6502::DEC_dx() {
    ZPADDR zp = fetch_from_pc();
    MEMADDR a = addr_zp_x(zp);
    UINT8 val = read_mem(a);
    write_mem(a, --val);
    setNZflags(val);
    return 6;
}

UINT8 cpu6502::DEC_a() {
    MEMADDR a = fetch_addr_from_pc();
    UINT8 val = read_mem(a);
    write_mem(a, --val);
    setNZflags(val);
    return 6;
}

UINT8 cpu6502::DEC_ax() {
    struct addrmem_res res;
    MEMADDR a = fetch_addr_from_pc();
    res = addr_absolute_x(a);
    UINT8 val = read_mem(res.val);
    write_mem(res.val, --val);
    setNZflags(val);
    return 7; // no additional cycle
}

// inc
// ---

UINT8 cpu6502::INC_d() {
    MEMADDR zp = (MEMADDR) fetch_from_pc();
    UINT8 val = read_mem(zp);
    write_mem(zp, ++val);
    setNZflags(val);
    return 5;
}

UINT8 cpu6502::INC_dx() {
    ZPADDR zp = fetch_from_pc();
    MEMADDR a = addr_zp_x(zp);
    UINT8 val = read_mem(a);
    write_mem(a, ++val);
    setNZflags(val);
    return 6;
}

UINT8 cpu6502::INC_a() {
    MEMADDR a = fetch_addr_from_pc();
    UINT8 val = read_mem(a);
    write_mem(a, ++val);
    setNZflags(val);
    return 6;
}

UINT8 cpu6502::INC_ax() {
    struct addrmem_res res;
    MEMADDR a = fetch_addr_from_pc();
    res = addr_absolute_x(a);
    UINT8 val = read_mem(res.val);
    write_mem(res.val, ++val);
    setNZflags(val);
    return 7; // no additional cycle
}

UINT8 cpu6502::INC_ay() {
    struct addrmem_res res;
    MEMADDR a = fetch_addr_from_pc();
    res = addr_absolute_y(a);
    UINT8 val = read_mem(res.val);
    write_mem(res.val, ++val);
    setNZflags(val);
    return 7; // no additional cycle
}

// ------------------- BITWISE OPS

// AND
// ---

UINT8 cpu6502::AND_i() {
    UINT8 op = fetch_from_pc();
    regs.A &= op;
    setNZflags(regs.A);
    return 2;
}

UINT8 cpu6502::AND_d() {
    ZPADDR zp = fetch_from_pc();
    regs.A &= read_mem((MEMADDR) zp);
    setNZflags(regs.A);
    return 3;
}

UINT8 cpu6502::AND_dx() {
    ZPADDR zp = fetch_from_pc();
    MEMADDR a = addr_zp_x(zp);
    regs.A &= read_mem(a);
    setNZflags(regs.A);
    return 4;
}

UINT8 cpu6502::AND_a() {
    MEMADDR a = fetch_addr_from_pc();
    regs.A &= read_mem(a);
    setNZflags(regs.A);
    return 4;
}

UINT8 cpu6502::AND_ax() {
    struct addrmem_res res;
    MEMADDR a = fetch_addr_from_pc();
    res = addr_absolute_x(a);
    regs.A &= read_mem(res.val);
    setNZflags(regs.A);
    return 4+res.nr_cycles;
}

UINT8 cpu6502::AND_ay() {
    struct addrmem_res res;
    MEMADDR a = fetch_addr_from_pc();
    res = addr_absolute_y(a);
    regs.A &= read_mem(res.val);
    setNZflags(regs.A);
    return 4+res.nr_cycles;
}

UINT8 cpu6502::AND_xb() {
    ZPADDR zp = fetch_from_pc();
    MEMADDR a = addr_x_indirect(zp);
    regs.A &= read_mem(a);
    setNZflags(regs.A);
    return 6;
}

UINT8 cpu6502::AND_by() {
    struct addrmem_res res;
    ZPADDR zp = fetch_from_pc();
    res = addr_indirect_y(zp);
    regs.A &= read_mem(res.val);
    setNZflags(regs.A);
    return 5+res.nr_cycles;
}

// ORA
// ---

UINT8 cpu6502::ORA_i() {
    UINT8 op = fetch_from_pc();
    regs.A |= op;
    setNZflags(regs.A);
    return 2;
}

UINT8 cpu6502::ORA_d() {
    ZPADDR zp = fetch_from_pc();
    regs.A |= read_mem((MEMADDR) zp);
    setNZflags(regs.A);
    return 3;
}

UINT8 cpu6502::ORA_dx() {
    ZPADDR zp = fetch_from_pc();
    MEMADDR a = addr_zp_x(zp);
    regs.A |= read_mem(a);
    setNZflags(regs.A);
    return 4;
}

UINT8 cpu6502::ORA_a() {
    MEMADDR a = fetch_addr_from_pc();
    regs.A |= read_mem(a);
    setNZflags(regs.A);
    return 4;
}

UINT8 cpu6502::ORA_ax() {
    struct addrmem_res res;
    MEMADDR a = fetch_addr_from_pc();
    res = addr_absolute_x(a);
    regs.A |= read_mem(res.val);
    setNZflags(regs.A);
    return 4+res.nr_cycles;
}

UINT8 cpu6502::ORA_ay() {
    struct addrmem_res res;
    MEMADDR a = fetch_addr_from_pc();
    res = addr_absolute_y(a);
    regs.A |= read_mem(res.val);
    setNZflags(regs.A);
    return 4+res.nr_cycles;
}

UINT8 cpu6502::ORA_xb() {
    ZPADDR zp = fetch_from_pc();
    MEMADDR a = addr_x_indirect(zp);
    regs.A |= read_mem(a);
    setNZflags(regs.A);
    return 6;
}

UINT8 cpu6502::ORA_by() {
    struct addrmem_res res;
    ZPADDR zp = fetch_from_pc();
    res = addr_indirect_y(zp);
    regs.A |= read_mem(res.val);
    setNZflags(regs.A);
    return 5+res.nr_cycles;
}

// EOR
// ---

UINT8 cpu6502::EOR_i() {
    UINT8 op = fetch_from_pc();
    regs.A ^= op;
    setNZflags(regs.A);
    return 2;
}

UINT8 cpu6502::EOR_d() {
    ZPADDR zp = fetch_from_pc();
    regs.A ^= read_mem((MEMADDR) zp);
    setNZflags(regs.A);
    return 3;
}

UINT8 cpu6502::EOR_dx() {
    ZPADDR zp = fetch_from_pc();
    MEMADDR a = addr_zp_x(zp);
    regs.A ^= read_mem(a);
    setNZflags(regs.A);
    return 4;
}

UINT8 cpu6502::EOR_a() {
    MEMADDR a = fetch_addr_from_pc();
    regs.A ^= read_mem(a);
    setNZflags(regs.A);
    return 4;
}

UINT8 cpu6502::EOR_ax() {
    struct addrmem_res res;
    MEMADDR a = fetch_addr_from_pc();
    res = addr_absolute_x(a);
    regs.A ^= read_mem(res.val);
    setNZflags(regs.A);
    return 4+res.nr_cycles;
}

UINT8 cpu6502::EOR_ay() {
    struct addrmem_res res;
    MEMADDR a = fetch_addr_from_pc();
    res = addr_absolute_y(a);
    regs.A ^= read_mem(res.val);
    setNZflags(regs.A);
    return 4+res.nr_cycles;
}

UINT8 cpu6502::EOR_xb() {
    ZPADDR zp = fetch_from_pc();
    MEMADDR a = addr_x_indirect(zp);
    regs.A ^= read_mem(a);
    setNZflags(regs.A);
    return 6;
}

UINT8 cpu6502::EOR_by() {
    struct addrmem_res res;
    ZPADDR zp = fetch_from_pc();
    res = addr_indirect_y(zp);
    regs.A ^= read_mem(res.val);
    setNZflags(regs.A);
    return 5+res.nr_cycles;
}

// ----------------------- STACK OPS

UINT8 cpu6502::TXS() {
    regs.S = regs.X;
    return 2;
}

UINT8 cpu6502::TSX() {
    regs.X = regs.S;
    setNZflags(regs.X);
    return 2;
}

UINT8 cpu6502::PHA() {
    push_stack(regs.A);
    return 3;
}

UINT8 cpu6502::PLA() {
    regs.A = pop_stack();
    setNZflags(regs.A);
    return 4;
}

UINT8 cpu6502::PHP() {
    UINT8 flags_b = flags_to_byte(flags, 1);
    push_stack(flags_b);
    return 3;
}

UINT8 cpu6502::PLP() {
    flags = byte_to_flags(pop_stack());
    flags.I = 1;
    return 4;
}

// ------------------- SHIFTS AND ROTATIONS

// ASL
// ---

UINT8 cpu6502::ASL() {
    flags.C = regs.A & 0x80;
    regs.A <<= 1;
    setNZflags(regs.A);
    return 2;
}

UINT8 cpu6502::ASL_d() {
    ZPADDR zp = fetch_from_pc();
    MEMADDR a = (MEMADDR) zp;
    UINT8 val = read_mem(a);
    flags.C = val & 0x80;
    val <<= 1;
    setNZflags(val);
    write_mem(a, val);
    return 5;
}

UINT8 cpu6502::ASL_dx() {
    ZPADDR zp = fetch_from_pc();
    MEMADDR a = addr_zp_x(zp);
    UINT8 val = read_mem(a);
    flags.C = val & 0x80;
    val <<= 1;
    setNZflags(val);
    write_mem(a, val);
    return 6;
}

UINT8 cpu6502::ASL_a() {
    MEMADDR a = fetch_addr_from_pc();
    UINT8 val = read_mem(a);
    flags.C = val & 0x80;
    val <<= 1;
    setNZflags(val);
    write_mem(a, val);
    return 6;
}

UINT8 cpu6502::ASL_ax() {
    struct addrmem_res res;
    MEMADDR a = fetch_addr_from_pc();
    res = addr_absolute_x(a);
    UINT8 val = read_mem(res.val);
    flags.C = val & 0x80;
    val <<= 1;
    setNZflags(val);
    write_mem(res.val, val);
    return 7;
}

// LSR
// ---

UINT8 cpu6502::LSR() {
    flags.C = regs.A & 0x01;
    regs.A >>= 1;
    setNZflags(regs.A);
    return 2;
}

UINT8 cpu6502::LSR_d() {
    ZPADDR zp = fetch_from_pc();
    MEMADDR a = (MEMADDR) zp;
    UINT8 val = read_mem(a);
    flags.C = val & 0x01;
    val >>= 1;
    setNZflags(val);
    write_mem(a, val);
    return 5;
}

UINT8 cpu6502::LSR_dx() {
    ZPADDR zp = fetch_from_pc();
    MEMADDR a = addr_zp_x(zp);
    UINT8 val = read_mem(a);
    flags.C = val & 0x01;
    val >>= 1;
    setNZflags(val);
    write_mem(a, val);
    return 6;
}

UINT8 cpu6502::LSR_a() {
    MEMADDR a = fetch_addr_from_pc();
    UINT8 val = read_mem(a);
    flags.C = val & 0x01;
    val >>= 1;
    setNZflags(val);
    write_mem(a, val);
    return 6;
}

UINT8 cpu6502::LSR_ax() {
    struct addrmem_res res;
    MEMADDR a = fetch_addr_from_pc();
    res = addr_absolute_x(a);
    UINT8 val = read_mem(res.val);
    flags.C = val & 0x01;
    val >>= 1;
    setNZflags(val);
    write_mem(res.val, val);
    return 7;
}

// ROR
// ---

UINT8 cpu6502::ROR() {
    BOOL c = flags.C;
    flags.C = regs.A & 0x01;
    regs.A >>= 1;
    if(c) regs.A |= 0x80;
    setNZflags(regs.A);
    return 2;
}

UINT8 cpu6502::ROR_d() {
    ZPADDR zp = fetch_from_pc();
    MEMADDR a = (MEMADDR) zp;
    UINT8 val = read_mem(a);
    BOOL c = flags.C;
    flags.C = val & 0x01;
    val >>= 1;
    if(c) val |= 0x80;
    setNZflags(val);
    write_mem(a, val);
    return 5;
}

UINT8 cpu6502::ROR_dx() {
    ZPADDR zp = fetch_from_pc();
    MEMADDR a = addr_zp_x(zp);
    UINT8 val = read_mem(a);
    BOOL c = flags.C;
    flags.C = val & 0x01;
    val >>= 1;
    if(c) val |= 0x80;
    setNZflags(val);
    write_mem(a, val);
    return 6;
}

UINT8 cpu6502::ROR_a() {
    MEMADDR a = fetch_addr_from_pc();
    UINT8 val = read_mem(a);
    BOOL c = flags.C;
    flags.C = val & 0x01;
    val >>= 1;
    if(c) val |= 0x80;
    setNZflags(val);
    write_mem(a, val);
    return 6;
}

UINT8 cpu6502::ROR_ax() {
    struct addrmem_res res;
    MEMADDR a = fetch_addr_from_pc();
    res = addr_absolute_x(a);
    UINT8 val = read_mem(res.val);
    BOOL c = flags.C;
    flags.C = val & 0x01;
    val >>= 1;
    if(c) val |= 0x80;
    setNZflags(val);
    write_mem(res.val, val);
    return 7;
}

// ROL
// ---

UINT8 cpu6502::ROL() {
    BOOL c = flags.C;
    flags.C = regs.A & 0x80;
    regs.A <<= 1;
    if(c) regs.A |= 0x01;
    setNZflags(regs.A);
    return 2;
}

UINT8 cpu6502::ROL_d() {
    ZPADDR zp = fetch_from_pc();
    MEMADDR a = (MEMADDR) zp;
    UINT8 val = read_mem(a);
    BOOL c = flags.C;
    flags.C = val & 0x80;
    val <<= 1;
    if(c) val |= 0x01;
    setNZflags(val);
    write_mem(a, val);
    return 5;
}

UINT8 cpu6502::ROL_dx() {
    ZPADDR zp = fetch_from_pc();
    MEMADDR a = addr_zp_x(zp);
    UINT8 val = read_mem(a);
    BOOL c = flags.C;
    flags.C = val & 0x80;
    val <<= 1;
    if(c) val |= 0x01;
    setNZflags(val);
    write_mem(a, val);
    return 6;
}

UINT8 cpu6502::ROL_a() {
    MEMADDR a = fetch_addr_from_pc();
    UINT8 val = read_mem(a);
    BOOL c = flags.C;
    flags.C = val & 0x80;
    val <<= 1;
    if(c) val |= 0x01;
    setNZflags(val);
    write_mem(a, val);
    return 6;
}

UINT8 cpu6502::ROL_ax() {
    struct addrmem_res res;
    MEMADDR a = fetch_addr_from_pc();
    res = addr_absolute_x(a);
    UINT8 val = read_mem(res.val);
    BOOL c = flags.C;
    flags.C = val & 0x80;
    val <<= 1;
    if(c) val |= 0x01;
    setNZflags(val);
    write_mem(res.val, val);
    return 7;
}

// ------------ BIT OP

UINT8 cpu6502::BIT_d() {
    ZPADDR zp = fetch_from_pc();
    UINT8 val = read_mem((MEMADDR) zp);
    flags.Z = !(regs.A & val);
    flags.N = val & 0x80;
    flags.V = val & 0x40;
    return 3;
}

UINT8 cpu6502::BIT_a() {
    MEMADDR a = fetch_addr_from_pc();
    UINT8 val = read_mem(a);
    flags.Z = !(regs.A & val);
    flags.N = val & 0x80;
    flags.V = val & 0x40;
    return 4;
}

// -------------- ARITHMETIC

// ADC
// ---

UINT8 cpu6502::doADC(UINT8 n) {
    // TODO : optimize this
    flags.C = !!(flags.C);
    //std::printf("0x%02X 0x%02X\n", regs.A, n);
    // sorry i'm not very smart so I do it like this
    UINT16 ures = (UINT16) regs.A + (UINT16) n + (UINT16) flags.C;
    INT16  sres = (INT16)  ((INT8)regs.A) + (INT16)  ((INT8)n) + (INT16)  flags.C;
    //std::printf("%d %d\n", ures, sres);
    UINT8  res  = (UINT8) ures;

    // carry : we look at ures
    flags.C = !!(ures & 0x0100);
    // overflow
    flags.V = (sres < -128) || (sres > 127);

    setNZflags(res);
    return res;
}

UINT8 cpu6502::ADC_i() {
    regs.A = doADC(fetch_from_pc());
    return 2;
}

UINT8 cpu6502::ADC_d() {
    UINT8 val = read_mem((MEMADDR) fetch_from_pc());
    regs.A = doADC(val);
    return 3;
}

UINT8 cpu6502::ADC_dx() {
    MEMADDR a = addr_zp_x(fetch_from_pc());
    regs.A = doADC(read_mem(a));
    return 4;
}

UINT8 cpu6502::ADC_a() {
    MEMADDR a = fetch_addr_from_pc();
    regs.A = doADC(read_mem(a));
    return 4;
}

UINT8 cpu6502::ADC_ax() {
    struct addrmem_res res;
    res = addr_absolute_x(fetch_addr_from_pc());
    regs.A = doADC(read_mem(res.val));
    return 4+res.nr_cycles;
}

UINT8 cpu6502::ADC_ay() {
    struct addrmem_res res;
    res = addr_absolute_y(fetch_addr_from_pc());
    regs.A = doADC(read_mem(res.val));
    return 4+res.nr_cycles;
}

UINT8 cpu6502::ADC_xb() {
    MEMADDR a = addr_x_indirect(fetch_from_pc());
    regs.A = doADC(read_mem(a));
    return 6;
}

UINT8 cpu6502::ADC_by() {
    struct addrmem_res res;
    res = addr_indirect_y(fetch_from_pc());
    regs.A = doADC(read_mem(res.val));
    return 5+res.nr_cycles;
}

// SBC
// ---

UINT8 cpu6502::doSBC(UINT8 n) {
    // TODO : test and optimize it too !
    flags.C = !!(flags.C);
    UINT16 ures = (UINT16) regs.A - (UINT16) n - (UINT16) !flags.C;
    INT16  sres = (INT16)  ((INT8)regs.A) - (INT16)  ((INT8)n) - (INT16)  !flags.C;
    UINT8  res = (UINT8) ures;

    flags.C = !(ures & 0x0100);
    flags.V = (sres < -128) || (sres > 127);

    setNZflags(res);
    return res;
}

UINT8 cpu6502::SBC_i() {
    regs.A = doSBC(fetch_from_pc());
    return 2;
}

UINT8 cpu6502::SBC_d() {
    UINT8 val = read_mem((MEMADDR) fetch_from_pc());
    regs.A = doSBC(val);
    return 3;
}

UINT8 cpu6502::SBC_dx() {
    MEMADDR a = addr_zp_x(fetch_from_pc());
    regs.A = doSBC(read_mem(a));
    return 4;
}

UINT8 cpu6502::SBC_a() {
    MEMADDR a = fetch_addr_from_pc();
    regs.A = doSBC(read_mem(a));
    return 4;
}

UINT8 cpu6502::SBC_ax() {
    struct addrmem_res res;
    res = addr_absolute_x(fetch_addr_from_pc());
    regs.A = doSBC(read_mem(res.val));
    return 4+res.nr_cycles;
}

UINT8 cpu6502::SBC_ay() {
    struct addrmem_res res;
    res = addr_absolute_y(fetch_addr_from_pc());
    regs.A = doSBC(read_mem(res.val));
    return 4+res.nr_cycles;
}

UINT8 cpu6502::SBC_xb() {
    MEMADDR a = addr_x_indirect(fetch_from_pc());
    regs.A = doSBC(read_mem(a));
    return 6;
}

UINT8 cpu6502::SBC_by() {
    struct addrmem_res res;
    res = addr_indirect_y(fetch_from_pc());
    regs.A = doSBC(read_mem(res.val));
    return 5+res.nr_cycles;
}

// ----------- SUBROUTINES

UINT8 cpu6502::RTS() {
    //std::printf("RTS : 0x%02X\n", regs.S);
    UINT8 low_pc = pop_stack();
    UINT8 high_pc = pop_stack();
    regs.PC = two_bytes_into_addr(low_pc, high_pc);

    regs.PC++;
    return 6;
}

UINT8 cpu6502::RTI() {
    UINT8 flags_byte = pop_stack();
    flags = byte_to_flags(flags_byte);
    UINT8 low_pc = pop_stack();
    UINT8 high_pc = pop_stack();
    regs.PC = two_bytes_into_addr(low_pc, high_pc);

    return 6;
}

UINT8 cpu6502::JMP_a() {
    regs.PC = fetch_addr_from_pc();
    return 3;
}

UINT8 cpu6502::JMP_ab() {
    MEMADDR dest = addr_absolute_indirect(
                    fetch_addr_from_pc());
    regs.PC = dest;
    return 5;
}

UINT8 cpu6502::JSR_a() {
    MEMADDR a = fetch_addr_from_pc();
    regs.PC--;

    UINT8 high = (UINT8)(regs.PC >> 8);
    push_stack(high);
    push_stack((UINT8)regs.PC); // push low PC

    regs.PC = a;
    return 6;
}

UINT8 cpu6502::BRK() {
    if(flags.I) return 2;
    regs.PC++; // so that PC =  &BRK + 2
    enter_irq(true);
    flags.I = 1;
    return 7;
}

// ---------------------- COMPARE OPS

// CMP
// ---

void cpu6502::doCMP(UINT8 a, UINT8 b) {
    UINT8 m = a - b;
    flags.C = (a >= b);
    flags.N = (m & 0x80);
    flags.Z = !m;
}

UINT8 cpu6502::CMP_i() {
    UINT8 val = fetch_from_pc();
    doCMP(regs.A, val);
    return 2;
}

UINT8 cpu6502::CMP_d() {
    UINT8 val = read_mem((MEMADDR)fetch_from_pc());
    doCMP(regs.A, val);
    return 3;
}

UINT8 cpu6502::CMP_dx() {
    UINT8 val = read_mem(addr_zp_x(fetch_from_pc()));
    doCMP(regs.A, val);
    return 4;
}

UINT8 cpu6502::CMP_a() {
    UINT8 val = read_mem(fetch_addr_from_pc());
    doCMP(regs.A, val);
    return 4;
}

UINT8 cpu6502::CMP_ax() {
    struct addrmem_res res;
    res = addr_absolute_x(fetch_addr_from_pc());
    UINT8 val = read_mem(res.val);
    doCMP(regs.A, val);
    return 4+res.nr_cycles;
}

UINT8 cpu6502::CMP_ay() {
    struct addrmem_res res;
    res = addr_absolute_y(fetch_addr_from_pc());
    UINT8 val = read_mem(res.val);
    doCMP(regs.A, val);
    return 4+res.nr_cycles;
}

UINT8 cpu6502::CMP_xb() {
    UINT8 val = read_mem(addr_x_indirect(fetch_from_pc()));
    doCMP(regs.A, val);
    return 6;
}

UINT8 cpu6502::CMP_by() {
    struct addrmem_res res;
    res = addr_indirect_y(fetch_from_pc());
    UINT8 val = read_mem(res.val);
    doCMP(regs.A, val);
    return 5+res.nr_cycles;
}

// CPX
// ---

UINT8 cpu6502::CPX_i() {
    UINT8 val = fetch_from_pc();
    doCMP(regs.X, val);
    return 2;
}

UINT8 cpu6502::CPX_d() {
    UINT8 val = read_mem((MEMADDR) fetch_from_pc());
    doCMP(regs.X, val);
    return 3;
}

UINT8 cpu6502::CPX_a() {
    UINT8 val = read_mem(fetch_addr_from_pc());
    doCMP(regs.X, val);
    return 4;
}

// CPY
// ---

UINT8 cpu6502::CPY_i() {
    UINT8 val = fetch_from_pc();
    doCMP(regs.Y, val);
    return 2;
}

UINT8 cpu6502::CPY_d() {
    UINT8 val = read_mem((MEMADDR) fetch_from_pc());
    doCMP(regs.Y, val);
    return 3;
}

UINT8 cpu6502::CPY_a() {
    UINT8 val = read_mem(fetch_addr_from_pc());
    doCMP(regs.Y, val);
    return 4;
}

// -------------------------- BRANCHINGS

UINT8 cpu6502::doBRANCH(INT8 r) {
    // this assumes that PC points after the branch operand
    UINT16 new_pc = regs.PC + (INT16)r;
    UINT8 page_crossed = ((new_pc & 0xFF00) != (regs.PC & 0xFF00));
    regs.PC = new_pc;
    return page_crossed;
}

UINT8 cpu6502::BPL() {
    UINT8 r = fetch_from_pc();
    if(!flags.N) return 3 + doBRANCH(r);
    else return 2;
}

UINT8 cpu6502::BCC() {
    UINT8 r = fetch_from_pc();
    if(!flags.C) return 3 + doBRANCH(r);
    else return 2;
}

UINT8 cpu6502::BCS() {
    UINT8 r = fetch_from_pc();
    if(flags.C) return 3 + doBRANCH(r);
    else return 2;
}

UINT8 cpu6502::BNE() {
    UINT8 r = fetch_from_pc();
    if(!flags.Z) return 3 + doBRANCH(r);
    else return 2;
}

UINT8 cpu6502::BEQ() {
    UINT8 r = fetch_from_pc();
    if(flags.Z) return 3 + doBRANCH(r);
    else return 2;
}

UINT8 cpu6502::BMI() {
    UINT8 r = fetch_from_pc();
    if(flags.N) return 3 + doBRANCH(r);
    else return 2;
}

UINT8 cpu6502::BVC() {
    UINT8 r = fetch_from_pc();
    if(!flags.V) return 3 + doBRANCH(r);
    else return 2;
}

UINT8 cpu6502::BVS() {
    UINT8 r = fetch_from_pc();
    if(flags.V) return 3 + doBRANCH(r);
    else return 2;
}

// -------------------------------------
// now for the gangsta ops
// (illegal/undocumented ops)

// KIL
// ---

UINT8 cpu6502::KIL() {
    throw CPUHalted(regs.PC);
    return 1; // yeah useless
}

// SLO
// ---

UINT8 cpu6502::doSLO(UINT8 val) {
    flags.C = val & 0x80;
    val <<= 1;
    regs.A |= val;
    setNZflags(regs.A);
    return val;
}

UINT8 cpu6502::SLO_a() {
    MEMADDR a = fetch_addr_from_pc();
    UINT8 val = read_mem(a);
    write_mem(a, doSLO(val));
    return 6;
}

UINT8 cpu6502::SLO_d() {
    MEMADDR zp = fetch_from_pc();
    UINT8 val = read_mem(zp);
    write_mem(zp, doSLO(val));
    return 5;
}

UINT8 cpu6502::SLO_dx() {
    MEMADDR a = addr_zp_x(fetch_from_pc());
    UINT8 val = read_mem(a);
    write_mem(a, doSLO(val));
    return 6;
}

UINT8 cpu6502::SLO_ax() {
    struct addrmem_res res;
    res = addr_absolute_x(fetch_addr_from_pc());
    UINT8 val = read_mem(res.val);
    write_mem(res.val, doSLO(val));
    return 7;
}

UINT8 cpu6502::SLO_ay() {
    struct addrmem_res res;
    res = addr_absolute_y(fetch_addr_from_pc());
    UINT8 val = read_mem(res.val);
    write_mem(res.val, doSLO(val));
    return 7;
}

UINT8 cpu6502::SLO_xb() {
    MEMADDR a = addr_x_indirect(fetch_from_pc());
    UINT8 val = read_mem(a);
    write_mem(a, doSLO(val));
    return 8;
}

UINT8 cpu6502::SLO_by() {
    struct addrmem_res res;
    res = addr_indirect_y(fetch_from_pc());
    UINT8 val = read_mem(res.val);
    write_mem(res.val, doSLO(val));
    return 8;
}

// SRE
// ---

UINT8 cpu6502::doSRE(UINT8 val) {
    flags.C = val & 0x01;
    val >>= 1;
    regs.A ^= val;
    setNZflags(regs.A);
    return val;
}

UINT8 cpu6502::SRE_a() {
    MEMADDR a = fetch_addr_from_pc();
    UINT8 val = read_mem(a);
    write_mem(a, doSRE(val));
    return 6;
}

UINT8 cpu6502::SRE_d() {
    MEMADDR zp = fetch_from_pc();
    UINT8 val = read_mem(zp);
    write_mem(zp, doSRE(val));
    return 5;
}

UINT8 cpu6502::SRE_dx() {
    MEMADDR a = addr_zp_x(fetch_from_pc());
    UINT8 val = read_mem(a);
    write_mem(a, doSRE(val));
    return 6;
}

UINT8 cpu6502::SRE_ax() {
    struct addrmem_res res;
    res = addr_absolute_x(fetch_addr_from_pc());
    UINT8 val = read_mem(res.val);
    write_mem(res.val, doSRE(val));
    return 7;
}

UINT8 cpu6502::SRE_ay() {
    struct addrmem_res res;
    res = addr_absolute_y(fetch_addr_from_pc());
    UINT8 val = read_mem(res.val);
    write_mem(res.val, doSRE(val));
    return 7;
}

UINT8 cpu6502::SRE_xb() {
    MEMADDR a = addr_x_indirect(fetch_from_pc());
    UINT8 val = read_mem(a);
    write_mem(a, doSRE(val));
    return 8;
}

UINT8 cpu6502::SRE_by() {
    struct addrmem_res res;
    res = addr_indirect_y(fetch_from_pc());
    UINT8 val = read_mem(res.val);
    write_mem(res.val, doSRE(val));
    return 8;
}

// RRA
// ---

UINT8 cpu6502::doRRA(UINT8 val) {
    BOOL c = flags.C;
    flags.C = val & 0x01;
    val >>= 1;
    if(c) val |= 0x80;
    regs.A = doADC(val);
    setNZflags(regs.A);
    return val;
}

UINT8 cpu6502::RRA_a() {
    MEMADDR a = fetch_addr_from_pc();
    UINT8 val = read_mem(a);
    write_mem(a, doRRA(val));
    return 6;
}

UINT8 cpu6502::RRA_d() {
    MEMADDR zp = fetch_from_pc();
    UINT8 val = read_mem(zp);
    write_mem(zp, doRRA(val));
    return 5;
}

UINT8 cpu6502::RRA_dx() {
    MEMADDR a = addr_zp_x(fetch_from_pc());
    UINT8 val = read_mem(a);
    write_mem(a, doRRA(val));
    return 6;
}

UINT8 cpu6502::RRA_ax() {
    struct addrmem_res res;
    res = addr_absolute_x(fetch_addr_from_pc());
    UINT8 val = read_mem(res.val);
    write_mem(res.val, doRRA(val));
    return 7;
}

UINT8 cpu6502::RRA_ay() {
    struct addrmem_res res;
    res = addr_absolute_y(fetch_addr_from_pc());
    UINT8 val = read_mem(res.val);
    write_mem(res.val, doRRA(val));
    return 7;
}

UINT8 cpu6502::RRA_xb() {
    MEMADDR a = addr_x_indirect(fetch_from_pc());
    UINT8 val = read_mem(a);
    write_mem(a, doRRA(val));
    return 8;
}

UINT8 cpu6502::RRA_by() {
    struct addrmem_res res;
    res = addr_indirect_y(fetch_from_pc());
    UINT8 val = read_mem(res.val);
    write_mem(res.val, doRRA(val));
    return 8;
}   

// RLA
// ---

UINT8 cpu6502::doRLA(UINT8 val) {
    BOOL c = flags.C;
    flags.C = val & 0x80;
    val <<= 1;
    if(c) val |= 0x01;
    regs.A &= val;
    setNZflags(regs.A);
    return val;
}

UINT8 cpu6502::RLA_a() {
    MEMADDR a = fetch_addr_from_pc();
    UINT8 val = read_mem(a);
    write_mem(a, doRLA(val));
    return 6;
}

UINT8 cpu6502::RLA_d() {
    MEMADDR zp = fetch_from_pc();
    UINT8 val = read_mem(zp);
    write_mem(zp, doRLA(val));
    return 5;
}

UINT8 cpu6502::RLA_dx() {
    MEMADDR a = addr_zp_x(fetch_from_pc());
    UINT8 val = read_mem(a);
    write_mem(a, doRLA(val));
    return 6;
}

UINT8 cpu6502::RLA_ax() {
    struct addrmem_res res;
    res = addr_absolute_x(fetch_addr_from_pc());
    UINT8 val = read_mem(res.val);
    write_mem(res.val, doRLA(val));
    return 7;
}

UINT8 cpu6502::RLA_ay() {
    struct addrmem_res res;
    res = addr_absolute_y(fetch_addr_from_pc());
    UINT8 val = read_mem(res.val);
    write_mem(res.val, doRLA(val));
    return 7;
}

UINT8 cpu6502::RLA_xb() {
    MEMADDR a = addr_x_indirect(fetch_from_pc());
    UINT8 val = read_mem(a);
    write_mem(a, doRLA(val));
    return 8;
}

UINT8 cpu6502::RLA_by() {
    struct addrmem_res res;
    res = addr_indirect_y(fetch_from_pc());
    UINT8 val = read_mem(res.val);
    write_mem(res.val, doRLA(val));
    return 8;
}   

// AXS
// ---

// for this one I've read contradictory sources about how it affects flags,
// so I decided that it wouldn't affect any

UINT8 cpu6502::SAX_d() {
    MEMADDR a = fetch_from_pc();
    write_mem(a, regs.A & regs.X);
    return 3;
}

UINT8 cpu6502::SAX_dy() {
    MEMADDR a = addr_zp_y(fetch_from_pc());
    write_mem(a, regs.A & regs.X);
    return 4;
}

UINT8 cpu6502::SAX_a() {
    MEMADDR a = fetch_addr_from_pc();
    write_mem(a, regs.A & regs.X);
    return 4;
}

UINT8 cpu6502::SAX_xb() {
    MEMADDR a = addr_x_indirect(fetch_from_pc());
    write_mem(a, regs.A & regs.X);
    return 6;
}

// LAX
// ---

UINT8 cpu6502::LAX_i() {
    // different from LAXs below
    // ... also contradictory sources for this one
    regs.A |= 0xEE; // some sources tell me to do this, some others don't, plz help
    regs.A &= fetch_from_pc();
    regs.X = regs.A;
    setNZflags(regs.A);
    return 2;
}

UINT8 cpu6502::LAX_d() {
    MEMADDR a = fetch_from_pc();
    regs.A = regs.X = read_mem(a);
    setNZflags(regs.A);
    return 3;
}

UINT8 cpu6502::LAX_dy() {
    MEMADDR a = addr_zp_y(fetch_from_pc());
    regs.A = regs.X = read_mem(a);
    setNZflags(regs.A);
    return 4;
}

UINT8 cpu6502::LAX_a() {
    regs.A = regs.X = read_mem(fetch_addr_from_pc());
    setNZflags(regs.A);
    return 4;
}

UINT8 cpu6502::LAX_ay() {
    struct addrmem_res res = addr_absolute_y(fetch_addr_from_pc());
    regs.A = regs.X = read_mem(res.val);
    setNZflags(regs.A);
    return 4+res.nr_cycles;
}

UINT8 cpu6502::LAX_xb() {
    MEMADDR a = addr_x_indirect(fetch_from_pc());
    regs.A = regs.X = read_mem(a);
    setNZflags(regs.A);
    return 6;
}

UINT8 cpu6502::LAX_by() {
    struct addrmem_res res = addr_indirect_y(fetch_from_pc());
    regs.A = regs.X = read_mem(res.val);
    setNZflags(regs.A);
    return 5+res.nr_cycles;
}

// DCM
// ---

// not very used, so I didn't really optimize them
// todo : optimize it a little !

UINT8 cpu6502::DCP_d() {
    DEC_d();
    regs.PC--;
    CMP_d();
    return 5;
}

UINT8 cpu6502::DCP_dx() {
    DEC_dx();
    regs.PC--;
    CMP_dx();
    return 6;
}

UINT8 cpu6502::DCP_a() {
    DEC_a();
    regs.PC -= 2;
    CMP_a();
    return 6;
}

UINT8 cpu6502::DCP_ax() {
    DEC_ax();
    regs.PC -= 2;
    CMP_ax();
    return 7;
}

UINT8 cpu6502::DCP_ay() {
    struct addrmem_res res = addr_absolute_y(fetch_addr_from_pc());
    write_mem(res.val, read_mem(res.val) - 1);
    regs.PC-=2;
    CMP_ay();
    return 7;
}

UINT8 cpu6502::DCP_xb() {
    MEMADDR a = addr_x_indirect(fetch_from_pc());
    write_mem(a, read_mem(a) - 1);
    regs.PC--;
    CMP_xb();
    return 8;
}

UINT8 cpu6502::DCP_by() {
    struct addrmem_res res = addr_indirect_y(fetch_from_pc());
    write_mem(res.val, read_mem(res.val) - 1);
    regs.PC--;
    CMP_by();
    return 8;
}

// ISC
// ---

UINT8 cpu6502::ISC_d() {
    INC_d();
    regs.PC--;
    SBC_d();
    return 5;
}

UINT8 cpu6502::ISC_dx() {
    INC_dx();
    regs.PC--;
    SBC_dx();
    return 6;
}

UINT8 cpu6502::ISC_a() {
    INC_a();
    regs.PC-=2;
    SBC_a();
    return 6;
}

UINT8 cpu6502::ISC_ax() {
    INC_ax();
    regs.PC-=2;
    SBC_ax();
    return 7;
}

UINT8 cpu6502::ISC_ay() {
    INC_ay();
    regs.PC-=2;
    SBC_ax();
    return 7;
}

UINT8 cpu6502::ISC_xb() {
    MEMADDR a = addr_x_indirect(fetch_from_pc());
    write_mem(a, read_mem(a) + 1);
    regs.PC--;
    SBC_xb();
    return 8;
}

UINT8 cpu6502::ISC_by() {
    struct addrmem_res res = addr_indirect_y(fetch_from_pc());
    write_mem(res.val, read_mem(res.val) + 1);
    regs.PC--;
    SBC_by();
    return 8;
}

// ALR, ARR
// ---

UINT8 cpu6502::ALR_i() {
    UINT8 val = fetch_from_pc();
    regs.A &= val;
    LSR();
    return 2;
}

UINT8 cpu6502::ARR_i() {
    UINT8 val = fetch_from_pc();
    regs.A &= val;
    ROR();
    return 2;
}

// SAX
// ---

UINT8 cpu6502::AXS_i() {
    UINT8 a = regs.A;
    regs.A &= regs.X;
    SEC();
    SBC_i();
    regs.X = regs.A;
    regs.A = a;
    return 2;
}

// ANC
// ---

UINT8 cpu6502::AAC_i() {
    AND_i();
    flags.C = flags.N;
    return 2;
}

// XAA
// ---

UINT8 cpu6502::XAA_i() {
    regs.A = regs.X;
    AND_i();
    return 2;
}

// SHY, SHX

UINT8 cpu6502::SHY_ax() {
    MEMADDR a = fetch_addr_from_pc();
    struct addrmem_res res = addr_absolute_x(a);
    regs.Y &= ((UINT8)(a >> 8)) + 1;
    write_mem(res.val, regs.Y);
    return 5;
}

UINT8 cpu6502::SHX_ay() {
    MEMADDR a = fetch_addr_from_pc();
    struct addrmem_res res = addr_absolute_y(a);
    regs.X &= ((UINT8)(a >> 8)) + 1;
    write_mem(res.val, regs.X);
    return 5;
}

// AHX
// ---

UINT8 cpu6502::AHX_ay() {
    MEMADDR a = fetch_addr_from_pc();
    struct addrmem_res res = addr_absolute_y(a);
    UINT8 val = regs.A;
    val &= regs.X;
    val &= ((UINT8)(a >> 8)) + 1;
    write_mem(res.val, val);
    return 5;
}

UINT8 cpu6502::AHX_by() {
    ZPADDR a = fetch_from_pc();
    struct addrmem_res res = addr_indirect_y(a);
    UINT8 val = regs.A;
    val &= regs.X;
    val &= a + 1;
    write_mem(res.val, val);
    return 6;
}

// TAS, LAS
// ---

UINT8 cpu6502::TAS_ay() {
    MEMADDR a = fetch_addr_from_pc();
    struct addrmem_res res = addr_absolute_y(a);
    UINT8 val = regs.A & regs.X;
    regs.S = val;
    val &= ((UINT8)(a>>8)) + 1;
    write_mem(res.val, val);
    return 5;
}

UINT8 cpu6502::LAS_ay() {
    struct addrmem_res res = addr_absolute_y(fetch_addr_from_pc());
    UINT8 val = read_mem(res.val);
    val &= regs.S;
    regs.A = regs.X = regs.S = val;
    setNZflags(val);
    return 4+res.nr_cycles;
}

/* =================== INTERRUPTS =================== */

void cpu6502::enter_irq(bool from_brk) {
    UINT8 high = (UINT8)(regs.PC >> 8);
    push_stack(high);
    push_stack((UINT8)regs.PC);
    UINT8 flags_b = flags_to_byte(flags, (from_brk)?1:0);
    push_stack(flags_b);
    // starts IRQ
    regs.PC = ((MEMADDR) read_mem(0xFFFF)) << 8;
    regs.PC |= read_mem(0xFFFE);
}

void cpu6502::enter_nmi() {
    UINT8 high = (UINT8)(regs.PC >> 8);
    push_stack(high);
    push_stack((UINT8)regs.PC);
    UINT8 flags_b = flags_to_byte(flags, 0);
    push_stack(flags_b);
    // starts NMI
    regs.PC = ((UINT16) read_mem(0xFFFB)) << 8;
    regs.PC |= read_mem(0xFFFA);
    flags.I = 1;
}

void cpu6502::enter_reset() {
    // we don't handle PC and P on stack for now, directly to the routine vector
    regs.PC = ((UINT16) read_mem(0xFFFD)) << 8;
    regs.PC |= read_mem(0xFFFC);
    std::printf("After reset : 0x%04X\n", regs.PC);
}



/* =============== VON NEUMANN RELATED ============== */

UINT8 cpu6502::fetch_from_pc() {
    return read_mem(regs.PC++);
}

UINT8 cpu6502::execute_op(UINT8 op) {
    UINT8 (cpu6502::*ophandler)() = handlers_ptrs[op];
    return (this->*ophandler)(); // execute the correct handler
}


UINT8 cpu6502::von_neumann_cycle() {
    UINT8 op = fetch_from_pc(); //increments PC
    return execute_op(op);
}



cpu6502::execute_cycles_res cpu6502::execute_cycles(UINT32 nr_cycles) {
    // some code duplication, sorry it is called lots of times each second so it should be optimized
    execute_cycles_res res = {0, 0};
    UINT32 nr_returned = 0;
    UINT32 nr_total = 0;

    while(nr_total <= nr_cycles) {

        MEMADDR pc = regs.PC; // to save it in case we need it

        UINT8 op = fetch_from_pc();

        try
        {
            UINT8 (cpu6502::*ophandler)() = handlers_ptrs[op];
            nr_returned = (this->*ophandler)();
        }
        catch(const PpuSync& e)
        {
            // got to sync cpu and ppu !
            skip_next_op_ppu = 1;
            regs.PC = pc; // we'll have to execute the instruction again
            res.cycles = nr_total; // before the instruction which triggers sync
            res.ppu_dirty = 1;
            return res;
        }
        
        nr_total += nr_returned;
    }

    res.cycles = nr_total;
    res.ppu_dirty = 0;
    return res;
}

cpu6502::execute_cycles_res cpu6502::execute_cycles_debug(UINT32 nr_cycles, FILE *debug_s, UINT32 cycle_min) {
    // some code duplication, sorry it is called lots of times each second so it should be optimized
    execute_cycles_res res = {0, 0};
    UINT32 nr_total = 0;
    UINT32 nr_returned = 0;
    UINT8 op;
    while(nr_total <= nr_cycles) {
        if(nr_total >= cycle_min) {
            std::fprintf(debug_s, "%04X A:%02X X:%02X Y:%02X SP:%02X P: NV--DIZC %d%d--%d%d%d%d \n", regs.PC, regs.A, regs.X, regs.Y, regs.S, 
                !!flags.N, !!flags.V, !!flags.D, !!flags.I, !!flags.V, !!flags.Z);
        }

        std::fflush(NULL);

        MEMADDR pc = regs.PC; // to save it in case we need it
        op = fetch_from_pc();

        try
        {
            UINT8 (cpu6502::*ophandler)() = handlers_ptrs[op];
            nr_returned = (this->*ophandler)();
        }
        catch(const PpuSync& e)
        {
            // got to sync cpu and ppu !
            skip_next_op_ppu = 1;
            regs.PC = pc; // we'll have to execute the instruction again
            res.cycles = nr_total; // before the instruction which triggers sync
            res.ppu_dirty = 1;
            return res;
        }
        
        nr_total += nr_returned;
    }
    res.cycles = nr_total;
    res.ppu_dirty = 0;
    return res;
}

/* ========== CONSTRUCTOR, DESTRUCTOR ============ */

int cpu6502::init_cpu(MEMADDR pc) {
    // TODO : proper initialization
    regs.A = 0; regs.X = 0; regs.Y = 0; regs.S = 0xFD;
    flags.C = 0; flags.I = 1; flags.N = 0; flags.V = 0; flags.Z = 0; flags.D = 0;
    regs.PC = pc;
    return 0;
}

cpu6502::cpu6502(CPUMemoryManager *mem_handl) : mem_handl(mem_handl), return_on_ppu_op(0), skip_next_op_ppu(0), elapsed_cycles(0) {

    for (int i = 0; i < 256; i++)
    {
        flagNtable[i] = !(i < 128);
    }

    regs.S = 0xFD;

    // ========== initialization of ophandlers

    handlers_ptrs[0x00] = &cpu6502::BRK;
    handlers_ptrs[0x01] = &cpu6502::ORA_xb;
    handlers_ptrs[0x02] = &cpu6502::KIL;
    handlers_ptrs[0x03] = &cpu6502::SLO_xb;
    handlers_ptrs[0x04] = &cpu6502::DOP_d;
    handlers_ptrs[0x05] = &cpu6502::ORA_d;
    handlers_ptrs[0x06] = &cpu6502::ASL_d;
    handlers_ptrs[0x07] = &cpu6502::SLO_d;
    handlers_ptrs[0x08] = &cpu6502::PHP;
    handlers_ptrs[0x09] = &cpu6502::ORA_i;
    handlers_ptrs[0x0A] = &cpu6502::ASL;
    handlers_ptrs[0x0B] = &cpu6502::AAC_i;
    handlers_ptrs[0x0C] = &cpu6502::TOP_a;
    handlers_ptrs[0x0D] = &cpu6502::ORA_a;
    handlers_ptrs[0x0E] = &cpu6502::ASL_a;
    handlers_ptrs[0x0F] = &cpu6502::SLO_a;
    handlers_ptrs[0x10] = &cpu6502::BPL;
    handlers_ptrs[0x11] = &cpu6502::ORA_by;
    handlers_ptrs[0x12] = &cpu6502::KIL;
    handlers_ptrs[0x13] = &cpu6502::SLO_by;
    handlers_ptrs[0x14] = &cpu6502::NOP_dx;
    handlers_ptrs[0x15] = &cpu6502::ORA_dx;
    handlers_ptrs[0x16] = &cpu6502::ASL_dx;
    handlers_ptrs[0x17] = &cpu6502::SLO_dx;
    handlers_ptrs[0x18] = &cpu6502::CLC;
    handlers_ptrs[0x19] = &cpu6502::ORA_ay;
    handlers_ptrs[0x1A] = &cpu6502::NOP;
    handlers_ptrs[0x1B] = &cpu6502::SLO_ay;
    handlers_ptrs[0x1C] = &cpu6502::NOP_ax;
    handlers_ptrs[0x1D] = &cpu6502::ORA_ax;
    handlers_ptrs[0x1E] = &cpu6502::ASL_ax;
    handlers_ptrs[0x1F] = &cpu6502::SLO_ax;

    handlers_ptrs[0x20] = &cpu6502::JSR_a;
    handlers_ptrs[0x21] = &cpu6502::AND_xb;
    handlers_ptrs[0x22] = &cpu6502::KIL;
    handlers_ptrs[0x23] = &cpu6502::RLA_xb;
    handlers_ptrs[0x24] = &cpu6502::BIT_d;
    handlers_ptrs[0x25] = &cpu6502::AND_d;
    handlers_ptrs[0x26] = &cpu6502::ROL_d;
    handlers_ptrs[0x27] = &cpu6502::RLA_d;
    handlers_ptrs[0x28] = &cpu6502::PLP;
    handlers_ptrs[0x29] = &cpu6502::AND_i;
    handlers_ptrs[0x2A] = &cpu6502::ROL;
    handlers_ptrs[0x2B] = &cpu6502::AAC_i;
    handlers_ptrs[0x2C] = &cpu6502::BIT_a;
    handlers_ptrs[0x2D] = &cpu6502::AND_a;
    handlers_ptrs[0x2E] = &cpu6502::ROL_a;
    handlers_ptrs[0x2F] = &cpu6502::RLA_a;
    handlers_ptrs[0x30] = &cpu6502::BMI;
    handlers_ptrs[0x31] = &cpu6502::AND_by;
    handlers_ptrs[0x32] = &cpu6502::KIL;
    handlers_ptrs[0x33] = &cpu6502::RLA_by;
    handlers_ptrs[0x34] = &cpu6502::NOP_dx;
    handlers_ptrs[0x35] = &cpu6502::AND_dx;
    handlers_ptrs[0x36] = &cpu6502::ROL_dx;
    handlers_ptrs[0x37] = &cpu6502::RLA_dx;
    handlers_ptrs[0x38] = &cpu6502::SEC;
    handlers_ptrs[0x39] = &cpu6502::AND_ay;
    handlers_ptrs[0x3A] = &cpu6502::NOP;
    handlers_ptrs[0x3B] = &cpu6502::RLA_ay;
    handlers_ptrs[0x3C] = &cpu6502::NOP_ax;
    handlers_ptrs[0x3D] = &cpu6502::AND_ax;
    handlers_ptrs[0x3E] = &cpu6502::ROL_ax;
    handlers_ptrs[0x3F] = &cpu6502::RLA_ax;

    handlers_ptrs[0x40] = &cpu6502::RTI;
    handlers_ptrs[0x41] = &cpu6502::EOR_xb;
    handlers_ptrs[0x42] = &cpu6502::KIL;
    handlers_ptrs[0x43] = &cpu6502::SRE_xb;
    handlers_ptrs[0x44] = &cpu6502::DOP_d;
    handlers_ptrs[0x45] = &cpu6502::EOR_d;
    handlers_ptrs[0x46] = &cpu6502::LSR_d;
    handlers_ptrs[0x47] = &cpu6502::SRE_d;
    handlers_ptrs[0x48] = &cpu6502::PHA;
    handlers_ptrs[0x49] = &cpu6502::EOR_i;
    handlers_ptrs[0x4A] = &cpu6502::LSR;
    handlers_ptrs[0x4B] = &cpu6502::ALR_i;
    handlers_ptrs[0x4C] = &cpu6502::JMP_a;
    handlers_ptrs[0x4D] = &cpu6502::EOR_a;
    handlers_ptrs[0x4E] = &cpu6502::LSR_a;
    handlers_ptrs[0x4F] = &cpu6502::SRE_a;
    handlers_ptrs[0x50] = &cpu6502::BVC;
    handlers_ptrs[0x51] = &cpu6502::EOR_by;
    handlers_ptrs[0x52] = &cpu6502::KIL;
    handlers_ptrs[0x53] = &cpu6502::SRE_by;
    handlers_ptrs[0x54] = &cpu6502::NOP_dx;
    handlers_ptrs[0x55] = &cpu6502::EOR_dx;
    handlers_ptrs[0x56] = &cpu6502::LSR_dx;
    handlers_ptrs[0x57] = &cpu6502::SRE_dx;
    handlers_ptrs[0x58] = &cpu6502::CLI;
    handlers_ptrs[0x59] = &cpu6502::EOR_ay;
    handlers_ptrs[0x5A] = &cpu6502::NOP;
    handlers_ptrs[0x5B] = &cpu6502::SRE_ay;
    handlers_ptrs[0x5C] = &cpu6502::NOP_ax;
    handlers_ptrs[0x5D] = &cpu6502::EOR_ax;
    handlers_ptrs[0x5E] = &cpu6502::LSR_ax;
    handlers_ptrs[0x5F] = &cpu6502::SRE_ax;

    handlers_ptrs[0x60] = &cpu6502::RTS;
    handlers_ptrs[0x61] = &cpu6502::ADC_xb;
    handlers_ptrs[0x62] = &cpu6502::KIL;
    handlers_ptrs[0x63] = &cpu6502::RRA_xb;
    handlers_ptrs[0x64] = &cpu6502::DOP_d;
    handlers_ptrs[0x65] = &cpu6502::ADC_d;
    handlers_ptrs[0x66] = &cpu6502::ROR_d;
    handlers_ptrs[0x67] = &cpu6502::RRA_d;
    handlers_ptrs[0x68] = &cpu6502::PLA;
    handlers_ptrs[0x69] = &cpu6502::ADC_i;
    handlers_ptrs[0x6A] = &cpu6502::ROR;
    handlers_ptrs[0x6B] = &cpu6502::ARR_i;
    handlers_ptrs[0x6C] = &cpu6502::JMP_ab;
    handlers_ptrs[0x6D] = &cpu6502::ADC_a;
    handlers_ptrs[0x6E] = &cpu6502::ROR_a;
    handlers_ptrs[0x6F] = &cpu6502::RRA_a;
    handlers_ptrs[0x70] = &cpu6502::BVS;
    handlers_ptrs[0x71] = &cpu6502::ADC_by;
    handlers_ptrs[0x72] = &cpu6502::KIL;
    handlers_ptrs[0x73] = &cpu6502::RRA_by;
    handlers_ptrs[0x74] = &cpu6502::NOP_dx;
    handlers_ptrs[0x75] = &cpu6502::ADC_dx;
    handlers_ptrs[0x76] = &cpu6502::ROR_dx;
    handlers_ptrs[0x77] = &cpu6502::RRA_dx;
    handlers_ptrs[0x78] = &cpu6502::SEI;
    handlers_ptrs[0x79] = &cpu6502::ADC_ay;
    handlers_ptrs[0x7A] = &cpu6502::NOP;
    handlers_ptrs[0x7B] = &cpu6502::RRA_ay;
    handlers_ptrs[0x7C] = &cpu6502::NOP_ax;
    handlers_ptrs[0x7D] = &cpu6502::ADC_ax;
    handlers_ptrs[0x7E] = &cpu6502::ROR_ax;
    handlers_ptrs[0x7F] = &cpu6502::RRA_ax;

    handlers_ptrs[0x80] = &cpu6502::DOP_i;
    handlers_ptrs[0x81] = &cpu6502::STA_xb;
    handlers_ptrs[0x82] = &cpu6502::DOP_i;
    handlers_ptrs[0x83] = &cpu6502::SAX_xb;
    handlers_ptrs[0x84] = &cpu6502::STY_d;
    handlers_ptrs[0x85] = &cpu6502::STA_d;
    handlers_ptrs[0x86] = &cpu6502::STX_d;
    handlers_ptrs[0x87] = &cpu6502::SAX_d;
    handlers_ptrs[0x88] = &cpu6502::DEY;
    handlers_ptrs[0x89] = &cpu6502::DOP_i;
    handlers_ptrs[0x8A] = &cpu6502::TXA;
    handlers_ptrs[0x8B] = &cpu6502::XAA_i;
    handlers_ptrs[0x8C] = &cpu6502::STY_a;
    handlers_ptrs[0x8D] = &cpu6502::STA_a;
    handlers_ptrs[0x8E] = &cpu6502::STX_a;
    handlers_ptrs[0x8F] = &cpu6502::SAX_a;
    handlers_ptrs[0x90] = &cpu6502::BCC;
    handlers_ptrs[0x91] = &cpu6502::STA_by;
    handlers_ptrs[0x92] = &cpu6502::KIL;
    handlers_ptrs[0x93] = &cpu6502::AHX_by;
    handlers_ptrs[0x94] = &cpu6502::STY_dx;
    handlers_ptrs[0x95] = &cpu6502::STA_dx;
    handlers_ptrs[0x96] = &cpu6502::STX_dy;
    handlers_ptrs[0x97] = &cpu6502::SAX_dy;
    handlers_ptrs[0x98] = &cpu6502::TYA;
    handlers_ptrs[0x99] = &cpu6502::STA_ay;
    handlers_ptrs[0x9A] = &cpu6502::TXS;
    handlers_ptrs[0x9B] = &cpu6502::TAS_ay;
    handlers_ptrs[0x9C] = &cpu6502::SHY_ax;
    handlers_ptrs[0x9D] = &cpu6502::STA_ax;
    handlers_ptrs[0x9E] = &cpu6502::SHX_ay;
    handlers_ptrs[0x9F] = &cpu6502::AHX_ay;

    handlers_ptrs[0xA0] = &cpu6502::LDY_i;
    handlers_ptrs[0xA1] = &cpu6502::LDA_xb;
    handlers_ptrs[0xA2] = &cpu6502::LDX_i;
    handlers_ptrs[0xA3] = &cpu6502::LAX_xb;
    handlers_ptrs[0xA4] = &cpu6502::LDY_d;
    handlers_ptrs[0xA5] = &cpu6502::LDA_d;
    handlers_ptrs[0xA6] = &cpu6502::LDX_d;
    handlers_ptrs[0xA7] = &cpu6502::LAX_d;
    handlers_ptrs[0xA8] = &cpu6502::TAY;
    handlers_ptrs[0xA9] = &cpu6502::LDA_i;
    handlers_ptrs[0xAA] = &cpu6502::TAX;
    handlers_ptrs[0xAB] = &cpu6502::LAX_i;
    handlers_ptrs[0xAC] = &cpu6502::LDY_a;
    handlers_ptrs[0xAD] = &cpu6502::LDA_a;
    handlers_ptrs[0xAE] = &cpu6502::LDX_a;
    handlers_ptrs[0xAF] = &cpu6502::LAX_a;
    handlers_ptrs[0xB0] = &cpu6502::BCS;
    handlers_ptrs[0xB1] = &cpu6502::LDA_by;
    handlers_ptrs[0xB2] = &cpu6502::KIL;
    handlers_ptrs[0xB3] = &cpu6502::LAX_by;
    handlers_ptrs[0xB4] = &cpu6502::LDY_dx;
    handlers_ptrs[0xB5] = &cpu6502::LDA_dx;
    handlers_ptrs[0xB6] = &cpu6502::LDX_dy;
    handlers_ptrs[0xB7] = &cpu6502::LAX_dy;
    handlers_ptrs[0xB8] = &cpu6502::CLV;
    handlers_ptrs[0xB9] = &cpu6502::LDA_ay;
    handlers_ptrs[0xBA] = &cpu6502::TSX;
    handlers_ptrs[0xBB] = &cpu6502::LAS_ay;
    handlers_ptrs[0xBC] = &cpu6502::LDY_ax;
    handlers_ptrs[0xBD] = &cpu6502::LDA_ax;
    handlers_ptrs[0xBE] = &cpu6502::LDX_ay;
    handlers_ptrs[0xBF] = &cpu6502::LAX_ay;

    handlers_ptrs[0xC0] = &cpu6502::CPY_i;
    handlers_ptrs[0xC1] = &cpu6502::CMP_xb;
    handlers_ptrs[0xC2] = &cpu6502::DOP_i;
    handlers_ptrs[0xC3] = &cpu6502::DCP_xb;
    handlers_ptrs[0xC4] = &cpu6502::CPY_d;
    handlers_ptrs[0xC5] = &cpu6502::CMP_d;
    handlers_ptrs[0xC6] = &cpu6502::DEC_d;
    handlers_ptrs[0xC7] = &cpu6502::DCP_d;
    handlers_ptrs[0xC8] = &cpu6502::INY;
    handlers_ptrs[0xC9] = &cpu6502::CMP_i;
    handlers_ptrs[0xCA] = &cpu6502::DEX;
    handlers_ptrs[0xCB] = &cpu6502::AXS_i;
    handlers_ptrs[0xCC] = &cpu6502::CPY_a;
    handlers_ptrs[0xCD] = &cpu6502::CMP_a;
    handlers_ptrs[0xCE] = &cpu6502::DEC_a;
    handlers_ptrs[0xCF] = &cpu6502::DCP_a;
    handlers_ptrs[0xD0] = &cpu6502::BNE;
    handlers_ptrs[0xD1] = &cpu6502::CMP_by;
    handlers_ptrs[0xD2] = &cpu6502::KIL;
    handlers_ptrs[0xD3] = &cpu6502::DCP_by;
    handlers_ptrs[0xD4] = &cpu6502::NOP_dx;
    handlers_ptrs[0xD5] = &cpu6502::CMP_dx;
    handlers_ptrs[0xD6] = &cpu6502::DEC_dx;
    handlers_ptrs[0xD7] = &cpu6502::DCP_dx;
    handlers_ptrs[0xD8] = &cpu6502::CLD;
    handlers_ptrs[0xD9] = &cpu6502::CMP_ay;
    handlers_ptrs[0xDA] = &cpu6502::NOP;
    handlers_ptrs[0xDB] = &cpu6502::DCP_ay;
    handlers_ptrs[0xDC] = &cpu6502::NOP_ax;
    handlers_ptrs[0xDD] = &cpu6502::CMP_ax;
    handlers_ptrs[0xDE] = &cpu6502::DEC_ax;
    handlers_ptrs[0xDF] = &cpu6502::DCP_ax;

    handlers_ptrs[0xE0] = &cpu6502::CPX_i;
    handlers_ptrs[0xE1] = &cpu6502::SBC_xb;
    handlers_ptrs[0xE2] = &cpu6502::DOP_i;
    handlers_ptrs[0xE3] = &cpu6502::ISC_xb;
    handlers_ptrs[0xE4] = &cpu6502::CPX_d;
    handlers_ptrs[0xE5] = &cpu6502::SBC_d;
    handlers_ptrs[0xE6] = &cpu6502::INC_d;
    handlers_ptrs[0xE7] = &cpu6502::ISC_d;
    handlers_ptrs[0xE8] = &cpu6502::INX;
    handlers_ptrs[0xE9] = &cpu6502::SBC_i;
    handlers_ptrs[0xEA] = &cpu6502::NOP;
    handlers_ptrs[0xEB] = &cpu6502::SBC_i;
    handlers_ptrs[0xEC] = &cpu6502::CPX_a;
    handlers_ptrs[0xED] = &cpu6502::SBC_a;
    handlers_ptrs[0xEE] = &cpu6502::INC_a;
    handlers_ptrs[0xEF] = &cpu6502::ISC_a;
    handlers_ptrs[0xF0] = &cpu6502::BEQ;
    handlers_ptrs[0xF1] = &cpu6502::SBC_by;
    handlers_ptrs[0xF2] = &cpu6502::KIL;
    handlers_ptrs[0xF3] = &cpu6502::ISC_by;
    handlers_ptrs[0xF4] = &cpu6502::NOP_dx;
    handlers_ptrs[0xF5] = &cpu6502::SBC_dx;
    handlers_ptrs[0xF6] = &cpu6502::INC_dx;
    handlers_ptrs[0xF7] = &cpu6502::ISC_dx;
    handlers_ptrs[0xF8] = &cpu6502::SED;
    handlers_ptrs[0xF9] = &cpu6502::SBC_ay;
    handlers_ptrs[0xFA] = &cpu6502::NOP;
    handlers_ptrs[0xFB] = &cpu6502::ISC_ay;
    handlers_ptrs[0xFC] = &cpu6502::NOP_ax;
    handlers_ptrs[0xFD] = &cpu6502::SBC_ax;
    handlers_ptrs[0xFE] = &cpu6502::INC_ax;
    handlers_ptrs[0xFF] = &cpu6502::ISC_ax;
}