/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

/*
 * This file is included directly into other source files.  It provides
 * functions common across 6809 ISA CPUs.
 */

/* Memory interface */

static uint8_t fetch_byte(struct MC6809 *cpu, uint16_t a) {
	cpu->cycle++;
	return cpu->read_cycle(a);
}

static void store_byte(struct MC6809 *cpu, uint16_t a, uint8_t d) {
	cpu->cycle++;
	cpu->write_cycle(a, d);
}

/* Read & write various addressing modes */

static uint8_t byte_immediate(struct MC6809 *cpu) {
	return fetch_byte(cpu, REG_PC++);
}

static uint8_t byte_direct(struct MC6809 *cpu) {
	uint16_t ea = ea_direct(cpu);
	return fetch_byte(cpu, ea);
}

static uint8_t byte_extended(struct MC6809 *cpu) {
	uint16_t ea = ea_extended(cpu);
	return fetch_byte(cpu, ea);
}

static uint8_t byte_indexed(struct MC6809 *cpu) {
	uint16_t ea = ea_indexed(cpu);
	return fetch_byte(cpu, ea);
}

static uint16_t word_immediate(struct MC6809 *cpu) {
	unsigned v = fetch_byte(cpu, REG_PC++) << 8;
	v |= fetch_byte(cpu, REG_PC++);
	return v;
}

static uint16_t word_direct(struct MC6809 *cpu) {
	uint16_t ea = ea_direct(cpu);
	unsigned v = fetch_byte(cpu, ea) << 8;
	v |= fetch_byte(cpu, ea + 1);
	return v;
}

static uint16_t word_extended(struct MC6809 *cpu) {
	uint16_t ea = ea_extended(cpu);
	unsigned v = fetch_byte(cpu, ea) << 8;
	v |= fetch_byte(cpu, ea + 1);
	return v;
}

static uint16_t word_indexed(struct MC6809 *cpu) {
	uint16_t ea = ea_indexed(cpu);
	unsigned v = fetch_byte(cpu, ea) << 8;
	v |= fetch_byte(cpu, ea + 1);
	return v;
}

static uint16_t short_relative(struct MC6809 *cpu) {
	return sex8(byte_immediate(cpu));
}

/* Stack operations */

static void push_byte(struct MC6809 *cpu, uint16_t *s, uint8_t v) {
	store_byte(cpu, --(*s), v);
}

static void push_word(struct MC6809 *cpu, uint16_t *s, uint16_t v) {
	store_byte(cpu, --(*s), v);
	store_byte(cpu, --(*s), v >> 8);
}

static uint8_t pull_byte(struct MC6809 *cpu, uint16_t *s) {
	return fetch_byte(cpu, (*s)++);
}

static uint16_t pull_word(struct MC6809 *cpu, uint16_t *s) {
	unsigned val = fetch_byte(cpu, (*s)++);
	return (val << 8) | fetch_byte(cpu, (*s)++);
}

/* 8-bit inherent operations */

static uint8_t op_neg(struct MC6809 *cpu, uint8_t in) {
	unsigned out = ~in + 1;
	CLR_NZVC;
	SET_NZVC8(0, in, out);
	return out;
}

static uint8_t op_com(struct MC6809 *cpu, uint8_t in) {
	unsigned out = ~in;
	CLR_NZV;
	SET_NZ8(out);
	REG_CC |= CC_C;
	return out;
}

static uint8_t op_lsr(struct MC6809 *cpu, uint8_t in) {
	unsigned out = in >> 1;
	CLR_NZC;
	REG_CC |= (in & 1);
	SET_Z(out);
	return out;
}

static uint8_t op_ror(struct MC6809 *cpu, uint8_t in) {
	unsigned out = (in >> 1) | ((REG_CC & 1) << 7);
	CLR_NZC;
	REG_CC |= (in & 1);
	SET_NZ8(out);
	return out;
}

static uint8_t op_asr(struct MC6809 *cpu, uint8_t in) {
	unsigned out = (in >> 1) | (in & 0x80);
	CLR_NZC;
	REG_CC |= (in & 1);
	SET_NZ8(out);
	return out;
}

static uint8_t op_asl(struct MC6809 *cpu, uint8_t in) {
	unsigned out = in << 1;
	CLR_NZVC;
	SET_NZVC8(in, in, out);
	return out;
}

static uint8_t op_rol(struct MC6809 *cpu, uint8_t in) {
	unsigned out = (in << 1) | (REG_CC & 1);
	CLR_NZVC;
	SET_NZVC8(in, in, out);
	return out;
}

static uint8_t op_dec(struct MC6809 *cpu, uint8_t in) {
	unsigned out = in - 1;
	CLR_NZV;
	SET_NZ8(out);
	if (out == 0x7f) REG_CC |= CC_V;
	return out;
}

static uint8_t op_inc(struct MC6809 *cpu, uint8_t in) {
	unsigned out = in + 1;
	CLR_NZV;
	SET_NZ8(out);
	if (out == 0x80) REG_CC |= CC_V;
	return out;
}

static uint8_t op_tst(struct MC6809 *cpu, uint8_t in) {
	CLR_NZV;
	SET_NZ8(in);
	return in;
}

static uint8_t op_clr(struct MC6809 *cpu, uint8_t in) {
	(void)in;
	CLR_NVC;
	REG_CC |= CC_Z;
	return 0;
}

/* 8-bit arithmetic operations */

static uint8_t op_sub(struct MC6809 *cpu, uint8_t a, uint8_t b) {
	unsigned out = a - b;
	CLR_NZVC;
	SET_NZVC8(a, b, out);
	return out;
}

static uint8_t op_sbc(struct MC6809 *cpu, uint8_t a, uint8_t b) {
	unsigned out = a - b - (REG_CC & CC_C);
	CLR_NZVC;
	SET_NZVC8(a, b, out);
	return out;
}

static uint8_t op_and(struct MC6809 *cpu, uint8_t a, uint8_t b) {
	unsigned out = a & b;
	CLR_NZV;
	SET_NZ8(out);
	return out;
}

static uint8_t op_ld(struct MC6809 *cpu, uint8_t a, uint8_t b) {
	(void)a;
	CLR_NZV;
	SET_NZ8(b);
	return b;
}

static uint8_t op_eor(struct MC6809 *cpu, uint8_t a, uint8_t b) {
	unsigned out = a ^ b;
	CLR_NZV;
	SET_NZ8(out);
	return out;
}

static uint8_t op_adc(struct MC6809 *cpu, uint8_t a, uint8_t b) {
	unsigned out = a + b + (REG_CC & CC_C);
	CLR_HNZVC;
	SET_NZVC8(a, b, out);
	SET_H(a, b, out);
	return out;
}

static uint8_t op_or(struct MC6809 *cpu, uint8_t a, uint8_t b) {
	unsigned out = a | b;
	CLR_NZV;
	SET_NZ8(out);
	return out;
}

static uint8_t op_add(struct MC6809 *cpu, uint8_t a, uint8_t b) {
	unsigned out = a + b;
	CLR_HNZVC;
	SET_NZVC8(a, b, out);
	SET_H(a, b, out);
	return out;
}

/* 16-bit arithmetic operations */

static uint16_t op_sub16(struct MC6809 *cpu, uint16_t a, uint16_t b) {
	unsigned out = a - b;
	CLR_NZVC;
	SET_NZVC16(a, b, out);
	return out;
}

static uint16_t op_ld16(struct MC6809 *cpu, uint16_t a, uint16_t b) {
	(void)a;
	CLR_NZV;
	SET_NZ16(b);
	return b;
}

static uint16_t op_add16(struct MC6809 *cpu, uint16_t a, uint16_t b) {
	unsigned out = a + b;
	CLR_NZVC;
	SET_NZVC16(a, b, out);
	return out;
}

/* Various utility functions */

/* Sign extend 5 bits into 16 bits */
static uint16_t sex5(uint16_t v) {
	return (v & 0x0f) - (v & 0x10);
}

/* Sign extend 8 bits into 16 bits */
static uint16_t sex8(uint8_t v) {
	return (int16_t)(*((int8_t *)&v));
}

#define N_EOR_V ((REG_CC & CC_N)^((REG_CC & CC_V)<<2))

/* Determine branch condition from op-code */
static _Bool branch_condition(struct MC6809 *cpu, unsigned op) {
	_Bool cond;
	switch (op & 0xf) {
	default:
	case 0x0: cond = 1; break; // BRA, LBRA
	case 0x1: cond = 0; break; // BRN, LBRN
	case 0x2: cond = !(REG_CC & (CC_Z|CC_C)); break; // BHI, LBHI
	case 0x3: cond = REG_CC & (CC_Z|CC_C); break; // BLS, LBLS
	case 0x4: cond = !(REG_CC & CC_C); break; // BCC, BHS, LBCC, LBHS
	case 0x5: cond = REG_CC & CC_C; break; // BCS, BLO, LBCS, LBLO
	case 0x6: cond = !(REG_CC & CC_Z); break; // BNE, LBNE
	case 0x7: cond = REG_CC & CC_Z; break; // BEQ, LBEQ
	case 0x8: cond = !(REG_CC & CC_V); break; // BVC, LBVC
	case 0x9: cond = REG_CC & CC_V; break; // BVS, LBVS
	case 0xa: cond = !(REG_CC & CC_N); break; // BPL, LBPL
	case 0xb: cond = REG_CC & CC_N; break; // BMI, LBMI
	case 0xc: cond = !N_EOR_V; break; // BGE, LBGE
	case 0xd: cond = N_EOR_V; break; // BLT, LBLT
	case 0xe: cond = !(N_EOR_V || REG_CC & CC_Z); break; // BGT, LBGT
	case 0xf: cond = N_EOR_V || REG_CC & CC_Z; break; // BLE, LBLE
	}
	return cond;
}

