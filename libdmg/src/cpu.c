#include <dmg/cpu.h>
#include <dmg/state.h>

static DMG_INLINE uint8_t read8(DMGState *state, uint16_t address) {
    state->cycles += 4;
    return dmg_mmu_read(state, address);
}

static DMG_INLINE uint16_t read16(DMGState *state, uint16_t address) {
    state->cycles += 8;
    return dmg_mmu_read(state, address) | (dmg_mmu_read(state, (uint16_t) (address + 1)) << 8);
}

static DMG_INLINE uint8_t read8_pc(DMGState *state) {
    uint8_t tmp = read8(state, state->cpu.pc);
    state->cpu.pc += 1;
    return tmp;
}

static DMG_INLINE uint16_t read16_pc(DMGState *state) {
    uint16_t tmp = read16(state, state->cpu.pc);
    state->cpu.pc += 2;
    return tmp;
}

static DMG_INLINE void write8(DMGState *state, uint16_t address, uint8_t byte) {
    state->cycles += 4;
    dmg_mmu_write(state, address, byte);
}

static DMG_INLINE void write16(DMGState *state, uint16_t address, uint16_t bytes) {
    state->cycles += 8;
    dmg_mmu_write(state, address, (uint8_t) (bytes & 0xFF));
    dmg_mmu_write(state, (uint16_t) (address + 1), (uint8_t) (bytes >> 8));
}

static DMG_INLINE void set_bit(uint8_t *byte, uint8_t n, bool value) {
    *byte = (uint8_t) ((*byte & ~(1 << n)) | (value << n));
}

static DMG_INLINE bool get_bit(uint8_t *byte, uint8_t n) {
    return ((*byte >> n) & 0x01) != 0x00;
}

static DMG_INLINE void set_z(uint8_t *f, bool value) {
    set_bit(f, 7, value);
}

static DMG_INLINE void set_n(uint8_t *f, bool value) {
    set_bit(f, 6, value);
}

static DMG_INLINE void set_h(uint8_t *f, bool value) {
    set_bit(f, 5, value);
}

static DMG_INLINE void set_c(uint8_t *f, bool value) {
    set_bit(f, 4, value);
}

static DMG_INLINE bool get_z(uint8_t *f) {
    return get_bit(f, 7);
}

static DMG_INLINE bool get_n(uint8_t *f) {
    return get_bit(f, 6);
}

static DMG_INLINE bool get_h(uint8_t *f) {
    return get_bit(f, 5);
}

static DMG_INLINE bool get_c(uint8_t *f) {
    return get_bit(f, 4);
}

static DMG_INLINE void inc_rr(DMGState *state, uint16_t *rr) {
    state->cycles += 4;
    *rr += 1;
}

static DMG_INLINE void inc_r(DMGState *state, uint8_t *r) {
    uint8_t *f = &state->cpu.f;
    set_h(f, (*r & 0x0F) == 0x00);
    *r += 1;
    set_z(f, *r == 0x00);
    set_n(f, false);
}

static DMG_INLINE void dec_r(DMGState *state, uint8_t *r) {
    uint8_t *f = &state->cpu.f;
    set_h(f, (*r & 0x0F) == 0x0F);
    *r -= 1;
    set_z(f, *r == 0x00);
    set_n(f, true);
}

static DMG_INLINE void add_hl_rr(DMGState *state, uint16_t *rr) {
    state->cycles += 4;
    uint8_t *f = &state->cpu.f;
    uint16_t *hl = &state->cpu.hl;
    uint32_t overflow = *hl + *rr;
    set_n(f, false);
    set_h(f, ((*hl & 0x0FFF) + (*rr & 0x0FFF)) > 0x0FFF);
    set_c(f, overflow > 0xFFFF);
    *hl = (uint16_t) (overflow & 0xFFFF);
}

static DMG_INLINE void dec_rr(DMGState *state, uint16_t *rr) {
    state->cycles += 4;
    *rr -= 1;
}

static DMG_INLINE void stop(DMGState *state) {
    state->cpu.pc += 1;
    // TODO: Not implemented
    assert(false);
}

static DMG_INLINE void jr_n(DMGState *state) {
    state->cycles += 4;
    state->cpu.pc += (int8_t) read8_pc(state);
}

static DMG_INLINE void ldi_hl_a(DMGState *state) {
    write8(state, state->cpu.hl, state->cpu.a);
    state->cpu.hl += 1;
}

static DMG_INLINE void ldi_a_hl(DMGState *state) {
    state->cpu.a = read8(state, state->cpu.hl);
    state->cpu.hl += 1;
}

static DMG_INLINE void jr_nz_n(DMGState *state) {
    if (!get_z(&state->cpu.f)) {
        jr_n(state);
    } else {
        state->cycles += 4;
        state->cpu.pc += 1;
    }
}

static DMG_INLINE void daa(DMGState *state) {
    // TODO: Unimplemented
    assert(false);
}

static DMG_INLINE void jr_z_n(DMGState *state) {
    if (get_z(&state->cpu.f)) {
        jr_n(state);
    } else {
        state->cycles += 4;
        state->cpu.pc += 1;
    }
}

static DMG_INLINE void cpl(DMGState *state) {
    uint8_t *f = &state->cpu.f;
    state->cpu.a ^= 0xFF;
    set_n(f, true);
    set_h(f, true);
}

static DMG_INLINE void jr_nc_n(DMGState *state) {
    if (!get_c(&state->cpu.f)) {
        jr_n(state);
    } else {
        state->cycles += 4;
        state->cpu.pc += 1;
    }
}

static DMG_INLINE void ldd_hl_a(DMGState *state) {
    write8(state, state->cpu.hl, state->cpu.a);
    state->cpu.hl -= 1;
}

static DMG_INLINE void inc_mem_hl(DMGState *state) {
    uint16_t *hl = &state->cpu.hl;
    uint8_t *f = &state->cpu.f;
    uint16_t overflow = read8(state, *hl);
    set_h(f, (overflow & 0x0F) == 0x00);
    overflow = (uint16_t) ((overflow + 1) & 0xFF);
    write8(state, *hl, (uint8_t) overflow);
    set_z(f, overflow == 0x00);
    set_n(f, false);
}

static DMG_INLINE void dec_mem_hl(DMGState *state) {
    uint16_t *hl = &state->cpu.hl;
    uint8_t *f = &state->cpu.f;
    uint16_t overflow = read8(state, *hl);
    set_h(f, (overflow & 0x0F) == 0x0F);
    overflow = (uint16_t) ((overflow - 1) & 0xFF);
    write8(state, *hl, (uint8_t) overflow);
    set_z(f, overflow == 0x00);
    set_n(f, true);
}

static DMG_INLINE void scf(DMGState *state) {
    uint8_t *f = &state->cpu.f;
    set_c(f, true);
    set_h(f, false);
    set_n(f, false);
}

static DMG_INLINE void jr_c_n(DMGState *state) {
    if (get_c(&state->cpu.f)) {
        jr_n(state);
    } else {
        state->cycles += 4;
        state->cpu.pc += 1;
    }
}

static DMG_INLINE void ldd_a_hl(DMGState *state) {
    state->cpu.a = read8(state, state->cpu.hl);
    state->cpu.hl -= 1;
}

static DMG_INLINE void ccf(DMGState *state) {
    uint8_t *f = &state->cpu.f;
    set_c(f, !get_c(f));
    set_h(f, false);
    set_n(f, false);
}

static DMG_INLINE void halt(DMGState *state) {
    // TODO: real interrupts
    state->cpu.halted = true;
    state->cpu.halt_flags = state->mmu.io[DMG_IO_IF];
}

static DMG_INLINE void add_r(DMGState *state, uint8_t r) {
    uint8_t *f = &state->cpu.f;
    uint8_t *a = &state->cpu.a;
    uint16_t tmp = *a + r;
    set_h(f, ((*a & 0x0F) + (r & 0x0F)) > 0x0F);
    set_c(f, tmp > *a);
    *a = (uint8_t) (tmp & 0xFF);
    set_z(f, *a == 0x00);
    set_n(f, false);
}

static DMG_INLINE void adc_r(DMGState *state, uint8_t r) {
    uint8_t *f = &state->cpu.f;
    uint8_t *a = &state->cpu.a;
    uint16_t tmp = *a + r + get_c(f);
    set_h(f, ((*a & 0x0F) + (r & 0x0F) + get_c(f)) > 0x0F);
    set_c(f, tmp > 0xFF);
    *a = (uint8_t) (tmp & 0xFF);
    set_z(f, *a == 0x00);
    set_n(f, false);
}

static DMG_INLINE void sub_r(DMGState *state, uint8_t r) {
    uint8_t *f = &state->cpu.f;
    uint8_t *a = &state->cpu.a;
    set_h(f, (*a & 0x0F) < (r & 0x0F));
    set_c(f, *a < r);
    *a -= r;
    set_z(f, *a == 0x00);
    set_n(f, true);
}

static DMG_INLINE void suba(DMGState *state) {
    assert(false);
    // TODO: This is sketchy. Logic from Gambatte
    uint8_t *f = &state->cpu.f;
    state->cpu.a = 0;
    set_z(f, true);
    set_n(f, true);
}

static DMG_INLINE void sbc_r(DMGState *state, uint8_t r) {
    uint8_t *f = &state->cpu.f;
    uint8_t *a = &state->cpu.a;
    uint16_t tmp = *a - r - get_c(f);
    set_h(f, (*a & 0x0F) < (r & 0x0F));
    set_c(f, *a < r + get_c(f));
    *a = (uint8_t) tmp;
    set_z(f, *a == 0x00);
    set_n(f, true);
}

static DMG_INLINE void and_r(DMGState *state, uint8_t r) {
    uint8_t *f = &state->cpu.f;
    uint8_t *a = &state->cpu.a;
    *a &= r;
    set_n(f, false);
    set_h(f, true);
    set_c(f, false);
    set_z(f, *a == 0x00);
}

static DMG_INLINE void xor_r(DMGState *state, uint8_t r) {
    uint8_t *f = &state->cpu.f;
    uint8_t *a = &state->cpu.a;
    *a ^= r;
    set_n(f, false);
    set_h(f, false);
    set_c(f, false);
    set_z(f, *a == 0x00);
}

static DMG_INLINE void or_r(DMGState *state, uint8_t r) {
    uint8_t *f = &state->cpu.f;
    uint8_t *a = &state->cpu.a;
    *a |= r;
    set_n(f, false);
    set_h(f, false);
    set_c(f, false);
    set_z(f, *a == 0x00);
}

static DMG_INLINE void cp_r(DMGState *state, uint8_t r) {
    uint8_t *f = &state->cpu.f;
    uint8_t *a = &state->cpu.a;
    set_h(f, (*a & 0x0F) < (r & 0x0F));
    set_c(f, *a < r);
    set_z(f, *a == r);
    set_n(f, true);
}

static DMG_INLINE uint16_t pop16(DMGState *state) {
    uint16_t tmp = read16(state, state->cpu.sp);
    state->cpu.sp += 2;
    return tmp;
}

static DMG_INLINE void ret(DMGState *state) {
    state->cycles += 4;
    state->cpu.pc = pop16(state);
}

static DMG_INLINE void ret_nz(DMGState *state) {
    state->cycles += 4;
    if (!get_z(&state->cpu.f)) {
        ret(state);
    }
}

static DMG_INLINE void jp_nn(DMGState *state) {
    state->cycles += 4;
    state->cpu.pc = read16_pc(state);
}

static DMG_INLINE void jp_nz(DMGState *state) {
    if (!get_z(&state->cpu.f)) {
        jp_nn(state);
    } else {
        state->cycles += 4;
        state->cpu.pc += 2;
    }
}

static DMG_INLINE void push16(DMGState *state, uint16_t bytes) {
    state->cpu.sp -= 2;
    write16(state, state->cpu.sp, bytes);
}

static DMG_INLINE void call_nn(DMGState *state) {
    push16(state, (uint16_t) (state->cpu.pc + 2));
    jp_nn(state);
}

static DMG_INLINE void call_nz(DMGState *state) {
    if (!get_z(&state->cpu.f)) {
        call_nn(state);
    } else {
        state->cycles += 4;
        state->cpu.pc += 2;
    }
}

static DMG_INLINE void rst(DMGState *state, uint8_t offset) {
    push16(state, state->cpu.pc);
    state->cycles += 4;
    state->cpu.pc = offset;
}

static DMG_INLINE void ret_z(DMGState *state) {
    state->cycles += 4;
    if (get_z(&state->cpu.f)) {
        ret(state);
    }
}

static DMG_INLINE void jp_z(DMGState *state) {
    if (get_z(&state->cpu.f)) {
        jp_nn(state);
    } else {
        state->cycles += 4;
        state->cpu.pc += 2;
    }
}

static DMG_INLINE void rlc_r(DMGState *state, uint8_t *r) {
    uint8_t *f = &state->cpu.f;
    uint8_t carry = *r >> 7;
    *r = (*r << 1) | carry;
    set_z(f, *r == 0x00);
    set_n(f, false);
    set_h(f, false);
    set_c(f, carry);
}

static DMG_INLINE void rrc_r(DMGState *state, uint8_t *r) {
    uint8_t *f = &state->cpu.f;
    uint8_t carry = (uint8_t) (*r & 0x01);
    *r = (*r >> 1) | (carry << 7);
    set_z(f, *r == 0x00);
    set_n(f, false);
    set_h(f, false);
    set_c(f, carry);
}

static DMG_INLINE void rlca(DMGState *state) {
    rlc_r(state, &state->cpu.a);
    set_z(&state->cpu.f, false); // This is a contentious line. Gambatte also sets Z to false in the RXXA instrs.
}

static DMG_INLINE void rrca(DMGState *state) {
    rrc_r(state, &state->cpu.a);
    set_z(&state->cpu.f, false);
}

static DMG_INLINE void rlc_mem_hl(DMGState *state) {
    uint8_t tmp = read8(state, state->cpu.hl);
    rlc_r(state, &tmp);
    write8(state, state->cpu.hl, tmp);
}

static DMG_INLINE void rrc_mem_hl(DMGState *state) {
    uint8_t tmp = read8(state, state->cpu.hl);
    rrc_r(state, &tmp);
    write8(state, state->cpu.hl, tmp);
}

static DMG_INLINE void rl_r(DMGState *state, uint8_t *r) {
    uint8_t *f = &state->cpu.f;
    uint8_t carry = *r >> 7;
    *r = (*r << 1) | get_c(f);
    set_z(f, *r == 0x00);
    set_n(f, false);
    set_h(f, false);
    set_c(f, carry);
}

static DMG_INLINE void rr_r(DMGState *state, uint8_t *r) {
    uint8_t *f = &state->cpu.f;
    uint8_t carry = (uint8_t) (*r & 0x01);
    *r = (*r >> 1) | (get_c(f) << 7);
    set_z(f, *r == 0x00);
    set_n(f, false);
    set_h(f, false);
    set_c(f, carry);
}

static DMG_INLINE void rla(DMGState *state) {
    rl_r(state, &state->cpu.a);
    set_z(&state->cpu.f, false);
}

static DMG_INLINE void rra(DMGState *state) {
    rr_r(state, &state->cpu.a);
    set_z(&state->cpu.f, false);
}

static DMG_INLINE void rl_mem_hl(DMGState *state) {
    uint8_t tmp = read8(state, state->cpu.hl);
    rl_r(state, &tmp);
    write8(state, state->cpu.hl, tmp);
}

static DMG_INLINE void rr_mem_hl(DMGState *state) {
    uint8_t tmp = read8(state, state->cpu.hl);
    rr_r(state, &tmp);
    write8(state, state->cpu.hl, tmp);
}

static DMG_INLINE void sla_r(DMGState *state, uint8_t *r) {
    uint8_t *f = &state->cpu.f;
    uint8_t carry = *r >> 7;
    *r <<= 1;
    set_z(f, *r == 0x00);
    set_n(f, false);
    set_h(f, false);
    set_c(f, carry);
}

static DMG_INLINE void sla_mem_hl(DMGState *state) {
    uint8_t tmp = read8(state, state->cpu.hl);
    sla_r(state, &tmp);
    write8(state, state->cpu.hl, tmp);
}

static DMG_INLINE void sra_r(DMGState *state, uint8_t *r) {
    uint8_t *f = &state->cpu.f;
    uint8_t carry = (uint8_t) (*r &  0x01);
    *r = (uint8_t) ((*r >> 1) | (*r & 0x80));
    set_z(f, *r == 0x00);
    set_n(f, false);
    set_h(f, false);
    set_c(f, carry);
}

static DMG_INLINE void sra_mem_hl(DMGState *state) {
    uint8_t tmp = read8(state, state->cpu.hl);
    sra_r(state, &tmp);
    write8(state, state->cpu.hl, tmp);
}

static DMG_INLINE void swap_r(DMGState *state, uint8_t *r) {
    uint8_t *f = &state->cpu.f;
    *r = (uint8_t) ((*r >> 4) | ((*r & 0xF) << 4));
    set_z(f, *r == 0x00);
    set_n(f, false);
    set_h(f, false);
    set_c(f, false);
}

static DMG_INLINE void swap_mem_hl(DMGState *state) {
    uint8_t tmp = read8(state, state->cpu.hl);
    swap_r(state, &tmp);
    write8(state, state->cpu.hl, tmp);
}

static DMG_INLINE void srl_r(DMGState *state, uint8_t *r) {
    uint8_t *f = &state->cpu.f;
    uint8_t carry = (uint8_t) (*r & 0x01);
    *r >>= 1;
    set_z(f, *r == 0x00);
    set_n(f, false);
    set_h(f, false);
    set_c(f, carry);
}

static DMG_INLINE void srl_mem_hl(DMGState *state) {
    uint8_t tmp = read8(state, state->cpu.hl);
    srl_r(state, &tmp);
    write8(state, state->cpu.hl, tmp);
}

static DMG_INLINE void bit_r(DMGState *state, uint8_t byte, uint8_t n) {
    uint8_t *f = &state->cpu.f;
    set_z(f, !get_bit(&byte, n));
    set_n(f, false);
    set_h(f, true);
}

static DMG_INLINE void set_bit_mem_hl(DMGState *state, uint8_t n, bool value) {
    uint8_t tmp = read8(state, state->cpu.hl);
    set_bit(&tmp, n, value);
    write8(state, state->cpu.hl, tmp);
}

static DMG_INLINE void call_z(DMGState *state) {
    if (get_z(&state->cpu.f)) {
        call_nn(state);
    } else {
        state->cycles += 4;
        state->cpu.pc += 2;
    }
}

static DMG_INLINE void ret_nc(DMGState *state) {
    state->cycles += 4;
    if (!get_c(&state->cpu.f)) {
        ret(state);
    }
}

static DMG_INLINE void jp_nc(DMGState *state) {
    if (!get_c(&state->cpu.f)) {
        jp_nn(state);
    } else {
        state->cycles += 4;
        state->cpu.pc += 2;
    }
}

static DMG_INLINE void call_nc(DMGState *state) {
    if (!get_c(&state->cpu.f)) {
        call_nn(state);
    } else {
        state->cycles += 4;
        state->cpu.pc += 2;
    }
}

static DMG_INLINE void ret_c(DMGState *state) {
    state->cycles += 4;
    if (get_c(&state->cpu.f)) {
        ret(state);
    }
}

static DMG_INLINE void reti(DMGState *state) {
    ret(state);
    // TODO: real interrupt handler
    state->cpu.ime = true;
}

static DMG_INLINE void jp_c(DMGState *state) {
    if (get_c(&state->cpu.f)) {
        jp_nn(state);
    } else {
        state->cycles += 4;
        state->cpu.pc += 2;
    }
}

static DMG_INLINE void call_c(DMGState *state) {
    if (get_c(&state->cpu.f)) {
        call_nn(state);
    } else {
        state->cycles += 4;
        state->cpu.pc += 2;
    }
}

static DMG_INLINE uint16_t sp_plus_n(DMGState *state) {
    // TODO: Unimplemented
    assert(false);
}

static DMG_INLINE void add_sp_n(DMGState *state) {
    state->cycles += 4;
    state->cpu.sp = sp_plus_n(state);
}

static DMG_INLINE void ldhl_sp_n(DMGState *state) {
    state->cycles += 4;
    state->cpu.hl = sp_plus_n(state);
}

static DMG_INLINE void ld_sp_hl(DMGState *state) {
    state->cycles += 4;
    state->cpu.sp = state->cpu.hl;
}

static DMG_INLINE void service_interrupts(DMGState *state) {
    DMGCpu *cpu = &state->cpu;
    DMGMmu *mmu = &state->mmu;

    // TODO: real interrupts
    if (cpu->halted) {
        state->cycles += 4;
        if (cpu->halt_flags != state->mmu.io[DMG_IO_IF]) {
            cpu->halted = false;
        }
    }
    if (!cpu->ime) {
        return;
    }
    uint8_t ints = mmu->io[DMG_IO_IF] & mmu->io[DMG_IO_IE];
    if (ints == 0x00) {
        return;
    }
    if (ints & 0x01) { // vblank
        rst(state, 0x40);
        mmu->io[DMG_IO_IF] ^= 0x01;
        cpu->ime = false;
        return;
    }
    if (ints & 0x02) { // lcd stat
        rst(state, 0x48);
        mmu->io[DMG_IO_IF] ^= 0x02;
        cpu->ime = false;
        return;
    }
    if (ints & 0x04) { // timer
        rst(state, 0x50);
        mmu->io[DMG_IO_IF] ^= 0x04;
        cpu->ime = false;
        return;
    }
    if (ints & 0x08) { // serial
        rst(state, 0x58);
        mmu->io[DMG_IO_IF] ^= 0x08;
        cpu->ime = false;
        return;
    }
    if (ints & 0x10) { // joypad
        rst(state, 0x60);
        mmu->io[DMG_IO_IF] ^= 0x10;
        cpu->ime = false;
        return;
    }
}

void dmg_cpu_run(DMGState *state, size_t cycles) {
    DMGCpu *cpu = &state->cpu;

    service_interrupts(state);

    switch (read8_pc(state)) {

        case 0x00: // NOP
            break;

        case 0x01: // LD BC, NN
            cpu->bc = read16_pc(state);
            break;

        case 0x02: // LD (BC), A
            write8(state, cpu->bc, cpu->a);
            break;

        case 0x03: // INC BC
            inc_rr(state, &cpu->bc);
            break;

        case 0x04: // INC B
            inc_r(state, &cpu->b);
            break;

        case 0x05: // DEC B
            dec_r(state, &cpu->b);
            break;

        case 0x06: // LD B, N
            cpu->b = read8_pc(state);
            break;

        case 0x07: // RLCA
            rlca(state);
            break;

        case 0x08: // LD (NN), SP
            write16(state, read16_pc(state), cpu->sp);
            break;

        case 0x09: // ADD HL, BC
            add_hl_rr(state, &cpu->bc);
            break;

        case 0x0A: // LD A, (BC)
            cpu->a = read8(state, cpu->bc);
            break;

        case 0x0B: // DEC BC
            dec_rr(state, &cpu->bc);
            break;

        case 0x0C: // INC C
            inc_r(state, &cpu->c);
            break;

        case 0x0D: // DEC C
            dec_r(state, &cpu->c);
            break;

        case 0x0E: // LD C, N
            cpu->c = read8_pc(state);
            break;

        case 0x0F: // RRCA
            rrca(state);
            break;

        case 0x10: // STOP
            stop(state);
            break;

        case 0x11: // LD DE, NN
            cpu->de = read16_pc(state);
            break;

        case 0x12: // LD (DE), A
            write8(state, cpu->de, cpu->a);
            break;

        case 0x13: // INC DE
            inc_rr(state, &cpu->de);
            break;

        case 0x14: // INC D
            inc_r(state, &cpu->d);
            break;

        case 0x15: // DEC D
            dec_r(state, &cpu->d);
            break;

        case 0x16: // LD D, N
            cpu->d = read8_pc(state);
            break;

        case 0x17: // RLA
            rla(state);
            break;

        case 0x18: // JR N
            jr_n(state);
            break;

        case 0x19: // ADD HL, DE
            add_hl_rr(state, &cpu->de);
            break;

        case 0x1A: // LD A, (DE)
            cpu->a = read8(state, cpu->de);
            break;

        case 0x1B: // DEC DE
            dec_rr(state, &cpu->de);
            break;

        case 0x1C: // INC E
            inc_r(state, &cpu->e);
            break;

        case 0x1D: // DEC E
            dec_r(state, &cpu->e);
            break;

        case 0x1E: // LD E, N
            cpu->e = read8_pc(state);
            break;

        case 0x1F: // RRA
            rra(state);
            break;

        case 0x20: // JR NZ, N
            jr_nz_n(state);
            break;

        case 0x21: // LD HL, NN
            cpu->hl = read16_pc(state);
            break;

        case 0x22: // LDI (HL), A
            ldi_hl_a(state);
            break;

        case 0x23: // INC HL
            inc_rr(state, &cpu->hl);
            break;

        case 0x24: // INC H
            inc_r(state, &cpu->h);
            break;

        case 0x25: // DEC H
            dec_r(state, &cpu->h);
            break;

        case 0x26: // LD H, N
            cpu->h = read8_pc(state);
            break;

        case 0x27: // DAA
            daa(state);
            break;

        case 0x28: // JR Z, N
            jr_z_n(state);
            break;

        case 0x29: // ADD HL, HL
            add_hl_rr(state, &cpu->hl);
            break;

        case 0x2A: // LDI A, (HL)
            ldi_a_hl(state);
            break;

        case 0x2B: // DEC HL
            dec_rr(state, &cpu->hl);
            break;

        case 0x2C: // INC L
            inc_r(state, &cpu->l);
            break;

        case 0x2D: // DEC L
            dec_r(state, &cpu->l);
            break;

        case 0x2E: // LD L, N
            cpu->l = read8_pc(state);
            break;

        case 0x2F: // CPL
            cpl(state);
            break;

        case 0x30: // JR NC, N
            jr_nc_n(state);
            break;

        case 0x31: // LD SP, NN
            cpu->sp = read16_pc(state);
            break;

        case 0x32: // LDD (HL), A
            ldd_hl_a(state);
            break;

        case 0x33: // INC SP
            inc_rr(state, &cpu->sp);
            break;

        case 0x34: // INC (HL)
            inc_mem_hl(state);
            break;

        case 0x35: // DEC (HL)
            dec_mem_hl(state);
            break;

        case 0x36: // LD (HL), N
            write8(state, cpu->hl, read8_pc(state));
            break;

        case 0x37: // SCF
            scf(state);
            break;

        case 0x38: // JR C, N
            jr_c_n(state);
            break;

        case 0x39: // ADD HL, SP
            add_hl_rr(state, &cpu->sp);
            break;

        case 0x3A: // LDD A, (HL)
            ldd_a_hl(state);
            break;

        case 0x3B: // DEC SP
            dec_rr(state, &cpu->sp);
            break;

        case 0x3C: // INC A
            inc_r(state, &cpu->a);
            break;

        case 0x3D: // DEC A
            dec_r(state, &cpu->a);
            break;

        case 0x3E: // LD A, N
            cpu->a = read8_pc(state);
            break;

        case 0x3F: // CCF
            ccf(state);
            break;

        case 0x40: // LD B, B
            cpu->b = cpu->b;
            break;

        case 0x41: // LD B, C
            cpu->b = cpu->c;
            break;

        case 0x42: // LD B, D
            cpu->b = cpu->d;
            break;

        case 0x43: // LD B, E
            cpu->b = cpu->e;
            break;

        case 0x44: // LD B, H
            cpu->b = cpu->h;
            break;

        case 0x45: // LD B, L
            cpu->b = cpu->l;
            break;

        case 0x46: // LD B, (HL)
            cpu->b = read8(state, cpu->hl);
            break;

        case 0x47: // LD B, A
            cpu->b = cpu->a;
            break;

        case 0x48: // LD C, B
            cpu->c = cpu->b;
            break;

        case 0x49: // LD C, C
            cpu->c = cpu->c;
            break;

        case 0x4A: // LD C, D
            cpu->c = cpu->d;
            break;

        case 0x4B: // LD C, E
            cpu->c = cpu->e;
            break;

        case 0x4C: // LD C, H
            cpu->c = cpu->h;
            break;

        case 0x4D: // LD C, L
            cpu->c = cpu->l;
            break;

        case 0x4E: // LD C, (HL)
            cpu->c = read8(state, cpu->hl);
            break;

        case 0x4F: // LD C, A
            cpu->c = cpu->a;
            break;

        case 0x50: // LD D, B
            cpu->d = cpu->b;
            break;

        case 0x51: // LD D, C
            cpu->d = cpu->c;
            break;

        case 0x52: // LD D, D
            cpu->d = cpu->d;
            break;

        case 0x53: // LD D, E
            cpu->d = cpu->e;
            break;

        case 0x54: // LD D, H
            cpu->d = cpu->h;
            break;

        case 0x55: // LD D, L
            cpu->d = cpu->l;
            break;

        case 0x56: // LD D, (HL)
            cpu->d = read8(state, cpu->hl);
            break;

        case 0x57: // LD D, A
            cpu->d = cpu->a;
            break;

        case 0x58: // LD E, B
            cpu->e = cpu->b;
            break;

        case 0x59: // LD E, C
            cpu->e = cpu->c;
            break;

        case 0x5A: // LD E, D
            cpu->e = cpu->d;
            break;

        case 0x5B: // LD E, E
            cpu->e = cpu->e;
            break;

        case 0x5C: // LD E, H
            cpu->e = cpu->h;
            break;

        case 0x5D: // LD E, L
            cpu->e = cpu->l;
            break;

        case 0x5E: // LD E, (HL)
            cpu->e = read8(state, cpu->hl);
            break;

        case 0x5F: // LD E, A
            cpu->e = cpu->a;
            break;

        case 0x60: // LD H, B
            cpu->h = cpu->b;
            break;

        case 0x61: // LD H, C
            cpu->h = cpu->c;
            break;

        case 0x62: // LD H, D
            cpu->h = cpu->d;
            break;

        case 0x63: // LD H, E
            cpu->h = cpu->e;
            break;

        case 0x64: // LD H, H
            cpu->h = cpu->h;
            break;

        case 0x65: // LD H, L
            cpu->h = cpu->l;
            break;

        case 0x66: // LD H, (HL)
            cpu->h = read8(state, cpu->hl);
            break;

        case 0x67: // LD H, A
            cpu->h = cpu->a;
            break;

        case 0x68: // LD L, B
            cpu->l = cpu->b;
            break;

        case 0x69: // LD L, C
            cpu->l = cpu->c;
            break;

        case 0x6A: // LD L, D
            cpu->l = cpu->d;
            break;

        case 0x6B: // LD L, E
            cpu->l = cpu->e;
            break;

        case 0x6C: // LD L, H
            cpu->l = cpu->h;
            break;

        case 0x6D: // LD L, L
            cpu->l = cpu->l;
            break;

        case 0x6E: // LD L, (HL)
            cpu->l = read8(state, cpu->hl);
            break;

        case 0x6F: // LD L, A
            cpu->l = cpu->a;
            break;

        case 0x70: // LD (HL), B
            write8(state, cpu->hl, cpu->b);
            break;

        case 0x71: // LD (HL), C
            write8(state, cpu->hl, cpu->c);
            break;

        case 0x72: // LD (HL), D
            write8(state, cpu->hl, cpu->d);
            break;

        case 0x73: // LD (HL), E
            write8(state, cpu->hl, cpu->e);
            break;

        case 0x74: // LD (HL), H
            write8(state, cpu->hl, cpu->h);
            break;

        case 0x75: // LD (HL), L
            write8(state, cpu->hl, cpu->l);
            break;

        case 0x76: // HALT
            halt(state);
            break;

        case 0x77: // LD (HL), A
            write8(state, cpu->hl, cpu->a);
            break;

        case 0x78: // LD A, B
            cpu->a = cpu->b;
            break;

        case 0x79: // LD A, C
            cpu->a = cpu->c;
            break;

        case 0x7A: // LD A, D
            cpu->a = cpu->d;
            break;

        case 0x7B: // LD A, E
            cpu->a = cpu->e;
            break;

        case 0x7C: // LD A, H
            cpu->a = cpu->h;
            break;

        case 0x7D: // LD A, L
            cpu->a = cpu->l;
            break;

        case 0x7E: // LD A, (HL)
            cpu->a = read8(state, cpu->hl);
            break;

        case 0x7F: // LD A, A
            cpu->a = cpu->a;
            break;

        case 0x80: // ADD B
            add_r(state, cpu->b);
            break;

        case 0x81: // ADD C
            add_r(state, cpu->c);
            break;

        case 0x82: // ADD D
            add_r(state, cpu->d);
            break;

        case 0x83: // ADD E
            add_r(state, cpu->e);
            break;

        case 0x84: // ADD H
            add_r(state, cpu->h);
            break;

        case 0x85: // ADD L
            add_r(state, cpu->l);
            break;

        case 0x86: // ADD (HL)
            add_r(state, read8(state, cpu->hl));
            break;

        case 0x87: // ADD A
            add_r(state, cpu->a);
            break;

        case 0x88: // ADC B
            adc_r(state, cpu->b);
            break;

        case 0x89: // ADC C
            adc_r(state, cpu->c);
            break;

        case 0x8A: // ADC D
            adc_r(state, cpu->c);
            break;

        case 0x8B: // ADC E
            adc_r(state, cpu->e);
            break;

        case 0x8C: // ADC H
            adc_r(state, cpu->h);
            break;

        case 0x8D: // ADC L
            adc_r(state, cpu->l);
            break;

        case 0x8E: // ADC (HL)
            adc_r(state, read8(state, cpu->hl));
            break;

        case 0x8F: // ADC A
            adc_r(state, cpu->a);
            break;

        case 0x90: // SUB B
            sub_r(state, cpu->b);
            break;

        case 0x91: // SUB C
            sub_r(state, cpu->c);
            break;

        case 0x92: // SUB D
            sub_r(state, cpu->d);
            break;

        case 0x93: // SUB E
            sub_r(state, cpu->e);
            break;

        case 0x94: // SUB H
            sub_r(state, cpu->h);
            break;

        case 0x95: // SUB L
            sub_r(state, cpu->l);
            break;

        case 0x96: // SUB (HL)
            sub_r(state, read8(state, cpu->hl));
            break;

        case 0x97: // SUB A
            suba(state);
            break;

        case 0x98: // SBC B
            sbc_r(state, cpu->b);
            break;

        case 0x99: // SBC C
            sbc_r(state, cpu->c);
            break;

        case 0x9A: // SBC D
            sbc_r(state, cpu->d);
            break;

        case 0x9B: // SBC E
            sbc_r(state, cpu->e);
            break;

        case 0x9C: // SBC H
            sbc_r(state, cpu->h);
            break;

        case 0x9D: // SBC L
            sbc_r(state, cpu->l);
            break;

        case 0x9E: // SBC (HL)
            sbc_r(state, read8(state, cpu->hl));
            break;

        case 0x9F: // SBC A
            sbc_r(state, cpu->a);
            break;

        case 0xA0: // AND B
            and_r(state, cpu->b);
            break;

        case 0xA1: // AND C
            and_r(state, cpu->c);
            break;

        case 0xA2: // AND D
            and_r(state, cpu->d);
            break;

        case 0xA3: // AND E
            and_r(state, cpu->e);
            break;

        case 0xA4: // AND H
            and_r(state, cpu->h);
            break;

        case 0xA5: // AND L
            and_r(state, cpu->l);
            break;

        case 0xA6: // AND (HL)
            and_r(state, read8(state, cpu->hl));
            break;

        case 0xA7: // AND A
            and_r(state, cpu->a);
            break;

        case 0xA8: // XOR B
            xor_r(state, cpu->b);
            break;

        case 0xA9: // XOR C
            xor_r(state, cpu->c);
            break;

        case 0xAA: // XOR D
            xor_r(state, cpu->d);
            break;

        case 0xAB: // XOR E
            xor_r(state, cpu->e);
            break;

        case 0xAC: // XOR H
            xor_r(state, cpu->h);
            break;

        case 0xAD: // XOR L
            xor_r(state, cpu->l);
            break;

        case 0xAE: // XOR (HL)
            xor_r(state, read8(state, cpu->hl));
            break;

        case 0xAF: // XOR A
            xor_r(state, cpu->a);
            break;

        case 0xB0: // OR B
            or_r(state, cpu->b);
            break;

        case 0xB1: // OR C
            or_r(state, cpu->c);
            break;

        case 0xB2: // OR D
            or_r(state, cpu->d);
            break;

        case 0xB3: // OR E
            or_r(state, cpu->e);
            break;

        case 0xB4: // OR H
            or_r(state, cpu->h);
            break;

        case 0xB5: // OR L
            or_r(state, cpu->l);
            break;

        case 0xB6: // OR (HL)
            or_r(state, read8(state, cpu->hl));
            break;

        case 0xB7: // OR A
            or_r(state, cpu->a);
            break;

        case 0xB8: // CP B
            cp_r(state, cpu->b);
            break;

        case 0xB9: // CP C
            cp_r(state, cpu->c);
            break;

        case 0xBA: // CP D
            cp_r(state, cpu->d);
            break;

        case 0xBB: // CP E
            cp_r(state, cpu->e);
            break;

        case 0xBC: // CP H
            cp_r(state, cpu->h);
            break;

        case 0xBD: // CP L
            cp_r(state, cpu->l);
            break;

        case 0xBE: // CP (HL)
            cp_r(state, read8(state, cpu->hl));
            break;

        case 0xBF: // CP A
            cp_r(state, cpu->a);
            break;

        case 0xC0: // RET NZ
            ret_nz(state);
            break;

        case 0xC1: // POP BC
            cpu->bc = pop16(state);
            break;

        case 0xC2: // JP NZ, NN
            jp_nz(state);
            break;

        case 0xC3: // JP NN
            jp_nn(state);
            break;

        case 0xC4: // CALL NZ, NN
            call_nz(state);
            break;

        case 0xC5: // PUSH BC
            push16(state, cpu->bc);
            break;

        case 0xC6: // ADD N
            add_r(state, read8_pc(state));
            break;

        case 0xC7: // RST $00
            rst(state, 0x00);
            break;

        case 0xC8: // RET Z
            ret_z(state);
            break;

        case 0xC9: // RET
            ret(state);
            break;

        case 0xCA: // JP Z, NN
            jp_z(state);
            break;

        case 0xCB: // CB-PREFIX
            switch (read8_pc(state)) {

                case 0x00: // RLC B
                    rlc_r(state, &cpu->b);
                    break;

                case 0x01: // RLC C
                    rlc_r(state, &cpu->c);
                    break;

                case 0x02: // RLC D
                    rlc_r(state, &cpu->d);
                    break;

                case 0x03: // RLC E
                    rlc_r(state, &cpu->e);
                    break;

                case 0x04: // RLC H
                    rlc_r(state, &cpu->h);
                    break;

                case 0x05: // RLC L
                    rlc_r(state, &cpu->l);
                    break;

                case 0x06: // RLC (HL)
                    rlc_mem_hl(state);
                    break;

                case 0x07: // RLC A
                    rlc_r(state, &cpu->a);
                    break;

                case 0x08: // RRC B
                    rrc_r(state, &cpu->b);
                    break;

                case 0x09: // RRC C
                    rrc_r(state, &cpu->c);
                    break;

                case 0x0A: // RRC D
                    rrc_r(state, &cpu->d);
                    break;

                case 0x0B: // RRC E
                    rrc_r(state, &cpu->e);
                    break;

                case 0x0C: // RRC H
                    rrc_r(state, &cpu->h);
                    break;

                case 0x0D: // RRC L
                    rrc_r(state, &cpu->l);
                    break;

                case 0x0E: // RRC (HL)
                    rrc_mem_hl(state);
                    break;

                case 0x0F: // RRC A
                    rrc_r(state, &cpu->a);
                    break;

                case 0x10: // RL B
                    rl_r(state, &cpu->b);
                    break;

                case 0x11: // RL C
                    rl_r(state, &cpu->c);
                    break;

                case 0x12: // RL D
                    rl_r(state, &cpu->d);
                    break;

                case 0x13: // RL E
                    rl_r(state, &cpu->e);
                    break;

                case 0x14: // RL H
                    rl_r(state, &cpu->h);
                    break;

                case 0x15: // RL L
                    rl_r(state, &cpu->l);
                    break;

                case 0x16: // RL (HL)
                    rl_mem_hl(state);
                    break;

                case 0x17: // RL A
                    rl_r(state, &cpu->a);
                    break;

                case 0x18: // RR B
                    rr_r(state, &cpu->b);
                    break;

                case 0x19: // RR C
                    rr_r(state, &cpu->c);
                    break;

                case 0x1A: // RR D
                    rr_r(state, &cpu->d);
                    break;

                case 0x1B: // RR E
                    rr_r(state, &cpu->e);
                    break;

                case 0x1C: // RR H
                    rr_r(state, &cpu->h);
                    break;

                case 0x1D: // RR L
                    rr_r(state, &cpu->l);
                    break;

                case 0x1E: // RR (HL)
                    rr_mem_hl(state);
                    break;

                case 0x1F: // RR A
                    rr_r(state, &cpu->a);
                    break;

                case 0x20: // SLA B
                    sla_r(state, &cpu->b);
                    break;

                case 0x21: // SLA C
                    sla_r(state, &cpu->c);
                    break;

                case 0x22: // SLA D
                    sla_r(state, &cpu->d);
                    break;

                case 0x23: // SLA E
                    sla_r(state, &cpu->e);
                    break;

                case 0x24: // SLA H
                    sla_r(state, &cpu->h);
                    break;

                case 0x25: // SLA L
                    sla_r(state, &cpu->l);
                    break;

                case 0x26: // SLA (HL)
                    sla_mem_hl(state);
                    break;

                case 0x27: // SLA A
                    sla_r(state, &cpu->a);
                    break;

                case 0x28: // SRA B
                    sra_r(state, &cpu->b);
                    break;

                case 0x29: // SRA C
                    sra_r(state, &cpu->c);
                    break;

                case 0x2A: // SRA D
                    sra_r(state, &cpu->d);
                    break;

                case 0x2B: // SRA E
                    sra_r(state, &cpu->e);
                    break;

                case 0x2C: // SRA H
                    sra_r(state, &cpu->h);
                    break;

                case 0x2D: // SRA L
                    sra_r(state, &cpu->l);
                    break;

                case 0x2E: // SRA (HL)
                    sra_mem_hl(state);
                    break;

                case 0x2F: // SRA A
                    sra_r(state, &cpu->a);
                    break;

                case 0x30: // SWAP B
                    swap_r(state, &cpu->b);
                    break;

                case 0x31: // SWAP C
                    swap_r(state, &cpu->c);
                    break;

                case 0x32: // SWAP D
                    swap_r(state, &cpu->d);
                    break;

                case 0x33: // SWAP E
                    swap_r(state, &cpu->e);
                    break;

                case 0x34: // SWAP H
                    swap_r(state, &cpu->h);
                    break;

                case 0x35: // SWAP L
                    swap_r(state, &cpu->l);
                    break;

                case 0x36: // SWAP (HL)
                    swap_mem_hl(state);
                    break;

                case 0x37: // SWAP A
                    swap_r(state, &cpu->a);
                    break;

                case 0x38: // SRL B
                    srl_r(state, &cpu->b);
                    break;

                case 0x39: // SRL C
                    srl_r(state, &cpu->c);
                    break;

                case 0x3A: // SRL D
                    srl_r(state, &cpu->d);
                    break;

                case 0x3B: // SRL E
                    srl_r(state, &cpu->e);
                    break;

                case 0x3C: // SRL H
                    srl_r(state, &cpu->h);
                    break;

                case 0x3D: // SRL L
                    srl_r(state, &cpu->l);
                    break;

                case 0x3E: // SRL (HL)
                    srl_mem_hl(state);
                    break;

                case 0x3F: // SRL A
                    srl_r(state, &cpu->a);
                    break;

                case 0x40: // BIT 0, B
                    bit_r(state, cpu->b, 0);
                    break;

                case 0x41: // BIT 0, C
                    bit_r(state, cpu->c, 0);
                    break;

                case 0x42: // BIT 0, D
                    bit_r(state, cpu->d, 0);
                    break;

                case 0x43: // BIT 0, E
                    bit_r(state, cpu->e, 0);
                    break;

                case 0x44: // BIT 0, H
                    bit_r(state, cpu->h, 0);
                    break;

                case 0x45: // BIT 0, L
                    bit_r(state, cpu->l, 0);
                    break;

                case 0x46: // BIT 0, (HL)
                    bit_r(state, read8(state, cpu->hl), 0);
                    break;

                case 0x47: // BIT 0, A
                    bit_r(state, cpu->a, 0);
                    break;

                case 0x48: // BIT 1, B
                    bit_r(state, cpu->b, 1);
                    break;

                case 0x49: // BIT 1, C
                    bit_r(state, cpu->c, 1);
                    break;

                case 0x4A: // BIT 1, D
                    bit_r(state, cpu->d, 1);
                    break;

                case 0x4B: // BIT 1, E
                    bit_r(state, cpu->e, 1);
                    break;

                case 0x4C: // BIT 1, H
                    bit_r(state, cpu->h, 1);
                    break;

                case 0x4D: // BIT 1, L
                    bit_r(state, cpu->l, 1);
                    break;

                case 0x4E: // BIT 1, (HL)
                    bit_r(state, read8(state, cpu->hl), 1);
                    break;

                case 0x4F: // BIT 1, A
                    bit_r(state, cpu->a, 1);
                    break;

                case 0x50: // BIT 2, B
                    bit_r(state, cpu->b, 2);
                    break;

                case 0x51: // BIT 2, C
                    bit_r(state, cpu->c, 2);
                    break;

                case 0x52: // BIT 2, D
                    bit_r(state, cpu->d, 2);
                    break;

                case 0x53: // BIT 2, E
                    bit_r(state, cpu->e, 2);
                    break;

                case 0x54: // BIT 2, H
                    bit_r(state, cpu->h, 2);
                    break;

                case 0x55: // BIT 2, L
                    bit_r(state, cpu->l, 2);
                    break;

                case 0x56: // BIT 2, (HL)
                    bit_r(state, read8(state, cpu->hl), 2);
                    break;

                case 0x57: // BIT 2, A
                    bit_r(state, cpu->a, 2);
                    break;

                case 0x58: // BIT 3, B
                    bit_r(state, cpu->b, 3);
                    break;

                case 0x59: // BIT 3, C
                    bit_r(state, cpu->c, 3);
                    break;

                case 0x5A: // BIT 3, D
                    bit_r(state, cpu->d, 3);
                    break;

                case 0x5B: // BIT 3, E
                    bit_r(state, cpu->e, 3);
                    break;

                case 0x5C: // BIT 3, H
                    bit_r(state, cpu->h, 3);
                    break;

                case 0x5D: // BIT 3, L
                    bit_r(state, cpu->l, 3);
                    break;

                case 0x5E: // BIT 3, (HL)
                    bit_r(state, read8(state, cpu->hl), 3);
                    break;

                case 0x5F: // BIT 3, A
                    bit_r(state, cpu->a, 3);
                    break;

                case 0x60: // BIT 4, B
                    bit_r(state, cpu->b, 4);
                    break;

                case 0x61: // BIT 4, C
                    bit_r(state, cpu->c, 4);
                    break;

                case 0x62: // BIT 4, D
                    bit_r(state, cpu->d, 4);
                    break;

                case 0x63: // BIT 4, E
                    bit_r(state, cpu->e, 4);
                    break;

                case 0x64: // BIT 4, H
                    bit_r(state, cpu->h, 4);
                    break;

                case 0x65: // BIT 4, L
                    bit_r(state, cpu->l, 4);
                    break;

                case 0x66: // BIT 4, (HL)
                    bit_r(state, read8(state, cpu->hl), 4);
                    break;

                case 0x67: // BIT 4, A
                    bit_r(state, cpu->a, 4);
                    break;

                case 0x68: // BIT 5, B
                    bit_r(state, cpu->b, 5);
                    break;

                case 0x69: // BIT 5, C
                    bit_r(state, cpu->c, 5);
                    break;

                case 0x6A: // BIT 5, D
                    bit_r(state, cpu->d, 5);
                    break;

                case 0x6B: // BIT 5, E
                    bit_r(state, cpu->e, 5);
                    break;

                case 0x6C: // BIT 5, H
                    bit_r(state, cpu->h, 5);
                    break;

                case 0x6D: // BIT 5, L
                    bit_r(state, cpu->l, 5);
                    break;

                case 0x6E: // BIT 5, (HL)
                    bit_r(state, read8(state, cpu->hl), 5);
                    break;

                case 0x6F: // BIT 5, A
                    bit_r(state, cpu->a, 5);
                    break;

                case 0x70: // BIT 6, B
                    bit_r(state, cpu->b, 6);
                    break;

                case 0x71: // BIT 6, C
                    bit_r(state, cpu->c, 6);
                    break;

                case 0x72: // BIT 6, D
                    bit_r(state, cpu->d, 6);
                    break;

                case 0x73: // BIT 6, E
                    bit_r(state, cpu->e, 6);
                    break;

                case 0x74: // BIT 6, H
                    bit_r(state, cpu->h, 6);
                    break;

                case 0x75: // BIT 6, L
                    bit_r(state, cpu->l, 6);
                    break;

                case 0x76: // BIT 6, (HL)
                    bit_r(state, read8(state, cpu->hl), 6);
                    break;

                case 0x77: // BIT 6, A
                    bit_r(state, cpu->a, 6);
                    break;

                case 0x78: // BIT 7, B
                    bit_r(state, cpu->b, 7);
                    break;

                case 0x79: // BIT 7, C
                    bit_r(state, cpu->c, 7);
                    break;

                case 0x7A: // BIT 7, D
                    bit_r(state, cpu->d, 7);
                    break;

                case 0x7B: // BIT 7, E
                    bit_r(state, cpu->e, 7);
                    break;

                case 0x7C: // BIT 7, H
                    bit_r(state, cpu->h, 7);
                    break;

                case 0x7D: // BIT 7, L
                    bit_r(state, cpu->l, 7);
                    break;

                case 0x7E: // BIT 7, (HL)
                    bit_r(state, read8(state, cpu->hl), 7);
                    break;

                case 0x7F: // BIT 7, A
                    bit_r(state, cpu->a, 7);
                    break;

                case 0x80: // RES 0, B
                    set_bit(&cpu->b, 0, false);
                    break;

                case 0x81: // RES 0, C
                    set_bit(&cpu->c, 0, false);
                    break;

                case 0x82: // RES 0, D
                    set_bit(&cpu->d, 0, false);
                    break;

                case 0x83: // RES 0, E
                    set_bit(&cpu->e, 0, false);
                    break;

                case 0x84: // RES 0, H
                    set_bit(&cpu->h, 0, false);
                    break;

                case 0x85: // RES 0, L
                    set_bit(&cpu->l, 0, false);
                    break;

                case 0x86: // RES 0, (HL)
                    set_bit_mem_hl(state, 0, false);
                    break;

                case 0x87: // RES 0, A
                    set_bit(&cpu->a, 0, false);
                    break;

                case 0x88: // RES 1, B
                    set_bit(&cpu->b, 1, false);
                    break;

                case 0x89: // RES 1, C
                    set_bit(&cpu->c, 1, false);
                    break;

                case 0x8A: // RES 1, D
                    set_bit(&cpu->d, 1, false);
                    break;

                case 0x8B: // RES 1, E
                    set_bit(&cpu->e, 1, false);
                    break;

                case 0x8C: // RES 1, H
                    set_bit(&cpu->h, 1, false);
                    break;

                case 0x8D: // RES 1, L
                    set_bit(&cpu->l, 1, false);
                    break;

                case 0x8E: // RES 1, (HL)
                    set_bit_mem_hl(state, 1, false);
                    break;

                case 0x8F: // RES 1, A
                    set_bit(&cpu->a, 1, false);
                    break;

                case 0x90: // RES 2, B
                    set_bit(&cpu->b, 2, false);
                    break;

                case 0x91: // RES 2, C
                    set_bit(&cpu->c, 2, false);
                    break;

                case 0x92: // RES 2, D
                    set_bit(&cpu->d, 2, false);
                    break;

                case 0x93: // RES 2, E
                    set_bit(&cpu->e, 2, false);
                    break;

                case 0x94: // RES 2, H
                    set_bit(&cpu->h, 2, false);
                    break;

                case 0x95: // RES 2, L
                    set_bit(&cpu->l, 2, false);
                    break;

                case 0x96: // RES 2, (HL)
                    set_bit_mem_hl(state, 2, false);
                    break;

                case 0x97: // RES 2, A
                    set_bit(&cpu->a, 2, false);
                    break;

                case 0x98: // RES 3, B
                    set_bit(&cpu->b, 3, false);
                    break;

                case 0x99: // RES 3, C
                    set_bit(&cpu->c, 3, false);
                    break;

                case 0x9A: // RES 3, D
                    set_bit(&cpu->d, 3, false);
                    break;

                case 0x9B: // RES 3, E
                    set_bit(&cpu->e, 3, false);
                    break;

                case 0x9C: // RES 3, H
                    set_bit(&cpu->h, 3, false);
                    break;

                case 0x9D: // RES 3, L
                    set_bit(&cpu->l, 3, false);
                    break;

                case 0x9E: // RES 3, (HL)
                    set_bit_mem_hl(state, 3, false);
                    break;

                case 0x9F: // RES 3, A
                    set_bit(&cpu->a, 3, false);
                    break;

                case 0xA0: // RES 4, B
                    set_bit(&cpu->b, 4, false);
                    break;

                case 0xA1: // RES 4, C
                    set_bit(&cpu->c, 4, false);
                    break;

                case 0xA2: // RES 4, D
                    set_bit(&cpu->d, 4, false);
                    break;

                case 0xA3: // RES 4, E
                    set_bit(&cpu->e, 4, false);
                    break;

                case 0xA4: // RES 4, H
                    set_bit(&cpu->h, 4, false);
                    break;

                case 0xA5: // RES 4, L
                    set_bit(&cpu->l, 4, false);
                    break;

                case 0xA6: // RES 4, (HL)
                    set_bit_mem_hl(state, 4, false);
                    break;

                case 0xA7: // RES 4, A
                    set_bit(&cpu->a, 4, false);
                    break;

                case 0xA8: // RES 5, B
                    set_bit(&cpu->b, 5, false);
                    break;

                case 0xA9: // RES 5, C
                    set_bit(&cpu->c, 5, false);
                    break;

                case 0xAA: // RES 5, D
                    set_bit(&cpu->d, 5, false);
                    break;

                case 0xAB: // RES 5, E
                    set_bit(&cpu->e, 5, false);
                    break;

                case 0xAC: // RES 5, H
                    set_bit(&cpu->h, 5, false);
                    break;

                case 0xAD: // RES 5, L
                    set_bit(&cpu->l, 5, false);
                    break;

                case 0xAE: // RES 5, (HL)
                    set_bit_mem_hl(state, 5, false);
                    break;

                case 0xAF: // RES 5, A
                    set_bit(&cpu->a, 5, false);
                    break;

                case 0xB0: // RES 6, B
                    set_bit(&cpu->b, 6, false);
                    break;

                case 0xB1: // RES 6, C
                    set_bit(&cpu->c, 6, false);
                    break;

                case 0xB2: // RES 6, D
                    set_bit(&cpu->d, 6, false);
                    break;

                case 0xB3: // RES 6, E
                    set_bit(&cpu->e, 6, false);
                    break;

                case 0xB4: // RES 6, H
                    set_bit(&cpu->h, 6, false);
                    break;

                case 0xB5: // RES 6, L
                    set_bit(&cpu->l, 6, false);
                    break;

                case 0xB6: // RES 6, (HL)
                    set_bit_mem_hl(state, 6, false);
                    break;

                case 0xB7: // RES 6, A
                    set_bit(&cpu->a, 6, false);
                    break;

                case 0xB8: // RES 7, B
                    set_bit(&cpu->b, 7, false);
                    break;

                case 0xB9: // RES 7, C
                    set_bit(&cpu->c, 7, false);
                    break;

                case 0xBA: // RES 7, D
                    set_bit(&cpu->d, 7, false);
                    break;

                case 0xBB: // RES 7, E
                    set_bit(&cpu->e, 7, false);
                    break;

                case 0xBC: // RES 7, H
                    set_bit(&cpu->h, 7, false);
                    break;

                case 0xBD: // RES 7, L
                    set_bit(&cpu->l, 7, false);
                    break;

                case 0xBE: // RES 7, (HL)
                    set_bit_mem_hl(state, 7, false);
                    break;

                case 0xBF: // RES 7, A
                    set_bit(&cpu->a, 7, false);
                    break;

                case 0xC0: // SET 0, B
                    set_bit(&cpu->b, 0, true);
                    break;

                case 0xC1: // SET 0, C
                    set_bit(&cpu->c, 0, true);
                    break;

                case 0xC2: // SET 0, D
                    set_bit(&cpu->d, 0, true);
                    break;

                case 0xC3: // SET 0, E
                    set_bit(&cpu->e, 0, true);
                    break;

                case 0xC4: // SET 0, H
                    set_bit(&cpu->h, 0, true);
                    break;

                case 0xC5: // SET 0, L
                    set_bit(&cpu->l, 0, true);
                    break;

                case 0xC6: // SET 0, (HL)
                    set_bit_mem_hl(state, 0, true);
                    break;

                case 0xC7: // SET 0, A
                    set_bit(&cpu->a, 0, true);
                    break;

                case 0xC8: // SET 1, B
                    set_bit(&cpu->b, 1, true);
                    break;

                case 0xC9: // SET 1, C
                    set_bit(&cpu->c, 1, true);
                    break;

                case 0xCA: // SET 1, D
                    set_bit(&cpu->d, 1, true);
                    break;

                case 0xCB: // SET 1, E
                    set_bit(&cpu->e, 1, true);
                    break;

                case 0xCC: // SET 1, H
                    set_bit(&cpu->h, 1, true);
                    break;

                case 0xCD: // SET 1, L
                    set_bit(&cpu->l, 1, true);
                    break;

                case 0xCE: // SET 1, (HL)
                    set_bit_mem_hl(state, 1, true);
                    break;

                case 0xCF: // SET 1, A
                    set_bit(&cpu->a, 1, true);
                    break;

                case 0xD0: // SET 2, B
                    set_bit(&cpu->b, 2, true);
                    break;

                case 0xD1: // SET 2, C
                    set_bit(&cpu->c, 2, true);
                    break;

                case 0xD2: // SET 2, D
                    set_bit(&cpu->d, 2, true);
                    break;

                case 0xD3: // SET 2, E
                    set_bit(&cpu->e, 2, true);
                    break;

                case 0xD4: // SET 2, H
                    set_bit(&cpu->h, 2, true);
                    break;

                case 0xD5: // SET 2, L
                    set_bit(&cpu->l, 2, true);
                    break;

                case 0xD6: // SET 2, (HL)
                    set_bit_mem_hl(state, 2, true);
                    break;

                case 0xD7: // SET 2, A
                    set_bit(&cpu->a, 2, true);
                    break;

                case 0xD8: // SET 3, B
                    set_bit(&cpu->b, 3, true);
                    break;

                case 0xD9: // SET 3, C
                    set_bit(&cpu->c, 3, true);
                    break;

                case 0xDA: // SET 3, D
                    set_bit(&cpu->d, 3, true);
                    break;

                case 0xDB: // SET 3, E
                    set_bit(&cpu->e, 3, true);
                    break;

                case 0xDC: // SET 3, H
                    set_bit(&cpu->h, 3, true);
                    break;

                case 0xDD: // SET 3, L
                    set_bit(&cpu->l, 3, true);
                    break;

                case 0xDE: // SET 3, (HL)
                    set_bit_mem_hl(state, 3, true);
                    break;

                case 0xDF: // SET 3, A
                    set_bit(&cpu->a, 3, true);
                    break;

                case 0xE0: // SET 4, B
                    set_bit(&cpu->b, 4, true);
                    break;

                case 0xE1: // SET 4, C
                    set_bit(&cpu->c, 4, true);
                    break;

                case 0xE2: // SET 4, D
                    set_bit(&cpu->d, 4, true);
                    break;

                case 0xE3: // SET 4, E
                    set_bit(&cpu->e, 4, true);
                    break;

                case 0xE4: // SET 4, H
                    set_bit(&cpu->h, 4, true);
                    break;

                case 0xE5: // SET 4, L
                    set_bit(&cpu->l, 4, true);
                    break;

                case 0xE6: // SET 4, (HL)
                    set_bit_mem_hl(state, 4, true);
                    break;

                case 0xE7: // SET 4, A
                    set_bit(&cpu->a, 4, true);
                    break;

                case 0xE8: // SET 5, B
                    set_bit(&cpu->b, 5, true);
                    break;

                case 0xE9: // SET 5, C
                    set_bit(&cpu->c, 5, true);
                    break;

                case 0xEA: // SET 5, D
                    set_bit(&cpu->d, 5, true);
                    break;

                case 0xEB: // SET 5, E
                    set_bit(&cpu->e, 5, true);
                    break;

                case 0xEC: // SET 5, H
                    set_bit(&cpu->h, 5, true);
                    break;

                case 0xED: // SET 5, L
                    set_bit(&cpu->l, 5, true);
                    break;

                case 0xEE: // SET 5, (HL)
                    set_bit_mem_hl(state, 5, true);
                    break;

                case 0xEF: // SET 5, A
                    set_bit(&cpu->a, 5, true);
                    break;

                case 0xF0: // SET 6, B
                    set_bit(&cpu->b, 6, true);
                    break;

                case 0xF1: // SET 6, C
                    set_bit(&cpu->c, 6, true);
                    break;

                case 0xF2: // SET 6, D
                    set_bit(&cpu->d, 6, true);
                    break;

                case 0xF3: // SET 6, E
                    set_bit(&cpu->e, 6, true);
                    break;

                case 0xF4: // SET 6, H
                    set_bit(&cpu->h, 6, true);
                    break;

                case 0xF5: // SET 6, L
                    set_bit(&cpu->l, 6, true);
                    break;

                case 0xF6: // SET 6, (HL)
                    set_bit_mem_hl(state, 6, true);
                    break;

                case 0xF7: // SET 6, A
                    set_bit(&cpu->a, 6, true);
                    break;

                case 0xF8: // SET 7, B
                    set_bit(&cpu->b, 7, true);
                    break;

                case 0xF9: // SET 7, C
                    set_bit(&cpu->c, 7, true);
                    break;

                case 0xFA: // SET 7, D
                    set_bit(&cpu->d, 7, true);
                    break;

                case 0xFB: // SET 7, E
                    set_bit(&cpu->e, 7, true);
                    break;

                case 0xFC: // SET 7, H
                    set_bit(&cpu->h, 7, true);
                    break;

                case 0xFD: // SET 7, L
                    set_bit(&cpu->l, 7, true);
                    break;

                case 0xFE: // SET 7, (HL)
                    set_bit_mem_hl(state, 7, true);
                    break;

                case 0xFF: // SET 7, A
                    set_bit(&cpu->a, 7, true);
                    break;

                default:
                    assert(false);
            }
            break;

        case 0xCC: // CALL Z, NN
            call_z(state);
            break;

        case 0xCD: // CALL NN
            call_nn(state);
            break;

        case 0xCE: // ADC N
            adc_r(state, read8_pc(state));
            break;

        case 0xCF: // RST $08
            rst(state, 0x08);
            break;

        case 0xD0: // RET NC
            ret_nc(state);
            break;

        case 0xD1: // POP DE
            cpu->de = pop16(state);
            break;

        case 0xD2: // JP NC, NN
            jp_nc(state);
            break;

        case 0xD3: // INVALID
            break;

        case 0xD4: // CALL NC, NN
            call_nc(state);
            break;

        case 0xD5: // PUSH DE
            push16(state, cpu->de);
            break;

        case 0xD6: // SUB N
            sub_r(state, read8_pc(state));
            break;

        case 0xD7: // RST $10
            rst(state, 0x10);
            break;

        case 0xD8: // RET C
            ret_c(state);
            break;

        case 0xD9: // RETI
            reti(state);
            break;

        case 0xDA: // JP C, NN
            jp_c(state);
            break;

        case 0xDB: // INVALID
            break;

        case 0xDC: // CALL C, NN
            call_c(state);
            break;

        case 0xDD: // INVALID
            break;

        case 0xDE: // SBC N
            sbc_r(state, read8_pc(state));
            break;

        case 0xDF: // RST $18
            rst(state, 0x18);
            break;

        case 0xE0: // LDH (N), A
            write8(state, (uint16_t) (0xFF00 + read8_pc(state)), cpu->a);
            break;

        case 0xE1: // POP HL
            cpu->hl = pop16(state);
            break;

        case 0xE2: // LDH (C), A
            write8(state, (uint16_t) (0xFF00 + cpu->c), cpu->a);
            break;

        case 0xE3: // INVALID
            break;

        case 0xE4: // INVALID
            break;

        case 0xE5: // PUSH HL
            push16(state, cpu->hl);
            break;

        case 0xE6: // AND N
            and_r(state, read8_pc(state));
            break;

        case 0xE7: // RST $20
            rst(state, 0x20);
            break;

        case 0xE8: // ADD SP, N
            add_sp_n(state);
            break;

        case 0xE9: // JP HL
            cpu->pc = cpu->hl;
            break;

        case 0xEA: // LD (NN), A
            write8(state, read16_pc(state), cpu->a);
            break;

        case 0xEB: // INVALID
            break;

        case 0xEC: // INVALID
            break;

        case 0xED: // INVALID
            break;

        case 0xEE: // XOR N
            xor_r(state, read8_pc(state));
            break;

        case 0xEF: // RST $28
            rst(state, 0x28);
            break;

        case 0xF0: // LDH A, (N)
            cpu->a = read8(state, (uint16_t) (0xFF00 + read8_pc(state)));
            break;

        case 0xF1: // POP AF
            cpu->af = (uint16_t) (pop16(state) & 0xFFF0);
            break;

        case 0xF2: // LDH A, (C)
            cpu->a = read8(state, (uint16_t) (0xFF00 + cpu->c));
            break;

        case 0xF3: // DI
            cpu->ime = false;
            break;

        case 0xF4: // INVALID
            break;

        case 0xF5: // PUSH AF
            push16(state, (uint16_t) (cpu->af & 0xFFF0));
            break;

        case 0xF6: // OR N
            or_r(state, read8_pc(state));
            break;

        case 0xF7: // RST $30
            rst(state, 0x30);
            break;

        case 0xF8: // LDHL SP, N
            ldhl_sp_n(state);
            break;

        case 0xF9: // LD SP, HL
            ld_sp_hl(state);
            break;

        case 0xFA: // LD A, (NN)
            cpu->a = read8(state, read8_pc(state));
            break;

        case 0xFB: // EI
            cpu->ime = true;
            break;

        case 0xFC: // INVALID
            break;

        case 0xFD: // INVALID
            break;

        case 0xFE: // CP N
            cp_r(state, read8_pc(state));
            break;

        case 0xFF: // RST $38
            rst(state, 0x38);
            break;

        default:
            assert(false);
    }
}
