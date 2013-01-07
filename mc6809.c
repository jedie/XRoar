/*  Copyright 2003-2013 Ciaran Anscomb
 *
 *  This file is part of XRoar.
 *
 *  XRoar is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  XRoar is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XRoar.  If not, see <http://www.gnu.org/licenses/>.
 */

/* References:
 *     MC6809E data sheet,
 *         Motorola
 *     MC6809 Cycle-By-Cycle Performance,
 *         http://koti.mbnet.fi/~atjs/mc6809/Information/6809cyc.txt
 *     Dragon Update, Illegal Op-codes,
 *         Feb 1994 Ciaran Anscomb
 *     Motorola 6809 and Hitachi 6309 Programmers Reference,
 *         2009 Darren Atkinson
 */

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "portalib/glib.h"

#include "mc6809.h"

/* MPU state.  Represents current position in the high-level flow chart from
* the data sheet (figure 14). */
enum mc6809_cpu_state {
	mc6809_flow_label_a      = MC6809_COMPAT_STATE_NORMAL,
	mc6809_flow_sync         = MC6809_COMPAT_STATE_SYNC,
	mc6809_flow_dispatch_irq = MC6809_COMPAT_STATE_CWAI,
	mc6809_flow_label_b,
	mc6809_flow_reset,
	mc6809_flow_reset_check_halt,
	mc6809_flow_next_instruction,
	mc6809_flow_instruction_page_2,
	mc6809_flow_instruction_page_3,
	mc6809_flow_cwai_check_halt,
	mc6809_flow_sync_check_halt,
	mc6809_flow_done_instruction = MC6809_COMPAT_STATE_DONE_INSTRUCTION,
	mc6809_flow_hcf          = MC6809_COMPAT_STATE_HCF
};

static void mc6809_free(struct MC6809 *cpu);
static void mc6809_reset(struct MC6809 *cpu);
static void mc6809_run(struct MC6809 *cpu);
static void mc6809_jump(struct MC6809 *cpu, uint16_t pc);

/* Condition code register macros */

#define CC_E 0x80
#define CC_F 0x40
#define CC_H 0x20
#define CC_I 0x10
#define CC_N 0x08
#define CC_Z 0x04
#define CC_V 0x02
#define CC_C 0x01
#define N_EOR_V ((REG_CC & CC_N)^((REG_CC & CC_V)<<2))

#define CLR_HNZVC ( REG_CC &= ~(CC_H|CC_N|CC_Z|CC_V|CC_C) )
#define CLR_NZ    ( REG_CC &= ~(CC_N|CC_Z) )
#define CLR_NZV   ( REG_CC &= ~(CC_N|CC_Z|CC_V) )
#define CLR_NZVC  ( REG_CC &= ~(CC_N|CC_Z|CC_V|CC_C) )
#define CLR_Z     ( REG_CC &= ~(CC_Z) )
#define CLR_NZC   ( REG_CC &= ~(CC_N|CC_Z|CC_C) )
#define CLR_NVC   ( REG_CC &= ~(CC_N|CC_V|CC_C) )
#define CLR_ZC    ( REG_CC &= ~(CC_Z|CC_C) )

#define SET_Z(r)          ( REG_CC |= ((r) ? 0 : CC_Z) )
#define SET_N8(r)         ( REG_CC |= (r&0x80)>>4 )
#define SET_N16(r)        ( REG_CC |= (r&0x8000)>>12 )
#define SET_H(a,b,r)      ( REG_CC |= ((a^b^r)&0x10)<<1 )
#define SET_C8(r)         ( REG_CC |= (r&0x100)>>8 )
#define SET_C16(r)        ( REG_CC |= (r&0x10000)>>16 )
#define SET_V8(a,b,r)     ( REG_CC |= ((a^b^r^(r>>1))&0x80)>>6 )
#define SET_V16(a,b,r)    ( REG_CC |= ((a^b^r^(r>>1))&0x8000)>>14 )
#define SET_NZ8(r)        ( SET_N8(r), SET_Z(r&0xff) )
#define SET_NZ16(r)       ( SET_N16(r), SET_Z(r&0xffff) )
#define SET_NZC8(r)       ( SET_N8(r), SET_Z(r&0xff), SET_C8(r) )
#define SET_NZVC8(a,b,r)  ( SET_N8(r), SET_Z(r&0xff), SET_V8(a,b,r), SET_C8(r) )
#define SET_NZVC16(a,b,r) ( SET_N16(r), SET_Z(r&0xffff), SET_V16(a,b,r), SET_C16(r) )

/* Register access macros */

#define REG_CC (cpu->reg_cc)
#define REG_D (cpu->reg_d)
#define REG_DP (cpu->reg_dp)
#define REG_X (cpu->reg_x)
#define REG_Y (cpu->reg_y)
#define REG_U (cpu->reg_u)
#define REG_S (cpu->reg_s)
#define REG_PC (cpu->reg_pc)

/* Register access macros - separate read & write */

#define RREG_A (REG_D >> 8)
#define RREG_B (REG_D & 0xff)
#define WREG_A (MC6809_REG_A(cpu))
#define WREG_B (MC6809_REG_B(cpu))

/* CPU fetch/store goes via SAM */
#define fetch_byte(a) (cpu->cycle++, cpu->read_cycle(a))
#define store_byte(a,v) do { cpu->cycle++; cpu->write_cycle(a, v); } while (0)
/* This one only used to try and get correct timing: */
#define peek_byte(a) do { cpu->cycle++; (void)cpu->read_cycle(a); } while (0)

#define NVMA_CYCLE() peek_byte(0xffff)

#define EA_DIRECT(a)    do { a = ea_direct(cpu); } while (0)
#define EA_EXTENDED(a)  do { a = ea_extended(cpu); } while (0)
#define EA_INDEXED(a)   do { a = ea_indexed(cpu); } while (0)

/* Memory fetch macros */
#define BYTE_IMMEDIATE(a,v)     { (void)a; v = fetch_byte(REG_PC); REG_PC++; }
#define BYTE_DIRECT(a,v)        { EA_DIRECT(a); v = fetch_byte(a); }
#define BYTE_INDEXED(a,v)       { EA_INDEXED(a); v = fetch_byte(a); }
#define BYTE_EXTENDED(a,v)      { EA_EXTENDED(a); v = fetch_byte(a); }
#define WORD_IMMEDIATE(a,v)     { (void)a; v = fetch_byte(REG_PC) << 8; v |= fetch_byte(REG_PC+1); REG_PC += 2; }
#define WORD_DIRECT(a,v)        { EA_DIRECT(a); v = fetch_byte(a) << 8; v |= fetch_byte(a+1); }
#define WORD_INDEXED(a,v)       { EA_INDEXED(a); v = fetch_byte(a) << 8; v |= fetch_byte(a+1); }
#define WORD_EXTENDED(a,v)      { EA_EXTENDED(a); v = fetch_byte(a) << 8; v |= fetch_byte(a+1); }

#define SHORT_RELATIVE(r)       { BYTE_IMMEDIATE(0, r); r = sex8(r); }
#define LONG_RELATIVE(r)        WORD_IMMEDIATE(0, r)

#define PUSHWORD(s,r)   { s -= 2; store_byte(s+1, r); store_byte(s, r >> 8); }
#define PUSHBYTE(s,r)   { s--; store_byte(s, r); }
#define PULLBYTE(s,r)   { r = fetch_byte(s); s++; }
#define PULLWORD(s,r)   { r = fetch_byte(s) << 8; r |= fetch_byte(s+1); s += 2; }

#define PUSHR(s,a) { \
		unsigned postbyte; \
		BYTE_IMMEDIATE(0, postbyte); \
		NVMA_CYCLE(); \
		NVMA_CYCLE(); \
		peek_byte(s); \
		if (postbyte & 0x80) { PUSHWORD(s, REG_PC); } \
		if (postbyte & 0x40) { PUSHWORD(s, a); } \
		if (postbyte & 0x20) { PUSHWORD(s, REG_Y); } \
		if (postbyte & 0x10) { PUSHWORD(s, REG_X); } \
		if (postbyte & 0x08) { PUSHBYTE(s, REG_DP); } \
		if (postbyte & 0x04) { PUSHBYTE(s, RREG_B); } \
		if (postbyte & 0x02) { PUSHBYTE(s, RREG_A); } \
		if (postbyte & 0x01) { PUSHBYTE(s, REG_CC); } \
	}

#define PULLR(s,a) { \
		unsigned postbyte; \
		BYTE_IMMEDIATE(0, postbyte); \
		NVMA_CYCLE(); \
		NVMA_CYCLE(); \
		if (postbyte & 0x01) { PULLBYTE(s, REG_CC); } \
		if (postbyte & 0x02) { PULLBYTE(s, WREG_A); } \
		if (postbyte & 0x04) { PULLBYTE(s, WREG_B); } \
		if (postbyte & 0x08) { PULLBYTE(s, REG_DP); } \
		if (postbyte & 0x10) { PULLWORD(s, REG_X); } \
		if (postbyte & 0x20) { PULLWORD(s, REG_Y); } \
		if (postbyte & 0x40) { PULLWORD(s, a); } \
		if (postbyte & 0x80) { PULLWORD(s, REG_PC); } \
		peek_byte(s); \
	}

#define PUSH_IRQ_REGISTERS_NO_E() do { \
		NVMA_CYCLE(); \
		PUSHWORD(REG_S, REG_PC); \
		PUSHWORD(REG_S, REG_U); \
		PUSHWORD(REG_S, REG_Y); \
		PUSHWORD(REG_S, REG_X); \
		PUSHBYTE(REG_S, REG_DP); \
		PUSHBYTE(REG_S, RREG_B); \
		PUSHBYTE(REG_S, RREG_A); \
		PUSHBYTE(REG_S, REG_CC); \
	} while (0)

#define PUSH_IRQ_REGISTERS() do { \
		REG_CC |= CC_E; \
		PUSH_IRQ_REGISTERS_NO_E(); \
	} while (0)

#define PUSH_FIRQ_REGISTERS() do { \
		REG_CC &= ~CC_E; \
		NVMA_CYCLE(); \
		PUSHWORD(REG_S, REG_PC); \
		PUSHBYTE(REG_S, REG_CC); \
	} while (0)

#define TAKE_INTERRUPT(i,cm,v) do { \
		REG_CC |= (cm); \
		NVMA_CYCLE(); \
		if (cpu->interrupt_hook) { \
			cpu->interrupt_hook(cpu, v); \
		} \
		REG_PC = fetch_byte(v) << 8; \
		REG_PC |= fetch_byte((v)+1); \
		NVMA_CYCLE(); \
	} while (0)

#define INSTRUCTION_POSTHOOK() do { \
		if (cpu->instruction_posthook) { \
			cpu->instruction_posthook(cpu); \
		} \
	} while (0)

/* If right shifts of signed values are arithmetic, faster code can be used.
 * These macros depend on the compiler optimising away the unused version. */
#define sex5(v) ( ((-1>>1)==-1) ? \
        (((signed int)(v)<<((8*sizeof(signed int))-5)) >> ((8*sizeof(signed int))-5)) : \
        ((int)(((v) & 0x0f) - ((v) & 0x10))) )
#define sex8(v) ( ((-1>>1)==-1) ? \
        (((signed int)(v)<<((8*sizeof(signed int))-8)) >> ((8*sizeof(signed int))-8)) : \
        ((int8_t)(v)) )

/* Dummy handlers */
static uint8_t dummy_read_cycle(uint16_t a) { (void)a; return 0; }
static void dummy_write_cycle(uint16_t a, uint8_t v) { (void)a; (void)v; }

/* ------------------------------------------------------------------------- */

/* 8-bit inherent operations */

static inline uint8_t op_neg(struct MC6809 *cpu, uint8_t in) {
	unsigned out = ~in + 1;
	CLR_NZVC;
	SET_NZVC8(0, in, out);
	return out;
}

static inline uint8_t op_com(struct MC6809 *cpu, uint8_t in) {
	unsigned out = ~in;
	CLR_NZV;
	SET_NZ8(out);
	REG_CC |= CC_C;
	return out;
}

static inline uint8_t op_negcom(struct MC6809 *cpu, uint8_t in) {
	unsigned out = ~in + (~REG_CC & 1);
	CLR_NZVC;
	SET_NZVC8(0, in, out);
	return out;
}

static inline uint8_t op_lsr(struct MC6809 *cpu, uint8_t in) {
	unsigned out = in >> 1;
	CLR_NZC;
	REG_CC |= (in & 1);
	SET_Z(out);
	return out;
}

static inline uint8_t op_ror(struct MC6809 *cpu, uint8_t in) {
	unsigned out = (in >> 1) | ((REG_CC & 1) << 7);
	CLR_NZC;
	REG_CC |= (in & 1);
	SET_NZ8(out);
	return out;
}

static inline uint8_t op_asr(struct MC6809 *cpu, uint8_t in) {
	unsigned out = (in >> 1) | (in & 0x80);
	CLR_NZC;
	REG_CC |= (in & 1);
	SET_NZ8(out);
	return out;
}

static inline uint8_t op_asl(struct MC6809 *cpu, uint8_t in) {
	unsigned out = in << 1;
	CLR_NZVC;
	SET_NZVC8(in, in, out);
	return out;
}

static inline uint8_t op_rol(struct MC6809 *cpu, uint8_t in) {
	unsigned out = (in << 1) | (REG_CC & 1);
	CLR_NZVC;
	SET_NZVC8(in, in, out);
	return out;
}

static inline uint8_t op_dec(struct MC6809 *cpu, uint8_t in) {
	unsigned out = in - 1;
	CLR_NZV;
	SET_NZ8(out);
	if (out == 0x7f) REG_CC |= CC_V;
	return out;
}

static inline uint8_t op_inc(struct MC6809 *cpu, uint8_t in) {
	unsigned out = in + 1;
	CLR_NZV;
	SET_NZ8(out);
	if (out == 0x80) REG_CC |= CC_V;
	return out;
}

static inline uint8_t op_tst(struct MC6809 *cpu, uint8_t in) {
	CLR_NZV;
	SET_NZ8(in);
	return in;
}

static inline uint8_t op_clr(struct MC6809 *cpu, uint8_t in) {
	(void)in;
	CLR_NVC;
	REG_CC |= CC_Z;
	return 0;
}

/* 8-bit arithmetic operations */

static inline uint8_t op_sub(struct MC6809 *cpu, uint8_t a, uint8_t b) {
	unsigned out = a - b;
	CLR_NZVC;
	SET_NZVC8(a, b, out);
	return out;
}

static inline uint8_t op_sbc(struct MC6809 *cpu, uint8_t a, uint8_t b) {
	unsigned out = a - b - (REG_CC & CC_C);
	CLR_NZVC;
	SET_NZVC8(a, b, out);
	return out;
}

static inline uint8_t op_and(struct MC6809 *cpu, uint8_t a, uint8_t b) {
	unsigned out = a & b;
	CLR_NZV;
	SET_NZ8(out);
	return out;
}

static inline uint8_t op_ld(struct MC6809 *cpu, uint8_t a, uint8_t b) {
	(void)a;
	CLR_NZV;
	SET_NZ8(b);
	return b;
}

static inline uint8_t op_discard(struct MC6809 *cpu, uint8_t a, uint8_t b) {
	(void)b;
	CLR_NZV;
	REG_CC |= CC_N;
	return a;
}

static inline uint8_t op_eor(struct MC6809 *cpu, uint8_t a, uint8_t b) {
	unsigned out = a ^ b;
	CLR_NZV;
	SET_NZ8(out);
	return out;
}

static inline uint8_t op_adc(struct MC6809 *cpu, uint8_t a, uint8_t b) {
	unsigned out = a + b + (REG_CC & CC_C);
	CLR_HNZVC;
	SET_NZVC8(a, b, out);
	SET_H(a, b, out);
	return out;
}

static inline uint8_t op_or(struct MC6809 *cpu, uint8_t a, uint8_t b) {
	unsigned out = a | b;
	CLR_NZV;
	SET_NZ8(out);
	return out;
}

static inline uint8_t op_add(struct MC6809 *cpu, uint8_t a, uint8_t b) {
	unsigned out = a + b;
	CLR_HNZVC;
	SET_NZVC8(a, b, out);
	SET_H(a, b, out);
	return out;
}

/* 16-bit arithmetic operations */

static inline uint16_t op_sub16(struct MC6809 *cpu, uint16_t a, uint16_t b) {
	unsigned out = a - b;
	CLR_NZVC;
	SET_NZVC16(a, b, out);
	return out;
}

static inline uint16_t op_ld16(struct MC6809 *cpu, uint16_t a, uint16_t b) {
	(void)a;
	CLR_NZV;
	SET_NZ16(b);
	return b;
}

static inline uint16_t op_add16(struct MC6809 *cpu, uint16_t a, uint16_t b) {
	unsigned out = a + b;
	CLR_NZVC;
	SET_NZVC16(a, b, out);
	return out;
}

/* Branch condition from op-code */

static _Bool branch_cond(struct MC6809 *cpu, unsigned op) {
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

/* ------------------------------------------------------------------------- */

static uint16_t ea_direct(struct MC6809 *cpu) {
	unsigned ea = REG_DP << 8 | fetch_byte(REG_PC++);
	NVMA_CYCLE();
	return ea;
}

static uint16_t ea_extended(struct MC6809 *cpu) {
	unsigned ea = fetch_byte(REG_PC) << 8;
	ea |= fetch_byte(REG_PC+1);
	REG_PC += 2;
	NVMA_CYCLE();
	return ea;
}

static uint16_t ea_indexed(struct MC6809 *cpu) {
	unsigned ea;
	unsigned postbyte;
	uint16_t *reg;
	BYTE_IMMEDIATE(0, postbyte);
	switch ((postbyte >> 5) & 3) {
		case 0: reg = &REG_X; break;
		case 1: reg = &REG_Y; break;
		case 2: reg = &REG_U; break;
		case 3: reg = &REG_S; break;
		default: return 0;
	}
	if ((postbyte & 0x80) == 0) {
		peek_byte(REG_PC);
		NVMA_CYCLE();
		return *reg + sex5(postbyte);
	}
	switch (postbyte & 0x0f) {
		case 0x00: ea = *reg; *reg += 1; peek_byte(REG_PC); NVMA_CYCLE(); NVMA_CYCLE(); break;
		case 0x01: ea = *reg; *reg += 2; peek_byte(REG_PC); NVMA_CYCLE(); NVMA_CYCLE(); NVMA_CYCLE(); break;
		case 0x02: *reg -= 1; ea = *reg; peek_byte(REG_PC); NVMA_CYCLE(); NVMA_CYCLE(); break;
		case 0x03: *reg -= 2; ea = *reg; peek_byte(REG_PC); NVMA_CYCLE(); NVMA_CYCLE(); NVMA_CYCLE(); break;
		case 0x04: ea = *reg; peek_byte(REG_PC); break;
		case 0x05: ea = *reg + sex8(RREG_B); peek_byte(REG_PC); NVMA_CYCLE(); break;
		case 0x07: // illegal
		case 0x06: ea = *reg + sex8(RREG_A); peek_byte(REG_PC); NVMA_CYCLE(); break;
		case 0x08: BYTE_IMMEDIATE(0, ea); ea = sex8(ea) + *reg; NVMA_CYCLE(); break;
		case 0x09: WORD_IMMEDIATE(0, ea); ea = ea + *reg; NVMA_CYCLE(); NVMA_CYCLE(); NVMA_CYCLE(); break;
		case 0x0a: ea = REG_PC | 0xff; break;
		case 0x0b: ea = *reg + REG_D; peek_byte(REG_PC); peek_byte(REG_PC + 1); NVMA_CYCLE(); NVMA_CYCLE(); NVMA_CYCLE(); break;
		case 0x0c: BYTE_IMMEDIATE(0, ea); ea = sex8(ea) + REG_PC; NVMA_CYCLE(); break;
		case 0x0d: WORD_IMMEDIATE(0, ea); ea = ea + REG_PC; peek_byte(REG_PC); NVMA_CYCLE(); NVMA_CYCLE(); NVMA_CYCLE(); break;
		case 0x0e: ea = 0xffff; break; // illegal
		case 0x0f: WORD_IMMEDIATE(0, ea); NVMA_CYCLE(); break;
		default: ea = 0; break;
	}
	if (postbyte & 0x10) {
		unsigned tmp_ea = fetch_byte(ea) << 8;
		ea = tmp_ea | fetch_byte(ea + 1);
		NVMA_CYCLE();
	}
	return ea;
}

/* ------------------------------------------------------------------------- */

struct MC6809 *mc6809_new(void) {
	struct MC6809 *new = g_malloc(sizeof(struct MC6809));
	mc6809_init(new);
	return new;
}

void mc6809_init(struct MC6809 *cpu) {
	memset(cpu, 0, sizeof(struct MC6809));
	cpu->free = mc6809_free;
	cpu->reset = mc6809_reset;
	cpu->run = mc6809_run;
	cpu->jump = mc6809_jump;
	// External handlers
	cpu->read_cycle = dummy_read_cycle;
	cpu->write_cycle = dummy_write_cycle;
	mc6809_reset(cpu);
}

static void mc6809_free(struct MC6809 *cpu) {
	free(cpu);
}

static void mc6809_reset(struct MC6809 *cpu) {
	cpu->halt = cpu->nmi = 0;
	cpu->firq = cpu->irq = 0;
	cpu->halt_cycle = cpu->nmi_cycle = cpu->firq_cycle = cpu->irq_cycle = cpu->cycle = 0;
	cpu->nmi_armed = 0;
	cpu->state = mc6809_flow_reset;
}

/* Run CPU while cpu->running is true. */

static void mc6809_run(struct MC6809 *cpu) {

	while (cpu->running) {

		_Bool nmi_active = cpu->nmi && ((int)(cpu->cycle - cpu->nmi_cycle) >= 2);
		_Bool firq_active = cpu->firq && ((int)(cpu->cycle - cpu->firq_cycle) >= 2);
		_Bool irq_active = cpu->irq && ((int)(cpu->cycle - cpu->irq_cycle) >= 2);

		switch (cpu->state) {

		case mc6809_flow_reset:
			REG_DP = 0;
			REG_CC |= (CC_F | CC_I);
			cpu->nmi = 0;
			cpu->nmi_armed = 0;
			cpu->state = mc6809_flow_reset_check_halt;
			// fall through

		case mc6809_flow_reset_check_halt:
			if (cpu->halt) {
				NVMA_CYCLE();
				continue;
			}
			REG_PC = fetch_byte(MC6809_INT_VEC_RESET) << 8;
			REG_PC |= fetch_byte(MC6809_INT_VEC_RESET + 1);
			NVMA_CYCLE();
			cpu->state = mc6809_flow_label_a;
			continue;

		// done_instruction case for backwards-compatibility
		case mc6809_flow_done_instruction:
		case mc6809_flow_label_a:
			if (cpu->halt) {
				NVMA_CYCLE();
				continue;
			}
			cpu->state = mc6809_flow_label_b;
			// fall through

		case mc6809_flow_label_b:
			if (cpu->nmi_armed && nmi_active) {
				peek_byte(REG_PC);
				peek_byte(REG_PC);
				PUSH_IRQ_REGISTERS();
				cpu->state = mc6809_flow_dispatch_irq;
				continue;
			}
			if (!(REG_CC & CC_F) && firq_active) {
				peek_byte(REG_PC);
				peek_byte(REG_PC);
				PUSH_FIRQ_REGISTERS();
				cpu->state = mc6809_flow_dispatch_irq;
				continue;
			}
			if (!(REG_CC & CC_I) && irq_active) {
				peek_byte(REG_PC);
				peek_byte(REG_PC);
				PUSH_IRQ_REGISTERS();
				cpu->state = mc6809_flow_dispatch_irq;
				continue;
			}
			cpu->state = mc6809_flow_next_instruction;
			continue;

		case mc6809_flow_dispatch_irq:
			if (cpu->nmi_armed && nmi_active) {
				cpu->nmi = 0;
				REG_CC |= (CC_F | CC_I);
				TAKE_INTERRUPT(cpu->nmi, CC_F|CC_I, MC6809_INT_VEC_NMI);
				cpu->state = mc6809_flow_label_a;
				continue;
			}
			if (!(REG_CC & CC_F) && firq_active) {
				REG_CC |= (CC_F | CC_I);
				TAKE_INTERRUPT(cpu->firq, CC_F|CC_I, MC6809_INT_VEC_FIRQ);
				cpu->state = mc6809_flow_label_a;
				continue;
			}
			if (!(REG_CC & CC_I) && irq_active) {
				REG_CC |= CC_I;
				TAKE_INTERRUPT(cpu->irq, CC_I, MC6809_INT_VEC_IRQ);
				cpu->state = mc6809_flow_label_a;
				continue;
			}
			cpu->state = mc6809_flow_cwai_check_halt;
			continue;

		case mc6809_flow_cwai_check_halt:
			NVMA_CYCLE();
			if (cpu->halt) {
				continue;
			}
			cpu->state = mc6809_flow_dispatch_irq;
			continue;

		case mc6809_flow_sync:
			if (nmi_active || firq_active || irq_active) {
				NVMA_CYCLE();
				NVMA_CYCLE();
				INSTRUCTION_POSTHOOK();
				cpu->state = mc6809_flow_label_b;
				continue;
			}
			NVMA_CYCLE();
			if (cpu->halt)
				cpu->state = mc6809_flow_sync_check_halt;
			continue;

		case mc6809_flow_sync_check_halt:
			NVMA_CYCLE();
			if (!cpu->halt) {
				cpu->state = mc6809_flow_sync;
			}
			continue;

		case mc6809_flow_next_instruction:
			{
			unsigned op;
			// Instruction fetch hook
			if (cpu->instruction_hook) {
				cpu->instruction_hook(cpu);
			}
			// Fetch op-code and process
			BYTE_IMMEDIATE(0, op);
			switch (op) {

			// 0x00 - 0x0f direct mode ops
			// 0x40 - 0x4f inherent A register ops
			// 0x50 - 0x5f inherent B register ops
			// 0x60 - 0x6f indexed mode ops
			// 0x70 - 0x7f extended mode ops
			case 0x00: case 0x01: case 0x02: case 0x03:
			case 0x04: case 0x05: case 0x06: case 0x07:
			case 0x08: case 0x09: case 0x0a: case 0x0b:
			case 0x0c: case 0x0d: case 0x0f:
			case 0x40: case 0x41: case 0x42: case 0x43:
			case 0x44: case 0x45: case 0x46: case 0x47:
			case 0x48: case 0x49: case 0x4a: case 0x4b:
			case 0x4c: case 0x4d: case 0x4f:
			case 0x50: case 0x51: case 0x52: case 0x53:
			case 0x54: case 0x55: case 0x56: case 0x57:
			case 0x58: case 0x59: case 0x5a: case 0x5b:
			case 0x5c: case 0x5d: case 0x5f:
			case 0x60: case 0x61: case 0x62: case 0x63:
			case 0x64: case 0x65: case 0x66: case 0x67:
			case 0x68: case 0x69: case 0x6a: case 0x6b:
			case 0x6c: case 0x6d: case 0x6f:
			case 0x70: case 0x71: case 0x72: case 0x73:
			case 0x74: case 0x75: case 0x76: case 0x77:
			case 0x78: case 0x79: case 0x7a: case 0x7b:
			case 0x7c: case 0x7d: case 0x7f: {
				unsigned ea, tmp1;
				switch ((op >> 4) & 0xf) {
				case 0x0: BYTE_DIRECT(ea, tmp1); break;
				case 0x4: ea = 0; tmp1 = RREG_A; break;
				case 0x5: ea = 0; tmp1 = RREG_B; break;
				case 0x6: BYTE_INDEXED(ea, tmp1); break;
				case 0x7: BYTE_EXTENDED(ea, tmp1); break;
				default: ea = tmp1 = 0; break;
				}
				switch (op & 0xf) {
				case 0x1: // NEG illegal
				case 0x0: tmp1 = op_neg(cpu, tmp1); break; // NEG, NEGA, NEGB
				case 0x2: tmp1 = op_negcom(cpu, tmp1); break; // NEGCOM illegal
				case 0x3: tmp1 = op_com(cpu, tmp1); break; // COM, COMA, COMB
				case 0x5: // LSR illegal
				case 0x4: tmp1 = op_lsr(cpu, tmp1); break; // LSR, LSRA, LSRB
				case 0x6: tmp1 = op_ror(cpu, tmp1); break; // ROR, RORA, RORB
				case 0x7: tmp1 = op_asr(cpu, tmp1); break; // ASR, ASRA, ASRB
				case 0x8: tmp1 = op_asl(cpu, tmp1); break; // ASL, ASLA, ASLB
				case 0x9: tmp1 = op_rol(cpu, tmp1); break; // ROL, ROLA, ROLB
				case 0xb: // DEC illegal
				case 0xa: tmp1 = op_dec(cpu, tmp1); break; // DEC, DECA, DECB
				case 0xc: tmp1 = op_inc(cpu, tmp1); break; // INC, INCA, INCB
				case 0xd: tmp1 = op_tst(cpu, tmp1); break; // TST, TSTA, TSTB
				case 0xf: tmp1 = op_clr(cpu, tmp1); break; // CLR, CLRA, CLRB
				default: break;
				}
				switch (op & 0xf) {
				case 0xd: // TST
					NVMA_CYCLE();
					NVMA_CYCLE();
					break;
				default: // the rest need storing
					switch ((op >> 4) & 0xf) {
					default:
					case 0x0: case 0x6: case 0x7:
						NVMA_CYCLE();
						store_byte(ea, tmp1);
						break;
					case 0x4:
						WREG_A = tmp1;
						peek_byte(REG_PC);
						break;
					case 0x5:
						WREG_B = tmp1;
						peek_byte(REG_PC);
						break;
					}
				}
			} break;

			// 0x0e JMP direct
			// 0x6e JMP indexed
			// 0x7e JMP extended
			case 0x0e: case 0x6e: case 0x7e: {
				unsigned ea;
				switch ((op >> 4) & 0xf) {
				case 0x0: ea = ea_direct(cpu); break;
				case 0x6: ea = ea_indexed(cpu); break;
				case 0x7: ea = ea_extended(cpu); break;
				default: ea = 0; break;
				}
				REG_PC = ea;
			} break;

			// 0x10 Page 2
			case 0x10:
				cpu->state = mc6809_flow_instruction_page_2;
				continue;
			// 0x11 Page 3
			case 0x11:
				cpu->state = mc6809_flow_instruction_page_3;
				continue;
			// 0x12 NOP inherent
			case 0x12: peek_byte(REG_PC); break;
			// 0x13 SYNC inherent
			case 0x13:
				peek_byte(REG_PC);
				cpu->state = mc6809_flow_sync;
				continue;
			// 0x14, 0x15 HCF? (illegal)
			case 0x14:
			case 0x15:
				cpu->state = mc6809_flow_hcf;
				goto done_instruction;
			// 0x16 LBRA relative
			case 0x16: {
				unsigned ea;
				LONG_RELATIVE(ea);
				REG_PC += ea;
				NVMA_CYCLE();
				NVMA_CYCLE();
			} break;
			// 0x17 LBSR relative
			case 0x17: {
				unsigned ea;
				LONG_RELATIVE(ea);
				ea += REG_PC;
				NVMA_CYCLE();
				NVMA_CYCLE();
				NVMA_CYCLE();
				NVMA_CYCLE();
				PUSHWORD(REG_S, REG_PC);
				REG_PC = ea;
			} break;
			// 0x18 Shift CC with mask inherent (illegal)
			case 0x18:
				REG_CC = (REG_CC << 1) & (CC_H | CC_Z);
				NVMA_CYCLE();
				peek_byte(REG_PC);
				break;
			// 0x19 DAA inherent
			case 0x19: {
				unsigned tmp = 0;
				if ((RREG_A&0x0f) >= 0x0a || REG_CC & CC_H) tmp |= 0x06;
				if (RREG_A >= 0x90 && (RREG_A&0x0f) >= 0x0a) tmp |= 0x60;
				if (RREG_A >= 0xa0 || REG_CC & CC_C) tmp |= 0x60;
				tmp += RREG_A;
				WREG_A = tmp;
				// CC.C NOT cleared, only set if appropriate
				CLR_NZV;
				SET_NZC8(tmp);
				peek_byte(REG_PC);
			} break;
			// 0x1a ORCC immediate
			case 0x1a: {
				unsigned data;
				BYTE_IMMEDIATE(0, data);
				REG_CC |= data;
				peek_byte(REG_PC);
			} break;
			// 0x1b NOP inherent (illegal)
			case 0x1b: peek_byte(REG_PC); break;
			// 0x1c ANDCC immediate
			case 0x1c: {
				unsigned data;
				BYTE_IMMEDIATE(0, data);
				REG_CC &= data;
				peek_byte(REG_PC);
			} break;
			// 0x1d SEX inherent
			case 0x1d:
				WREG_A = (RREG_B & 0x80) ? 0xff : 0;
				CLR_NZ;
				SET_NZ16(REG_D);
				peek_byte(REG_PC);
				break;
			// 0x1e EXG immediate
			case 0x1e: {
				unsigned postbyte;
				unsigned tmp1, tmp2;
				BYTE_IMMEDIATE(0, postbyte);
				switch (postbyte >> 4) {
					case 0x0: tmp1 = REG_D; break;
					case 0x1: tmp1 = REG_X; break;
					case 0x2: tmp1 = REG_Y; break;
					case 0x3: tmp1 = REG_U; break;
					case 0x4: tmp1 = REG_S; break;
					case 0x5: tmp1 = REG_PC; break;
					case 0x8: tmp1 = RREG_A | 0xff00; break;
					case 0x9: tmp1 = RREG_B | 0xff00; break;
					// TODO: verify this behaviour
					case 0xa: tmp1 = (REG_CC << 8) | REG_CC; break;
					case 0xb: tmp1 = (REG_DP << 8) | REG_DP; break;
					default: tmp1 = 0xffff; break;
				}
				switch (postbyte & 0xf) {
					case 0x0: tmp2 = REG_D; REG_D = tmp1; break;
					case 0x1: tmp2 = REG_X; REG_X = tmp1; break;
					case 0x2: tmp2 = REG_Y; REG_Y = tmp1; break;
					case 0x3: tmp2 = REG_U; REG_U = tmp1; break;
					case 0x4: tmp2 = REG_S; REG_S = tmp1; break;
					case 0x5: tmp2 = REG_PC; REG_PC = tmp1; break;
					case 0x8: tmp2 = RREG_A | 0xff00; WREG_A = tmp1; break;
					case 0x9: tmp2 = RREG_B | 0xff00; WREG_B = tmp1; break;
					// TODO: verify this behaviour
					case 0xa: tmp2 = (REG_CC << 8) | REG_CC; REG_CC = tmp1; break;
					case 0xb: tmp2 = (REG_DP << 8) | REG_DP; REG_DP = tmp1; break;
					default: tmp2 = 0xffff; break;
				}
				switch (postbyte >> 4) {
					case 0x0: REG_D = tmp2; break;
					case 0x1: REG_X = tmp2; break;
					case 0x2: REG_Y = tmp2; break;
					case 0x3: REG_U = tmp2; break;
					case 0x4: REG_S = tmp2; break;
					case 0x5: REG_PC = tmp2; break;
					case 0x8: WREG_A = tmp2; break;
					case 0x9: WREG_B = tmp2; break;
					case 0xa: REG_CC = tmp2; break;
					case 0xb: REG_DP = tmp2; break;
					default: break;
				}
				NVMA_CYCLE();
				NVMA_CYCLE();
				NVMA_CYCLE();
				NVMA_CYCLE();
				NVMA_CYCLE();
				NVMA_CYCLE();
			} break;
			// 0x1f TFR immediate
			case 0x1f: {
				unsigned postbyte;
				unsigned tmp1;
				BYTE_IMMEDIATE(0, postbyte);
				switch (postbyte >> 4) {
					case 0x0: tmp1 = REG_D; break;
					case 0x1: tmp1 = REG_X; break;
					case 0x2: tmp1 = REG_Y; break;
					case 0x3: tmp1 = REG_U; break;
					case 0x4: tmp1 = REG_S; break;
					case 0x5: tmp1 = REG_PC; break;
					case 0x8: tmp1 = RREG_A | 0xff00; break;
					case 0x9: tmp1 = RREG_B | 0xff00; break;
					// TODO: verify this behaviour
					case 0xa: tmp1 = (REG_CC << 8) | REG_CC; break;
					case 0xb: tmp1 = (REG_DP << 8) | REG_DP; break;
					default: tmp1 = 0xffff; break;
				}
				switch (postbyte & 0xf) {
					case 0x0: REG_D = tmp1; break;
					case 0x1: REG_X = tmp1; break;
					case 0x2: REG_Y = tmp1; break;
					case 0x3: REG_U = tmp1; break;
					case 0x4: REG_S = tmp1; break;
					case 0x5: REG_PC = tmp1; break;
					case 0x8: WREG_A = tmp1; break;
					case 0x9: WREG_B = tmp1; break;
					case 0xa: REG_CC = tmp1; break;
					case 0xb: REG_DP = tmp1; break;
					default: break;
				}
				NVMA_CYCLE();
				NVMA_CYCLE();
				NVMA_CYCLE();
				NVMA_CYCLE();
			} break;

			// 0x20 - 0x2f short branches
			case 0x20: case 0x21: case 0x22: case 0x23:
			case 0x24: case 0x25: case 0x26: case 0x27:
			case 0x28: case 0x29: case 0x2a: case 0x2b:
			case 0x2c: case 0x2d: case 0x2e: case 0x2f: {
				unsigned tmp;
				BYTE_IMMEDIATE(0, tmp);
				tmp = sex8(tmp);
				NVMA_CYCLE();
				if (branch_cond(cpu, op))
					REG_PC += tmp;
			} break;

			// 0x30 LEAX indexed
			case 0x30: EA_INDEXED(REG_X);
				CLR_Z;
				SET_Z(REG_X);
				NVMA_CYCLE();
				break;
			// 0x31 LEAY indexed
			case 0x31: EA_INDEXED(REG_Y);
				CLR_Z;
				SET_Z(REG_Y);
				NVMA_CYCLE();
				break;
			// 0x32 LEAS indexed
			case 0x32: EA_INDEXED(REG_S);
				NVMA_CYCLE();
				cpu->nmi_armed = 1;  // XXX: Really?
				break;
			// 0x33 LEAU indexed
			case 0x33: EA_INDEXED(REG_U);
				NVMA_CYCLE();
				break;
			// 0x34 PSHS immediate
			case 0x34: PUSHR(REG_S, REG_U); break;
			// 0x35 PULS immediate
			case 0x35: PULLR(REG_S, REG_U); break;
			// 0x36 PSHU immediate
			case 0x36: PUSHR(REG_U, REG_S); break;
			// 0x37 PULU immediate
			case 0x37: PULLR(REG_U, REG_S); break;
			// 0x38 ANDCC immediate (illegal)
			case 0x38: {
				unsigned data;
				BYTE_IMMEDIATE(0, data);
				REG_CC &= data;
				peek_byte(REG_PC);
				/* Differs from legal 0x1c version by
				 * taking one more cycle: */
				NVMA_CYCLE();
			} break;
			// 0x39 RTS inherent
			case 0x39:
				peek_byte(REG_PC);
				PULLWORD(REG_S, REG_PC);
				NVMA_CYCLE();
				break;
			// 0x3a ABX inherent
			case 0x3a:
				REG_X += RREG_B;
				peek_byte(REG_PC);
				NVMA_CYCLE();
				break;
			// 0x3b RTI inherent
			case 0x3b:
				peek_byte(REG_PC);
				PULLBYTE(REG_S, REG_CC);
				if (REG_CC & CC_E) {
					PULLBYTE(REG_S, WREG_A);
					PULLBYTE(REG_S, WREG_B);
					PULLBYTE(REG_S, REG_DP);
					PULLWORD(REG_S, REG_X);
					PULLWORD(REG_S, REG_Y);
					PULLWORD(REG_S, REG_U);
					PULLWORD(REG_S, REG_PC);
				} else {
					PULLWORD(REG_S, REG_PC);
				}
				cpu->nmi_armed = 1;
				peek_byte(REG_S);
				break;
			// 0x3c CWAI immediate
			case 0x3c: {
				unsigned data;
				BYTE_IMMEDIATE(0, data);
				REG_CC &= data;
				peek_byte(REG_PC);
				NVMA_CYCLE();
				PUSH_IRQ_REGISTERS();
				NVMA_CYCLE();
				cpu->state = mc6809_flow_dispatch_irq;
				goto done_instruction;
			} break;
			// 0x3d MUL inherent
			case 0x3d: {
				unsigned tmp = RREG_A * RREG_B;
				REG_D = tmp;
				CLR_ZC;
				SET_Z(tmp);
				if (tmp & 0x80)
					REG_CC |= CC_C;
				peek_byte(REG_PC);
				NVMA_CYCLE();
				NVMA_CYCLE();
				NVMA_CYCLE();
				NVMA_CYCLE();
				NVMA_CYCLE();
				NVMA_CYCLE();
				NVMA_CYCLE();
				NVMA_CYCLE();
				NVMA_CYCLE();
			} break;
			// 0x3e RESET (illegal)
			case 0x3e:
				peek_byte(REG_PC);
				PUSH_IRQ_REGISTERS_NO_E();
				INSTRUCTION_POSTHOOK();
				TAKE_INTERRUPT(swi, CC_F|CC_I, MC6809_INT_VEC_RESET);
				cpu->state = mc6809_flow_label_a;
				continue;
			// 0x3f SWI inherent
			case 0x3f:
				peek_byte(REG_PC);
				PUSH_IRQ_REGISTERS();
				INSTRUCTION_POSTHOOK();
				TAKE_INTERRUPT(swi, CC_F|CC_I, MC6809_INT_VEC_SWI);
				cpu->state = mc6809_flow_label_a;
				continue;

			// 0x80 - 0xbf A register arithmetic ops
			// 0xc0 - 0xff B register arithmetic ops
			case 0x80: case 0x81: case 0x82:
			case 0x84: case 0x85: case 0x86: case 0x87:
			case 0x88: case 0x89: case 0x8a: case 0x8b:
			case 0x90: case 0x91: case 0x92:
			case 0x94: case 0x95: case 0x96:
			case 0x98: case 0x99: case 0x9a: case 0x9b:
			case 0xa0: case 0xa1: case 0xa2:
			case 0xa4: case 0xa5: case 0xa6:
			case 0xa8: case 0xa9: case 0xaa: case 0xab:
			case 0xb0: case 0xb1: case 0xb2:
			case 0xb4: case 0xb5: case 0xb6:
			case 0xb8: case 0xb9: case 0xba: case 0xbb:
			case 0xc0: case 0xc1: case 0xc2:
			case 0xc4: case 0xc5: case 0xc6: case 0xc7:
			case 0xc8: case 0xc9: case 0xca: case 0xcb:
			case 0xd0: case 0xd1: case 0xd2:
			case 0xd4: case 0xd5: case 0xd6:
			case 0xd8: case 0xd9: case 0xda: case 0xdb:
			case 0xe0: case 0xe1: case 0xe2:
			case 0xe4: case 0xe5: case 0xe6:
			case 0xe8: case 0xe9: case 0xea: case 0xeb:
			case 0xf0: case 0xf1: case 0xf2:
			case 0xf4: case 0xf5: case 0xf6:
			case 0xf8: case 0xf9: case 0xfa: case 0xfb: {
				unsigned ea, tmp1, tmp2;
				tmp1 = !(op & 0x40) ? RREG_A : RREG_B;
				switch ((op >> 4) & 3) {
				case 0: BYTE_IMMEDIATE(0, tmp2); break;
				case 1: BYTE_DIRECT(ea, tmp2); break;
				case 2: BYTE_INDEXED(ea, tmp2); break;
				case 3: BYTE_EXTENDED(ea, tmp2); break;
				default: tmp2 = 0; break;
				}
				switch (op & 0xf) {
				case 0x0: tmp1 = op_sub(cpu, tmp1, tmp2); break; // SUBA, SUBB
				case 0x1: (void)op_sub(cpu, tmp1, tmp2); break; // CMPA, CMPB
				case 0x2: tmp1 = op_sbc(cpu, tmp1, tmp2); break; // SBCA, SBCB
				case 0x4: tmp1 = op_and(cpu, tmp1, tmp2); break; // ANDA, ANDB
				case 0x5: (void)op_and(cpu, tmp1, tmp2); break; // BITA, BITB
				case 0x6: tmp1 = op_ld(cpu, 0, tmp2); break; // LDA, LDB
				case 0x7: tmp1 = op_discard(cpu, tmp1, tmp2); break; // illegal
				case 0x8: tmp1 = op_eor(cpu, tmp1, tmp2); break; // EORA, EORB
				case 0x9: tmp1 = op_adc(cpu, tmp1, tmp2); break; // ADCA, ADCB
				case 0xa: tmp1 = op_or(cpu, tmp1, tmp2); break; // ORA, ORB
				case 0xb: tmp1 = op_add(cpu, tmp1, tmp2); break; // ADDA, ADDB
				default: break;
				}
				if (!(op & 0x40)) {
					WREG_A = tmp1;
				} else {
					WREG_B = tmp1;
				}
			} break;

			// 0x83, 0x93, 0xa3, 0xb3 SUBD
			// 0x8c, 0x9c, 0xac, 0xbc CMPX
			// 0xc3, 0xd3, 0xe3, 0xf3 ADDD
			case 0x83: case 0x93: case 0xa3: case 0xb3:
			case 0x8c: case 0x9c: case 0xac: case 0xbc:
			case 0xc3: case 0xd3: case 0xe3: case 0xf3: {
				unsigned ea, tmp1, tmp2;
				tmp1 = !(op & 0x08) ? REG_D : REG_X;
				switch ((op >> 4) & 3) {
				case 0: WORD_IMMEDIATE(0, tmp2); break;
				case 1: WORD_DIRECT(ea, tmp2); break;
				case 2: WORD_INDEXED(ea, tmp2); break;
				case 3: WORD_EXTENDED(ea, tmp2); break;
				default: tmp2 = 0; break;
				}
				switch (op & 0x4f) {
				case 0x03: tmp1 = op_sub16(cpu, tmp1, tmp2); break; // SUBD
				case 0x0c: (void)op_sub16(cpu, tmp1, tmp2); break; // CMPX
				case 0x43: tmp1 = op_add16(cpu, tmp1, tmp2); break; // ADDD
				default: break;
				}
				NVMA_CYCLE();
				if (!(op & 0x08)) {
					REG_D = tmp1;
				}
			} break;

			// 0x8d BSR
			// 0x9d, 0xad, 0xbd JSR
			case 0x8d: case 0x9d: case 0xad: case 0xbd: {
				unsigned ea;
				switch ((op >> 4) & 3) {
				case 0: SHORT_RELATIVE(ea); ea += REG_PC; NVMA_CYCLE(); NVMA_CYCLE(); NVMA_CYCLE(); break;
				case 1: EA_DIRECT(ea); peek_byte(ea); NVMA_CYCLE(); break;
				case 2: EA_INDEXED(ea); peek_byte(ea); NVMA_CYCLE(); break;
				case 3: EA_EXTENDED(ea); peek_byte(ea); NVMA_CYCLE(); break;
				default: ea = 0; break;
				}
				PUSHWORD(REG_S, REG_PC);
				REG_PC = ea;
			} break;

			// 0x8e, 0x9e, 0xae, 0xbe LDX
			// 0xcc, 0xdc, 0xec, 0xfc LDD
			// 0xce, 0xde, 0xee, 0xfe LDU
			case 0x8e: case 0x9e: case 0xae: case 0xbe:
			case 0xcc: case 0xdc: case 0xec: case 0xfc:
			case 0xce: case 0xde: case 0xee: case 0xfe: {
				unsigned ea, tmp1, tmp2;
				switch ((op >> 4) & 3) {
				case 0: WORD_IMMEDIATE(0, tmp2); break;
				case 1: WORD_DIRECT(ea, tmp2); break;
				case 2: WORD_INDEXED(ea, tmp2); break;
				case 3: WORD_EXTENDED(ea, tmp2); break;
				default: tmp2 = 0; break;
				}
				tmp1 = op_ld16(cpu, 0, tmp2);
				switch (op & 0x42) {
				case 0x02:
					REG_X = tmp1;
					break;
				case 0x40:
					REG_D = tmp1;
					break;
				case 0x42:
					REG_U = tmp1;
					break;
				default: break;
				}
			} break;

			// 0x97, 0xa7, 0xb7 STA
			// 0xd7, 0xe7, 0xf7 STB
			case 0x97: case 0xa7: case 0xb7:
			case 0xd7: case 0xe7: case 0xf7: {
				unsigned ea, tmp1;
				tmp1 = !(op & 0x40) ? RREG_A : RREG_B;
				switch ((op >> 4) & 3) {
				case 1: EA_DIRECT(ea); break;
				case 2: EA_INDEXED(ea); break;
				case 3: EA_EXTENDED(ea); break;
				default: ea = 0; break;
				}
				store_byte(ea, tmp1);
				CLR_NZV;
				SET_NZ8(tmp1);
			} break;

			// 0x9f, 0xaf, 0xbf STX
			// 0xdd, 0xed, 0xfd STD
			// 0xdf, 0xef, 0xff STU
			case 0x9f: case 0xaf: case 0xbf:
			case 0xdd: case 0xed: case 0xfd:
			case 0xdf: case 0xef: case 0xff: {
				unsigned ea, tmp1;
				switch (op & 0x42) {
				case 0x02: tmp1 = REG_X; break;
				case 0x40: tmp1 = REG_D; break;
				case 0x42: tmp1 = REG_U; break;
				default: tmp1 = 0; break;
				}
				switch ((op >> 4) & 3) {
				case 1: EA_DIRECT(ea); break;
				case 2: EA_INDEXED(ea); break;
				case 3: EA_EXTENDED(ea); break;
				default: ea = 0; break;
				}
				CLR_NZV;
				SET_NZ16(tmp1);
				store_byte(ea, tmp1 >> 8);
				store_byte(ea+1, tmp1);
			} break;

			// 0x8f STX immediate (illegal)
			// 0xcf STU immediate (illegal)
			// Illegal instruction only part working
			case 0x8f: case 0xcf: {
				unsigned tmp1;
				tmp1 = !(op & 0x40) ? REG_X : REG_U;
				(void)fetch_byte(REG_PC);
				REG_PC++;
				store_byte(REG_PC, tmp1);
				REG_PC++;
				CLR_NZV;
				REG_CC |= CC_N;
			} break;

			// 0xcd HCF? (illegal)
			case 0xcd:
				cpu->state = mc6809_flow_hcf;
				goto done_instruction;

			// Illegal instruction
			default:
				NVMA_CYCLE();
				break;
			}
			cpu->state = mc6809_flow_label_a;
			goto done_instruction;
			}

		case mc6809_flow_instruction_page_2:
			{
			unsigned op;
			BYTE_IMMEDIATE(0, op);
			switch (op) {

			// 0x10, 0x11 Page 2
			case 0x10:
			case 0x11:
				cpu->state = mc6809_flow_instruction_page_2;
				continue;

			// 0x1020 - 0x102f long branches
			case 0x20: case 0x21: case 0x22: case 0x23:
			case 0x24: case 0x25: case 0x26: case 0x27:
			case 0x28: case 0x29: case 0x2a: case 0x2b:
			case 0x2c: case 0x2d: case 0x2e: case 0x2f: {
				unsigned tmp;
				WORD_IMMEDIATE(0, tmp);
				if (branch_cond(cpu, op)) {
					REG_PC += tmp;
					NVMA_CYCLE();
				}
				NVMA_CYCLE();
			} break;

			// 0x103f SWI2 inherent
			case 0x3f:
				peek_byte(REG_PC);
				PUSH_IRQ_REGISTERS();
				INSTRUCTION_POSTHOOK();
				TAKE_INTERRUPT(swi2, 0, MC6809_INT_VEC_SWI2);
				cpu->state = mc6809_flow_label_a;
				continue;

			// 0x1083, 0x1093, 0x10a3, 0x10b3 CMPD
			// 0x108c, 0x109c, 0x10ac, 0x10bc CMPY
			case 0x83: case 0x93: case 0xa3: case 0xb3:
			case 0x8c: case 0x9c: case 0xac: case 0xbc: {
				unsigned ea, tmp1, tmp2;
				tmp1 = !(op & 0x08) ? REG_D : REG_Y;
				switch ((op >> 4) & 3) {
				case 0: WORD_IMMEDIATE(0, tmp2); break;
				case 1: WORD_DIRECT(ea, tmp2); break;
				case 2: WORD_INDEXED(ea, tmp2); break;
				case 3: WORD_EXTENDED(ea, tmp2); break;
				default: tmp2 = 0; break;
				}
				(void)op_sub16(cpu, tmp1, tmp2);
				NVMA_CYCLE();
			} break;

			// 0x108e, 0x109e, 0x10ae, 0x10be LDY
			// 0x10ce, 0x10de, 0x10ee, 0x10fe LDS
			case 0x8e: case 0x9e: case 0xae: case 0xbe:
			case 0xce: case 0xde: case 0xee: case 0xfe: {
				unsigned ea, tmp1, tmp2;
				switch ((op >> 4) & 3) {
				case 0: WORD_IMMEDIATE(0, tmp2); break;
				case 1: WORD_DIRECT(ea, tmp2); break;
				case 2: WORD_INDEXED(ea, tmp2); break;
				case 3: WORD_EXTENDED(ea, tmp2); break;
				default: tmp2 = 0; break;
				}
				tmp1 = op_ld16(cpu, 0, tmp2);
				if (!(op & 0x40)) {
					REG_Y = tmp1;
				} else {
					REG_S = tmp1;
					cpu->nmi_armed = 1;
				}
			} break;

			// 0x109f, 0x10af, 0x10bf STY
			// 0x10df, 0x10ef, 0x10ff STS
			case 0x9f: case 0xaf: case 0xbf:
			case 0xdf: case 0xef: case 0xff: {
				unsigned ea, tmp1;
				tmp1 = !(op & 0x40) ? REG_Y : REG_S;
				switch ((op >> 4) & 3) {
				case 1: EA_DIRECT(ea); break;
				case 2: EA_INDEXED(ea); break;
				case 3: EA_EXTENDED(ea); break;
				default: ea = 0; break;
				}
				CLR_NZV;
				SET_NZ16(tmp1);
				store_byte(ea, tmp1 >> 8);
				store_byte(ea+1, tmp1);
			} break;

			// Illegal instruction
			default:
				NVMA_CYCLE();
				break;
			}
			cpu->state = mc6809_flow_label_a;
			goto done_instruction;
			}

		case mc6809_flow_instruction_page_3:
			{
			unsigned op;
			BYTE_IMMEDIATE(0, op);
			switch (op) {

			// 0x10, 0x11 Page 3
			case 0x10:
			case 0x11:
				cpu->state = mc6809_flow_instruction_page_3;
				continue;

			// 0x113F SWI3 inherent
			case 0x3f:
				peek_byte(REG_PC);
				PUSH_IRQ_REGISTERS();
				INSTRUCTION_POSTHOOK();
				TAKE_INTERRUPT(swi3, 0, MC6809_INT_VEC_SWI3);
				cpu->state = mc6809_flow_label_a;
				continue;

			// 0x1183, 0x1193, 0x11a3, 0x11b3 CMPU
			// 0x118c, 0x119c, 0x11ac, 0x11bc CMPS
			case 0x83: case 0x93: case 0xa3: case 0xb3:
			case 0x8c: case 0x9c: case 0xac: case 0xbc: {
				unsigned ea, tmp1, tmp2;
				tmp1 = !(op & 0x08) ? REG_U : REG_S;
				switch ((op >> 4) & 3) {
				case 0: WORD_IMMEDIATE(0, tmp2); break;
				case 1: WORD_DIRECT(ea, tmp2); break;
				case 2: WORD_INDEXED(ea, tmp2); break;
				case 3: WORD_EXTENDED(ea, tmp2); break;
				default: tmp2 = 0; break;
				}
				(void)op_sub16(cpu, tmp1, tmp2);
				NVMA_CYCLE();
			} break;

			// Illegal instruction
			default:
				NVMA_CYCLE();
				break;
			}
			cpu->state = mc6809_flow_label_a;
			goto done_instruction;
			}

		// Certain illegal instructions cause the CPU to lock up:
		case mc6809_flow_hcf:
			NVMA_CYCLE();
			continue;

		}

done_instruction:
		INSTRUCTION_POSTHOOK();
		continue;

	}

}

static void mc6809_jump(struct MC6809 *cpu, uint16_t pc) {
	REG_PC = pc;
}
