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
    *byte &= ~(1 << n) | (value << n);
}

static DMG_INLINE bool get_bit(uint8_t *byte, uint8_t n) {
    return (*byte >> n) != 0;
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

static DMG_INLINE bool get_h(uint8_t *f) {
    return get_bit(f, 6);
}

static DMG_INLINE bool get_n(uint8_t *f) {
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
    set_h(f, (*r & 0x0F) == 0x0F);
    *r += 1;
    set_z(f, *r == 0x00);
    set_n(f, false);
}

static DMG_INLINE void dec_r(DMGState *state, uint8_t *r) {
    uint8_t *f = &state->cpu.f;
    set_h(f, (*r & 0x0F) == 0x00);
    *r -= 1;
    set_z(f, *r == 0x00);
    set_n(f, false);
}

static DMG_INLINE void rlca(DMGState *state) {
    uint8_t *f = &state->cpu.f;
    uint8_t *a = &state->cpu.a;
    uint8_t carry = *a >> 7;
    *a = (*a << 1) | carry;
    // This is a contentious line. But Gambatte also sets Z to false
    set_z(f, false);
    set_n(f, false);
    set_h(f, false);
    set_c(f, carry);
}

static DMG_INLINE void rrca(DMGState *state) {
    uint8_t *f = &state->cpu.f;
    uint8_t *a = &state->cpu.a;
    uint8_t carry = (uint8_t) (*a & 0x01);
    *a = (*a >> 1) | (carry << 7);
    // This is a contentious line. But Gambatte also sets Z to false
    set_z(f, false);
    set_n(f, false);
    set_h(f, false);
    set_c(f, carry);
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

static DMG_INLINE void rla(DMGState *state) {
    uint8_t *f = &state->cpu.f;
    uint8_t *a = &state->cpu.a;
    uint8_t carry = *a >> 7;
    *a = (*a << 1) | get_c(f);
    set_z(f, false);
    set_n(f, false);
    set_h(f, false);
    set_c(f, carry);
}

static DMG_INLINE void rra(DMGState *state) {
    uint8_t *f = &state->cpu.f;
    uint8_t *a = &state->cpu.a;
    uint8_t carry = *a & (uint8_t) 0x01;
    *a = (*a >> 1) | (get_c(f) << 7);
    set_z(f, false);
    set_n(f, false);
    set_h(f, false);
    set_c(f, carry);
}

static DMG_INLINE void jr_n(DMGState *state) {
    state->cycles += 4;
    uint8_t n = read8_pc(state);
    n = (uint8_t) ((n ^ 0x80) - 0x80);
    state->cpu.pc += n;
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
    set_h(f, (overflow & 0x0F) == 0x0F);
    overflow = (uint16_t) ((overflow + 1) & 0xFF);
    write8(state, *hl, (uint8_t) overflow);
    set_z(f, overflow == 0x00);
    set_n(f, false);
}

static DMG_INLINE void dec_mem_hl(DMGState *state) {
    uint16_t *hl = &state->cpu.hl;
    uint8_t *f = &state->cpu.f;
    uint16_t overflow = read8(state, *hl);
    set_h(f, (overflow & 0x0F) == 0x00);
    overflow = (uint16_t) ((overflow - 1) & 0xFF);
    write8(state, *hl, (uint8_t) overflow);
    set_z(f, overflow == 0x00);
    set_n(f, false);
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
    // TODO: Unimplemented
    assert(false);
}

static DMG_INLINE void add_r(DMGState *state, uint8_t r) {
    uint8_t *f = &state->cpu.f;
    uint8_t *a = &state->cpu.a;
    uint16_t tmp = *a;
    set_h(f, ((tmp & 0x0F) + (r & 0x0F)) > 0x0F);
    a += r;
    set_z(f, *a == 0x00);
    set_c(f, tmp > *a);
    set_n(f, false);
}

static DMG_INLINE void adc_r(DMGState *state, uint8_t r) {
    uint8_t *f = &state->cpu.f;
    uint8_t *a = &state->cpu.a;
    uint16_t tmp = *a + r + get_c(f);
    set_h(f, ((tmp & 0x0F) + (r & 0x0F) + get_c(f)) > 0x0F);
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
    a -= r;
    set_z(f, *a == 0x00);
    set_n(f, true);
}

static DMG_INLINE void suba(DMGState *state) {
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

void dmg_cpu_run(DMGState *state, uint32_t cycles) {
    DMGCpu *cpu = &state->cpu;

    uint8_t opcode = read8_pc(state);
    switch (opcode) {
        case 0x00:
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
            write8(state, cpu->de, cpu->a);
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

        case 0x7E: // LD L, (HL)
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

        case 0x8E: // ADD (HL)
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
    }
}
