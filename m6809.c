/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2005  Ciaran Anscomb
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "xroar.h"
#include "m6809.h"
#include "sam.h"
#include "pia.h"
#include "logging.h"
#include "tape.h"

/* Condition Code manipulation macros */

#define CC_E 0x80
#define CC_F 0x40
#define CC_H 0x20
#define CC_I 0x10
#define CC_N 0x08
#define CC_Z 0x04
#define CC_V 0x02
#define CC_C 0x01
#define N_EOR_V	((reg_cc & CC_N)^((reg_cc & CC_V)<<2))

#define CLR_HNZVC	reg_cc &= ~(CC_H|CC_N|CC_Z|CC_V|CC_C)
#define CLR_NZV		reg_cc &= ~(CC_N|CC_Z|CC_V)
#define CLR_NZVC	reg_cc &= ~(CC_N|CC_Z|CC_V|CC_C)
#define CLR_Z		reg_cc &= ~(CC_Z)
#define CLR_NZC		reg_cc &= ~(CC_N|CC_Z|CC_C)
#define CLR_NVC		reg_cc &= ~(CC_N|CC_V|CC_C)
#define CLR_ZC		reg_cc &= ~(CC_Z|CC_C)

#define SET_Z(a)	if (!(a)) reg_cc |= CC_Z;
#define SET_N8(a)	reg_cc |= (a&0x80)>>4;
#define SET_N16(a)	reg_cc |= (a&0x8000)>>12;
#define SET_H(a,b,r)	reg_cc |= ((a^b^r)&0x10)<<1;
#define SET_C8(a)	reg_cc |= (a&0x100)>>8;
#define SET_C16(a)	reg_cc |= (a&0x10000)>>16;
#define SET_V8(a,b,r)	reg_cc |= ((a^b^r^(r>>1))&0x80)>>6;
#define SET_V16(a,b,r)  reg_cc |= ((a^b^r^(r>>1))&0x8000)>>14;
#define SET_NZ8(a)		{ SET_N8(a); SET_Z(a&0xff); }
#define SET_NZ16(a)		{ SET_N16(a);SET_Z(a&0xffff); }
#define SET_NZC8(a)		{ SET_N8(a); SET_Z(a&0xff);SET_C8(a); }
#define SET_NZVC8(a,b,r)	{ SET_N8(r); SET_Z(r&0xff);SET_V8(a,b,r);SET_C8(r); }
#define SET_NZVC16(a,b,r)	{ SET_N16(r);SET_Z(r&0xffff);SET_V16(a,b,r);SET_C16(r); }

/* CPU fetch/store goes via SAM */
#define fetch_byte(a) sam_read_byte(((a)&0xffff))
#define fetch_word(a) (fetch_byte(a) << 8 | fetch_byte((a)+1))
#define store_byte(a,v) sam_store_byte(((a)&0xffff),(v))

#define EA_DIRECT(a)	{ a = reg_dp << 8 | fetch_byte(reg_pc); reg_pc += 1; if (trace) { LOG_DEBUG(0, "%02x", (a) & 0xff); } }
#define EA_INDEXED(a)	{ a = addr_indexed(); }
#define EA_EXTENDED(a)	{ a = fetch_byte(reg_pc) << 8 | fetch_byte(reg_pc+1); reg_pc += 2; if (trace) { LOG_DEBUG(0, "%04x", a); } }

/* These macros are designed to be "passed as an argument" to the op-code
 * macros.  Must be used carefully, as some of them declare an 'addr' variable
 */
#define BYTE_IMMEDIATE(a,v)	{ v = fetch_byte(reg_pc); reg_pc++; if (trace) { LOG_DEBUG(0, "%02x", v); } }
#define BYTE_DIRECT(a,v)	uint_least16_t addr; { EA_DIRECT(a); v = fetch_byte(a); }
#define BYTE_INDEXED(a,v)	uint_least16_t addr; { EA_INDEXED(a); v = fetch_byte(a); }
#define BYTE_EXTENDED(a,v)	uint_least16_t addr; { EA_EXTENDED(a); v = fetch_byte(a); }
#define WORD_IMMEDIATE(a,v)	{ v = fetch_byte(reg_pc) << 8 | fetch_byte(reg_pc+1); reg_pc += 2; if (trace) { LOG_DEBUG(0, "%04x", v); } }
#define WORD_DIRECT(a,v)	uint_least16_t addr; { EA_DIRECT(a); v = fetch_byte(a) << 8 | fetch_byte(a+1); }
#define WORD_INDEXED(a,v)	uint_least16_t addr; { EA_INDEXED(a); v = fetch_byte(a) << 8 | fetch_byte(a+1); }
#define WORD_EXTENDED(a,v)	uint_least16_t addr; { EA_EXTENDED(a); v = fetch_byte(a) << 8 | fetch_byte(a+1); }

#define SHORT_RELATIVE(r)	{ BYTE_IMMEDIATE(0,r); r = sex(r); }
#define LONG_RELATIVE(r)	WORD_IMMEDIATE(0,r)

#define PUSHWORD(s,r)	{ s -= 2; store_byte(s+1, r & 0xff); store_byte(s, r >> 8); }
#define PUSHBYTE(s,r)	{ s--; store_byte(s, r); }
#define PULLBYTE(s,r)	{ r = fetch_byte(s); s++; }
#define PULLWORD(s,r)	{ r = fetch_byte(s) << 8; r |= fetch_byte(s+1); s += 2; }

#define PUSHR(s,a) { \
		unsigned int postbyte; \
		uint_least16_t cyc = 5; \
		BYTE_IMMEDIATE(0,postbyte); \
		if (postbyte & 0x80) { PUSHWORD(s, reg_pc); cyc += 2; } \
		if (postbyte & 0x40) { PUSHWORD(s, a); cyc += 2; } \
		if (postbyte & 0x20) { PUSHWORD(s, reg_y); cyc += 2; } \
		if (postbyte & 0x10) { PUSHWORD(s, reg_x); cyc += 2; } \
		if (postbyte & 0x08) { PUSHBYTE(s, reg_dp); cyc++; } \
		if (postbyte & 0x04) { PUSHBYTE(s, reg_b); cyc++; } \
		if (postbyte & 0x02) { PUSHBYTE(s, reg_a); cyc++; } \
		if (postbyte & 0x01) { PUSHBYTE(s, reg_cc); cyc++; } \
		TAKEN_CYCLES(cyc); \
	}

#define PULLR(s,a) { \
		unsigned int postbyte; \
		uint_least16_t cyc = 5; \
		BYTE_IMMEDIATE(0,postbyte); \
		if (postbyte & 0x01) { PULLBYTE(s, reg_cc); cyc++; } \
		if (postbyte & 0x02) { PULLBYTE(s, reg_a); cyc++; } \
		if (postbyte & 0x04) { PULLBYTE(s, reg_b); cyc++; } \
		if (postbyte & 0x08) { PULLBYTE(s, reg_dp); cyc++; } \
		if (postbyte & 0x10) { PULLWORD(s, reg_x); cyc += 2; } \
		if (postbyte & 0x20) { PULLWORD(s, reg_y); cyc += 2; } \
		if (postbyte & 0x40) { PULLWORD(s, a); cyc += 2; } \
		if (postbyte & 0x80) { PULLWORD(s, reg_pc); cyc += 2; } \
		TAKEN_CYCLES(cyc); \
	}

#define TAKEN_CYCLES(c) current_cycle += ((c) * CPU_RATE_DIVISOR);
#define TOOK_CYCLES(c) TAKEN_CYCLES(c); return;

#define INTERRUPT_REGISTER_PUSH(e) \
	{ \
		PUSHWORD(reg_s, reg_pc); \
		if (e) { \
			reg_cc |= CC_E; \
			PUSHWORD(reg_s, reg_u); \
			PUSHWORD(reg_s, reg_y); \
			PUSHWORD(reg_s, reg_x); \
			PUSHBYTE(reg_s, reg_dp); \
			PUSHBYTE(reg_s, reg_b); \
			PUSHBYTE(reg_s, reg_a); \
		} else { reg_cc &= ~CC_E; } \
		PUSHBYTE(reg_s, reg_cc); \
	}

#define TEST_INTERRUPT(i,m,v,e,cm,cyc,nom) \
	if (i) { \
		i = 0; \
		if (!skip_register_push) \
			wait_for_interrupt = 0; \
		if (v == 0xfffc) LOG_DEBUG(4,"M6809: Interrupt: %s\n", nom); \
		if (!(reg_cc & m)) { \
			wait_for_interrupt = 0; \
			if (!skip_register_push) INTERRUPT_REGISTER_PUSH(e); \
			skip_register_push = 0; \
			reg_cc |= (cm); reg_pc = fetch_word(v); \
			TAKEN_CYCLES(cyc); \
		} \
	}

/* MPU registers */
static uint8_t reg_cc, reg_dp;
static uint16_t	reg_x, reg_y, reg_u, reg_s;
static uint16_t	reg_pc;
static accumulator_t reg_accum;

/* MPU interrupt state variables */
unsigned int nmi, firq, irq;
static unsigned int wait_for_interrupt;
static unsigned int skip_register_push;

#define sex5(v) (uint32_t)((((v)&0x10)?0xfffffff0:0x00000000)|((v)&0x0f))
static inline uint32_t sex(unsigned int reg);

static inline void switched_block(void);
static void switched_block_page2(void);
static void switched_block_page3(void);

/* ------------------------------------------------------------------------- */

static uint_least16_t addr_indexed(void) {
	unsigned int postbyte;
	uint_least16_t ead, tmp_word;
	uint16_t *word_ptr;
	BYTE_IMMEDIATE(0,postbyte);
	switch ((postbyte & 0x60)>>5) {
		default:
		case 0: word_ptr = &reg_x; tmp_word = reg_x; break;
		case 1: word_ptr = &reg_y; tmp_word = reg_y; break;
		case 2: word_ptr = &reg_u; tmp_word = reg_u; break;
		case 3: word_ptr = &reg_s; tmp_word = reg_s; break;
	}
	if (!(postbyte & 0x80)) {
		TAKEN_CYCLES(1);
		return tmp_word + sex5(postbyte);
	}
	switch (postbyte & 0x0f) {
		case 0x00:  /* ,R+ */
			ead = tmp_word;
			tmp_word += 1;
			TAKEN_CYCLES(2);
			break;
		case 0x01:  /* ,R++ */
			ead = tmp_word;
			tmp_word += 2;
			TAKEN_CYCLES(3);
			break;
		case 0x02:  /* ,-R */
			tmp_word -= 1;
			ead = tmp_word;
			TAKEN_CYCLES(2);
			break;
		case 0x03:  /* ,--R */
			tmp_word -= 2;
			ead = tmp_word;
			TAKEN_CYCLES(3);
			break;
		case 0x04:  /* ,R */
			ead = tmp_word;
			TAKEN_CYCLES(0);
			break;
		case 0x05:  /* B,R */
			ead = tmp_word + sex(reg_b);
			TAKEN_CYCLES(1);
			break;
		case 0x06:  /* A,R */
			ead = tmp_word + sex(reg_a);
			TAKEN_CYCLES(1);
			break;
		case 0x08:  /* 8n,R */
			BYTE_IMMEDIATE(0,ead); ead = sex(ead) + tmp_word;
			TAKEN_CYCLES(1);
			break;
		case 0x09:  /* 16n, R */
			WORD_IMMEDIATE(0,ead); ead += tmp_word;
			TAKEN_CYCLES(4);
			break;
		case 0x0b:  /* D,R */
			ead = tmp_word + reg_d;
			TAKEN_CYCLES(4);
			break;
		case 0x0c:  /* 8n,PCR */
			BYTE_IMMEDIATE(0,ead); ead = sex(ead) + reg_pc;
			TAKEN_CYCLES(1);
			break;
		case 0x0d:  /* 16n,PCR */
			WORD_IMMEDIATE(0,ead); ead += reg_pc;
			TAKEN_CYCLES(5);
			break;
		case 0x0f:
			WORD_IMMEDIATE(0,ead);
			TAKEN_CYCLES(2);
			break;
		default:
			ead = 0;
			break;
	}
	*word_ptr = tmp_word;
	if (postbyte & 0x10) {
		TAKEN_CYCLES(3);
		return fetch_byte(ead) << 8 | fetch_byte(ead+1);
	}
	return ead;
}

/* ------------------------------------------------------------------------- */

static inline uint32_t sex(unsigned int reg) {
	return ((reg&0x80)?0xffffff80:0x00000000)|(reg&0x7f);
}

/* ------------------------------------------------------------------------- */

void m6809_init(void) {
}

void m6809_reset(void) {
	reg_cc = 0xff;
	reg_dp = reg_d = reg_x = reg_y = reg_u = reg_s = 0;
	nmi = firq = irq = 0;
	wait_for_interrupt = 0;
	skip_register_push = 0;
	reg_pc = fetch_word(0xfffe);
}

void m6809_cycle(Cycle until) {
	TEST_INTERRUPT(nmi, 0, 0xfffc, CC_E, CC_F|CC_I, 19, "NMI");
	TEST_INTERRUPT(firq, CC_F, 0xfff6, 0, CC_F|CC_I, 10, "FIRQ");
	TEST_INTERRUPT(irq, CC_I, 0xfff8, CC_E, CC_I, 19, "IRQ");
	if (wait_for_interrupt) {
		current_cycle = until;
		return;
	}
	while ((int)(current_cycle - until) < 0 && !wait_for_interrupt && !(nmi||firq||irq)) {
		/* Trap cassette ROM routines - hack! */
		if (reg_pc == brk_csrdon) {
			PULLWORD(reg_s, reg_pc);
		}
		if (reg_pc == brk_bitin) {
			reg_cc &= ~CC_C;
			reg_cc |= tape_read_bit();
			PULLWORD(reg_s, reg_pc);
		}
		if (trace) LOG_DEBUG(0, "%04x| ", reg_pc);
		switched_block();
		if (trace) LOG_DEBUG(0, "\tcc=%02x a=%02x b=%02x dp=%02x x=%04x y=%04x u=%04x s=%04x\n", reg_cc, reg_a, reg_b, reg_dp, reg_x, reg_y, reg_u, reg_s);
	}
	if (wait_for_interrupt) {
		LOG_DEBUG(4,"M6809: Waiting for interrupt\n");
	}
}

void m6809_get_registers(uint8_t *regs) {
	regs[0] = reg_cc;
	regs[1] = reg_a;
	regs[2] = reg_b;
	regs[3] = reg_dp;
	regs[4] = reg_x >> 8; regs[5] = reg_x & 0xff;
	regs[6] = reg_y >> 8; regs[7] = reg_y & 0xff;
	regs[8] = reg_u >> 8; regs[9] = reg_u & 0xff;
	regs[10] = reg_s >> 8; regs[11] = reg_s & 0xff;
	regs[12] = reg_pc >> 8; regs[13] = reg_pc & 0xff;
}

void m6809_set_registers(uint8_t *regs) {
	reg_cc = regs[0];
	reg_a = regs[1];
	reg_b = regs[2];
	reg_dp = regs[3];
	reg_x = regs[4] << 8 | regs[5];
	reg_y = regs[6] << 8 | regs[7];
	reg_u = regs[8] << 8 | regs[9];
	reg_s = regs[10] << 8 | regs[11];
	reg_pc = regs[12] << 8 | regs[13];
}

void m6809_jump(uint_least16_t pc) {
	reg_pc = pc & 0xffff;
	//wait_for_interrupt = 0;
	//irq = firq = nmi = 0;
}

/* ------------------------------------------------------------------------- */

/* Operation templates */

#define OP_NEG(a) { uint_least16_t octet, result; a(addr,octet); result = ~(octet) + 1; CLR_NZVC; SET_NZVC8(0, octet, result); store_byte(addr,result); }
#define OP_COM(a) { uint_least16_t octet; a(addr,octet); octet = ~(octet); CLR_NZV; SET_NZ8(octet); reg_cc |= CC_C; store_byte(addr,octet); }
#define OP_LSR(a) { uint_least16_t octet; a(addr,octet); CLR_NZC; reg_cc |= (octet & CC_C); octet &= 0xff; octet >>= 1; SET_Z(octet); store_byte(addr,octet); }
#define OP_ROR(a) { uint_least16_t octet, result; a(addr,octet); result = (reg_cc & CC_C) << 7; CLR_NZC; reg_cc |= octet & CC_C; result |= (octet & 0xff) >> 1; SET_NZ8(result); store_byte(addr,result); }
#define OP_ASR(a) { uint_least16_t octet; CLR_NZC; a(addr,octet); reg_cc |= (octet & CC_C); octet = (octet & 0x80) | ((octet & 0xff) >> 1); SET_NZ8(octet); store_byte(addr,octet); }
#define OP_ASL(a) { uint_least16_t octet,result; a(addr,octet); result = octet << 1; CLR_NZVC; SET_NZVC8(octet, octet, result); store_byte(addr,result); }
#define OP_ROL(a) { uint_least16_t octet,result; a(addr,octet); result = (octet<<1)|(reg_cc & 1); CLR_NZVC; SET_NZVC8(octet, octet, result); store_byte(addr,result); }
#define OP_DEC(a) { uint_least16_t octet; a(addr,octet); octet--; octet &= 0xff; CLR_NZV; SET_NZ8(octet); if (octet == 0x7f) reg_cc |= CC_V; store_byte(addr,octet); }
#define OP_INC(a) { uint_least16_t octet; a(addr,octet); octet++; octet &= 0xff; CLR_NZV; SET_NZ8(octet); if (octet == 0x80) reg_cc |= CC_V; store_byte(addr,octet); }
#define OP_TST(a) { uint_least16_t octet; a(addr,octet); CLR_NZV; SET_NZ8(octet); }
#define OP_JMP(a) { uint_least16_t addr; a(addr); reg_pc = addr; }
#define OP_CLR(a) { uint_least16_t octet; a(addr,octet); store_byte(addr,0); CLR_NVC; reg_cc |= CC_Z; }

#define OP_NEGR(r) { uint_least16_t result = ~(r) + 1; CLR_NZVC; SET_NZVC8(0, r, result); r = result; }
#define OP_COMR(r) { r = ~(r); CLR_NZV; SET_NZ8(r); reg_cc |= CC_C; }
#define OP_LSRR(r) { CLR_NZC; reg_cc |= (r & CC_C); r >>= 1; SET_Z(r); }
#define OP_RORR(r) { uint_least16_t result = (reg_cc & CC_C) << 7; CLR_NZC; reg_cc |= r & CC_C; result |= (r & 0xff) >> 1; SET_NZ8(result); r = result; }
#define OP_ASRR(r) { CLR_NZC; reg_cc |= (r & CC_C); r = (r & 0x80) | ((r & 0xff) >> 1); SET_NZ8(r); }
#define OP_ASLR(r) { uint_least16_t result = r << 1; CLR_NZVC; SET_NZVC8(r, r, result); r = result; }
#define OP_ROLR(r) { uint_least16_t result = (reg_cc & CC_C) | (r << 1); CLR_NZVC; SET_NZVC8(r, r, result); r = result; }
#define OP_DECR(r) { r--; r &= 0xff; CLR_NZV; SET_NZ8(r); if (r == 0x7f) reg_cc |= CC_V; }
#define OP_INCR(r) { r++; r &= 0xff; CLR_NZV; SET_NZ8(r); if (r == 0x80) reg_cc |= CC_V; }
#define OP_TSTR(r) { CLR_NZV; SET_NZ8(r); }
#define OP_CLRR(r) { r = 0; CLR_NVC; reg_cc |= CC_Z; }

#define OP_SUB16(r,a) { uint_least32_t octet, result; a(addr,octet); result = r - octet; CLR_NZVC; SET_NZVC16(r, octet, result); r = result; }
#define OP_ADD16(r,a) { uint_least32_t octet, result; a(addr,octet); result = r + octet; CLR_NZVC; SET_NZVC16(r, octet, result); r = result; }
#define OP_CMP16(r,a) { uint_least32_t octet, result; a(addr,octet); result = r - octet; CLR_NZVC; SET_NZVC16(r, octet, result); }
#define OP_LD16(r,a) { a(addr,r); CLR_NZV; SET_NZ16(r); }
#define OP_ST16(r,a) { uint_least16_t addr; a(addr); store_byte(addr, r >> 8); store_byte(addr+1, r & 0xff); CLR_NZV; SET_NZ16(r); }
#define OP_JSR(a) { uint_least16_t addr; a(addr); PUSHWORD(reg_s, reg_pc); reg_pc = addr; }

#define OP_SUB(r,a) { uint_least16_t octet, result; a(addr,octet); result = r - octet; CLR_NZVC; SET_NZVC8(r, octet, result); r = result; }
#define OP_CMP(r,a) { uint_least16_t octet, result; a(addr,octet); result = r - octet; CLR_NZVC; SET_NZVC8(r, octet, result); }
#define OP_SBC(r,a) { uint_least16_t octet, result; a(addr,octet); result = r - octet - (reg_cc & CC_C); CLR_NZVC; SET_NZVC8(r, octet, result); r = result; }
#define OP_AND(r,a) { uint_least16_t octet; a(addr,octet); r &= octet; CLR_NZV; SET_NZ8(r); }
#define OP_BIT(r,a) { uint_least16_t octet, result; a(addr,octet); result = r & octet; CLR_NZV; SET_NZ8(result); }
#define OP_LD(r,a) { a(addr,r); CLR_NZV; SET_NZ8(r); }
#define OP_ST(r,a) { uint_least16_t addr; a(addr); store_byte(addr, r); CLR_NZV; SET_NZ8(r); }
#define OP_EOR(r,a) { uint_least16_t octet; a(addr,octet); r ^= octet; CLR_NZV; SET_NZ8(r); }
#define OP_ADC(r,a) { uint_least16_t octet, result; a(addr,octet); result = r + octet + (reg_cc & CC_C); CLR_HNZVC; SET_NZVC8(r, octet, result); SET_H(r, octet, result); r = result; }
#define OP_OR(r,a) { uint_least16_t octet; a(addr,octet); r |= octet; CLR_NZV; SET_NZ8(r); }
#define OP_ADD(r,a) { uint_least16_t octet, result; a(addr,octet); result = r + octet; CLR_HNZVC; SET_NZVC8(r, octet, result); SET_H(r, octet, result); r = result; }

#define BRANCHS(cond) { uint_least16_t RA; SHORT_RELATIVE(RA); \
		if (cond) { reg_pc += RA; TAKEN_CYCLES(3); } \
		return; \
	}
#define BRANCHL(cond,cyc) { uint_least16_t RA; LONG_RELATIVE(RA); \
		if (cond) { reg_pc += RA; TAKEN_CYCLES(cyc+1); return; } \
		TAKEN_CYCLES(cyc); return; \
	}

/* ------------------------------------------------------------------------- */

static void op_exg(void);
static void op_tfr(void);

static inline void switched_block(void) {
	unsigned int op;
	BYTE_IMMEDIATE(0,op);
	switch(op) {
		/* 0x00 NEG direct */
		case 0x00: OP_NEG(BYTE_DIRECT); TAKEN_CYCLES(6); return; break;
		/* 0x01 NEGCOM direct (illegal) */
		case 0x01:
			   if (reg_cc & CC_C) {
				   OP_COM(BYTE_DIRECT);
				   TOOK_CYCLES(6);
			   } else {
				   OP_NEG(BYTE_DIRECT);
				   TOOK_CYCLES(6);
			   }
			   break;
		/* 0x03 COM direct */
		case 0x03: OP_COM(BYTE_DIRECT); TOOK_CYCLES(6); break;
		/* 0x04 LSR direct */
		case 0x04: OP_LSR(BYTE_DIRECT); TOOK_CYCLES(6); break;
		/* 0x06 ROR direct */
		case 0x06: OP_ROR(BYTE_DIRECT); TOOK_CYCLES(6); break;
		/* 0x07 ASR direct */
		case 0x07: OP_ASR(BYTE_DIRECT); TOOK_CYCLES(6); break;
		/* 0x08 ASL,LSL direct */
		case 0x08: OP_ASL(BYTE_DIRECT); TOOK_CYCLES(6); break;
		/* 0x09 ROL direct */
		case 0x09: OP_ROL(BYTE_DIRECT); TOOK_CYCLES(6); break;
		/* 0x0A DEC direct */
		case 0x0A: OP_DEC(BYTE_DIRECT); TOOK_CYCLES(6); break;
		/* 0x0C INC direct */
		case 0x0C: OP_INC(BYTE_DIRECT); TOOK_CYCLES(6); break;
		/* 0x0D TST direct */
		case 0x0D: OP_TST(BYTE_DIRECT); TOOK_CYCLES(6); break;
		/* 0x0E JMP direct */
		case 0x0E: OP_JMP(EA_DIRECT); TOOK_CYCLES(3); break;
		/* 0x0F CLR direct */
		case 0x0F: OP_CLR(BYTE_DIRECT); TOOK_CYCLES(6); break;
		/* 0x10 Page 2 */
		case 0x10: switched_block_page2(); return; break;
		/* 0x11 Page 3 */
		case 0x11: switched_block_page3(); return; break;
		/* 0x12 NOP inherent */
		case 0x12: TOOK_CYCLES(2); break;
		/* 0x13 SYNC inherent */
		case 0x13: wait_for_interrupt = 1; TOOK_CYCLES(3); break;
		/* 0x16 LBRA relative */
		case 0x16: { uint_least16_t RA; LONG_RELATIVE(RA); reg_pc += RA; TOOK_CYCLES(5); } break;
		/* 0x17 LBSR relative */
		case 0x17:
			   { uint_least16_t RA; LONG_RELATIVE(RA);
			   PUSHWORD(reg_s, reg_pc);
			   reg_pc += RA;
			   TOOK_CYCLES(9);
			   }
			   break;
		/* 0x19 DAA inherent */
		case 0x19: {
			uint_least16_t result = 0;
			if ((reg_a&0x0f) >= 0x0a || reg_cc & CC_H) result |= 0x06;
			if (reg_a >= 0x90 && (reg_a&0x0f) >= 0x0a) result |= 0x60;
			if (reg_a >= 0xa0 || reg_cc & CC_C) result |= 0x60;
			result += reg_a;
			reg_a = result & 0xff;
			CLR_NZVC;  /* was CLR_NZV */
			SET_NZC8(result);
			TAKEN_CYCLES(2);
			return; } break;
		/* 0x1A ORCC immediate */
		case 0x1a: { unsigned int v; BYTE_IMMEDIATE(0,v); reg_cc |= v; TOOK_CYCLES(3); return; } break;
		/* 0x1C ANDCC immediate */
		case 0x1c: { unsigned int v; BYTE_IMMEDIATE(0,v); reg_cc &= v; TOOK_CYCLES(3); return; } break;
		/* 0x1D SEX inherent */
		case 0x1d:
			reg_d = sex(reg_b);
			CLR_NZV;
			SET_NZ16(reg_d);
			TOOK_CYCLES(2);
			break;
		/* 0x1E EXG immediate */
		case 0x1e: op_exg(); return; break;
		/* 0x1F TFR immediate */
		case 0x1f: op_tfr(); return; break;
		/* 0x20 BRA relative */
		case 0x20: BRANCHS(1); break;
		/* 0x21 BRN relative */
		case 0x21: BRANCHS(0); break;
		/* 0x22 BHI relative */
		case 0x22: BRANCHS(!(reg_cc & (CC_Z|CC_C))); break;
		/* 0x23 BLS relative */
		case 0x23: BRANCHS(reg_cc & (CC_Z|CC_C)); break;
		/* 0x24 BHS,BCC relative */
		case 0x24: BRANCHS(!(reg_cc & CC_C)); break;
		/* 0x25 BLO,BCS relative */
		case 0x25: BRANCHS(reg_cc & CC_C); break;
		/* 0x26 BNE relative */
		case 0x26: BRANCHS(!(reg_cc & CC_Z)); break;
		/* 0x27 BEQ relative */
		case 0x27: BRANCHS(reg_cc & CC_Z); break;
		/* 0x28 BVC relative */
		case 0x28: BRANCHS(!(reg_cc & CC_V)); break;
		/* 0x29 BVS relative */
		case 0x29: BRANCHS(reg_cc & CC_V); break;
		/* 0x2A BPL relative */
		case 0x2a: BRANCHS(!(reg_cc & CC_N)); break;
		/* 0x2B BMI relative */
		case 0x2b: BRANCHS(reg_cc & CC_N); break;
		/* 0x2C BGE relative */
		case 0x2c: BRANCHS(!N_EOR_V); break;
		/* 0x2D BLT relative */
		case 0x2d: BRANCHS(N_EOR_V); break;
		/* 0x2E BGT relative */
		case 0x2e: BRANCHS(!(N_EOR_V || reg_cc & CC_Z)); break;
		/* 0x2F BLE relative */
		case 0x2f: BRANCHS(N_EOR_V || reg_cc & CC_Z); break;
		/* 0x30 LEAX indexed */
		case 0x30: EA_INDEXED(reg_x);
			CLR_Z;
			SET_Z(reg_x);
			TOOK_CYCLES(4);
			break;
		/* 0x31 LEAY indexed */
		case 0x31: EA_INDEXED(reg_y);
			CLR_Z;
			SET_Z(reg_y);
			TOOK_CYCLES(4);
			break;
		/* 0x32 LEAS indexed */
		case 0x32: EA_INDEXED(reg_s);
			TOOK_CYCLES(4);
			break;
		/* 0x33 LEAU indexed */
		case 0x33: EA_INDEXED(reg_u);
			TOOK_CYCLES(4);
			break;
		/* 0x34 PSHS immediate */
		case 0x34: PUSHR(reg_s, reg_u); return; break;
		/* 0x35 PULS immediate */
		case 0x35: PULLR(reg_s, reg_u); return; break;
		/* 0x36 PSHU immediate */
		case 0x36: PUSHR(reg_u, reg_s); return; break;
		/* 0x37 PULU immediate */
		case 0x37: PULLR(reg_u, reg_s); return; break;
		/* 0x39 RTS inherent */
		case 0x39: PULLWORD(reg_s, reg_pc); TOOK_CYCLES(5); break;
		/* 0x3A ABX inherent */
		case 0x3a: reg_x += reg_b; TOOK_CYCLES(3); break;
		/* 0x3B RTI inherent */
		case 0x3b: PULLBYTE(reg_s, reg_cc);
			if (reg_cc & CC_E) {
				PULLBYTE(reg_s, reg_a);
				PULLBYTE(reg_s, reg_b);
				PULLBYTE(reg_s, reg_dp);
				PULLWORD(reg_s, reg_x);
				PULLWORD(reg_s, reg_y);
				PULLWORD(reg_s, reg_u);
				PULLWORD(reg_s, reg_pc);
				TOOK_CYCLES(15);
			} else {
				PULLWORD(reg_s, reg_pc);
				TOOK_CYCLES(6);
			}
			break;
		/* 0x3C CWAI immediate */
		case 0x3c: { unsigned int v; BYTE_IMMEDIATE(0,v); reg_cc &= v;
			INTERRUPT_REGISTER_PUSH(1);
			wait_for_interrupt = 1;
			skip_register_push = 1;
			LOG_DEBUG(4,"M6809: CWAI\n");
			TOOK_CYCLES(11);
			return; } break;
		/* 0x3D MUL inherent */
		case 0x3d: reg_d = reg_a * reg_b;
			CLR_ZC;
			SET_Z(reg_d);
			if (reg_d & 0x80)
				reg_cc |= CC_C;
			TOOK_CYCLES(11);
			break;
		/* 0x3F SWI inherent */
		case 0x3f: reg_cc |= CC_E;
			PUSHWORD(reg_s, reg_pc);
			PUSHWORD(reg_s, reg_u);
			PUSHWORD(reg_s, reg_y);
			PUSHWORD(reg_s, reg_x);
			PUSHBYTE(reg_s, reg_dp);
			PUSHBYTE(reg_s, reg_b);
			PUSHBYTE(reg_s, reg_a);
			PUSHBYTE(reg_s, reg_cc);
			reg_cc |= CC_F|CC_I;
			reg_pc = fetch_word(0xfffa);
			TOOK_CYCLES(19);
			break;
		/* 0x40 NEGA inherent */
		case 0x40: OP_NEGR(reg_a); TOOK_CYCLES(2); break;
		/* 0x43 COMA inherent */
		case 0x43: OP_COMR(reg_a); TOOK_CYCLES(2); break;
		/* 0x44 LSRA inherent */
		case 0x44: OP_LSRR(reg_a); TOOK_CYCLES(2); break;
		/* 0x46 RORA inherent */
		case 0x46: OP_RORR(reg_a); TOOK_CYCLES(2); break;
		/* 0x47 ASRA inherent */
		case 0x47: OP_ASRR(reg_a); TOOK_CYCLES(2); break;
		/* 0x48 ASLA,LSLA inherent */
		case 0x48: OP_ASLR(reg_a); TOOK_CYCLES(2); break;
		/* 0x49 ROLA inherent */
		case 0x49: OP_ROLR(reg_a); TOOK_CYCLES(2); break;
		/* 0x4A DECA inherent */
		case 0x4a: OP_DECR(reg_a); TOOK_CYCLES(2); break;
		/* 0x4C INCA inherent */
		case 0x4c: OP_INCR(reg_a); TOOK_CYCLES(2); break;
		/* 0x4D TSTA inherent */
		case 0x4d: OP_TSTR(reg_a); TOOK_CYCLES(2); break;
		/* 0x4F CLRA inherent */
		case 0x4f: OP_CLRR(reg_a); TOOK_CYCLES(2); break;
		/* 0x50 NEGB inherent */
		case 0x50: OP_NEGR(reg_b); TOOK_CYCLES(2); break;
		/* 0x53 COMB inherent */
		case 0x53: OP_COMR(reg_b); TOOK_CYCLES(2); break;
		/* 0x54 LSRB inherent */
		case 0x54: OP_LSRR(reg_b); TOOK_CYCLES(2); break;
		/* 0x56 RORB inherent */
		case 0x56: OP_RORR(reg_b); TOOK_CYCLES(2); break;
		/* 0x57 ASRB inherent */
		case 0x57: OP_ASRR(reg_b); TOOK_CYCLES(2); break;
		/* 0x58 ASLB,LSLB inherent */
		case 0x58: OP_ASLR(reg_b); TOOK_CYCLES(2); break;
		/* 0x59 ROLB inherent */
		case 0x59: OP_ROLR(reg_b); TOOK_CYCLES(2); break;
		/* 0x5A DECB inherent */
		case 0x5a: OP_DECR(reg_b); TOOK_CYCLES(2); break;
		/* 0x5C INCB inherent */
		case 0x5c: OP_INCR(reg_b); TOOK_CYCLES(2); break;
		/* 0x5D TSTB inherent */
		case 0x5d: OP_TSTR(reg_b); TOOK_CYCLES(2); break;
		/* 0x5F CLRB inherent */
		case 0x5f: OP_CLRR(reg_b); TOOK_CYCLES(2); break;
		/* 0x60 NEG indexed */
		case 0x60: OP_NEG(BYTE_INDEXED); TOOK_CYCLES(6); break;
		/* 0x63 COM indexed */
		case 0x63: OP_COM(BYTE_INDEXED); TOOK_CYCLES(6); break;
		/* 0x64 LSR indexed */
		case 0x64: OP_LSR(BYTE_INDEXED); TOOK_CYCLES(6); break;
		/* 0x66 ROR indexed */
		case 0x66: OP_ROR(BYTE_INDEXED); TOOK_CYCLES(6); break;
		/* 0x67 ASR indexed */
		case 0x67: OP_ASR(BYTE_INDEXED); TOOK_CYCLES(6); break;
		/* 0x68 ASL,LSL indexed */
		case 0x68: OP_ASL(BYTE_INDEXED); TOOK_CYCLES(6); break;
		/* 0x69 ROL indexed */
		case 0x69: OP_ROL(BYTE_INDEXED); TOOK_CYCLES(6); break;
		/* 0x6A DEC indexed */
		case 0x6a: OP_DEC(BYTE_INDEXED); TOOK_CYCLES(6); break;
		/* 0x6C INC indexed */
		case 0x6c: OP_INC(BYTE_INDEXED); TOOK_CYCLES(6); break;
		/* 0x6D TST indexed */
		case 0x6d: OP_TST(BYTE_INDEXED); TOOK_CYCLES(6); break;
		/* 0x6E JMP indexed */
		case 0x6e: OP_JMP(EA_INDEXED); TOOK_CYCLES(3); break;
		/* 0x6F CLR indexed */
		case 0x6f: OP_CLR(BYTE_INDEXED); TOOK_CYCLES(6); break;
		/* 0x70 NEG extended */
		case 0x70: OP_NEG(BYTE_EXTENDED); TOOK_CYCLES(7); break;
		/* 0x73 COM extended */
		case 0x73: OP_COM(BYTE_EXTENDED); TOOK_CYCLES(7); break;
		/* 0x74 LSR extended */
		case 0x74: OP_LSR(BYTE_EXTENDED); TOOK_CYCLES(7); break;
		/* 0x76 ROR extended */
		case 0x76: OP_ROR(BYTE_EXTENDED); TOOK_CYCLES(7); break;
		/* 0x77 ASR extended */
		case 0x77: OP_ASR(BYTE_EXTENDED); TOOK_CYCLES(7); break;
		/* 0x78 ASL,LSL extended */
		case 0x78: OP_ASL(BYTE_EXTENDED); TOOK_CYCLES(7); break;
		/* 0x79 ROL extended */
		case 0x79: OP_ROL(BYTE_EXTENDED); TOOK_CYCLES(7); break;
		/* 0x7A DEC extended */
		case 0x7a: OP_DEC(BYTE_EXTENDED); TOOK_CYCLES(7); break;
		/* 0x7C INC extended */
		case 0x7c: OP_INC(BYTE_EXTENDED); TOOK_CYCLES(7); break;
		/* 0x7D TST extended */
		case 0x7d: OP_TST(BYTE_EXTENDED); TOOK_CYCLES(7); break;
		/* 0x7E JMP extended */
		case 0x7e: OP_JMP(EA_EXTENDED); TOOK_CYCLES(4); break;
		/* 0x7F CLR extended */
		case 0x7f: OP_CLR(BYTE_EXTENDED); TOOK_CYCLES(7); break;
		/* 0x80 SUBA immediate */
		case 0x80: OP_SUB(reg_a, BYTE_IMMEDIATE); TOOK_CYCLES(2); break;
		/* 0x81 CMPA immediate */
		case 0x81: OP_CMP(reg_a, BYTE_IMMEDIATE); TOOK_CYCLES(2); break;
		/* 0x82 SBCA immediate */
		case 0x82: OP_SBC(reg_a, BYTE_IMMEDIATE); TOOK_CYCLES(2); break;
		/* 0x83 SUBD immediate */
		case 0x83: OP_SUB16(reg_d, WORD_IMMEDIATE); TOOK_CYCLES(4); break;
		/* 0x84 ANDA immediate */
		case 0x84: OP_AND(reg_a, BYTE_IMMEDIATE); TOOK_CYCLES(2); break;
		/* 0x85 BITA immediate */
		case 0x85: OP_BIT(reg_a, BYTE_IMMEDIATE); TOOK_CYCLES(2); break;
		/* 0x86 LDA immediate */
		case 0x86: OP_LD(reg_a, BYTE_IMMEDIATE); TOOK_CYCLES(2); break;
		/* 0x88 EORA immediate */
		case 0x88: OP_EOR(reg_a, BYTE_IMMEDIATE); TOOK_CYCLES(2); break;
		/* 0x89 ADCA immediate */
		case 0x89: OP_ADC(reg_a, BYTE_IMMEDIATE); TOOK_CYCLES(2); break;
		/* 0x8A ORA immediate */
		case 0x8a: OP_OR(reg_a, BYTE_IMMEDIATE); TOOK_CYCLES(2); break;
		/* 0x8B ADDA immediate */
		case 0x8b: OP_ADD(reg_a, BYTE_IMMEDIATE); TOOK_CYCLES(2); break;
		/* 0x8C CMPX immediate */
		case 0x8c: OP_CMP16(reg_x, WORD_IMMEDIATE); TOOK_CYCLES(4); break;
		/* 0x8D BSR relative */
		case 0x8d: { uint_least16_t RA; SHORT_RELATIVE(RA);
			   PUSHWORD(reg_s, reg_pc);
			   reg_pc += RA;
			   TOOK_CYCLES(7);
			   return; } break;
		/* 0x8E LDX immediate */
		case 0x8e: OP_LD16(reg_x, WORD_IMMEDIATE); TOOK_CYCLES(3); break;
		/* 0x90 SUBA direct */
		case 0x90: OP_SUB(reg_a, BYTE_DIRECT); TOOK_CYCLES(4); break;
		/* 0x91 CMPA direct */
		case 0x91: OP_CMP(reg_a, BYTE_DIRECT); TOOK_CYCLES(4); break;
		/* 0x92 SBCA direct */
		case 0x92: OP_SBC(reg_a, BYTE_DIRECT); TOOK_CYCLES(4); break;
		/* 0x93 SUBD direct */
		case 0x93: OP_SUB16(reg_d, WORD_DIRECT); TOOK_CYCLES(6); break;
		/* 0x94 ANDA direct */
		case 0x94: OP_AND(reg_a, BYTE_DIRECT); TOOK_CYCLES(4); break;
		/* 0x95 BITA direct */
		case 0x95: OP_BIT(reg_a, BYTE_DIRECT); TOOK_CYCLES(4); break;
		/* 0x96 LDA direct */
		case 0x96: OP_LD(reg_a, BYTE_DIRECT); TOOK_CYCLES(4); break;
		/* 0x97 STA direct */
		case 0x97: OP_ST(reg_a, EA_DIRECT); TOOK_CYCLES(4); break;
		/* 0x98 EORA direct */
		case 0x98: OP_EOR(reg_a, BYTE_DIRECT); TOOK_CYCLES(4); break;
		/* 0x99 ADCA direct */
		case 0x99: OP_ADC(reg_a, BYTE_DIRECT); TOOK_CYCLES(4); break;
		/* 0x9A ORA direct */
		case 0x9a: OP_OR(reg_a, BYTE_DIRECT); TOOK_CYCLES(4); break;
		/* 0x9B ADDA direct */
		case 0x9b: OP_ADD(reg_a, BYTE_DIRECT); TOOK_CYCLES(4); break;
		/* 0x9C CMPX direct */
		case 0x9c: OP_CMP16(reg_x, WORD_DIRECT); TOOK_CYCLES(6); break;
		/* 0x9D JSR direct */
		case 0x9d: OP_JSR(EA_DIRECT); TOOK_CYCLES(7); break;
		/* 0x9E LDX direct */
		case 0x9e: OP_LD16(reg_x, WORD_DIRECT); TOOK_CYCLES(5); break;
		/* 0x9F STX direct */
		case 0x9f: OP_ST16(reg_x, EA_DIRECT); TOOK_CYCLES(5); break;
		/* 0xA0 SUBA indexed */
		case 0xa0: OP_SUB(reg_a, BYTE_INDEXED); TOOK_CYCLES(4); break;
		/* 0xA1 CMPA indexed */
		case 0xa1: OP_CMP(reg_a, BYTE_INDEXED); TOOK_CYCLES(4); break;
		/* 0xA2 SBCA indexed */
		case 0xa2: OP_SBC(reg_a, BYTE_INDEXED); TOOK_CYCLES(4); break;
		/* 0xA3 SUBD indexed */
		case 0xa3: OP_SUB16(reg_d, WORD_INDEXED); TOOK_CYCLES(6); break;
		/* 0xA4 ANDA indexed */
		case 0xa4: OP_AND(reg_a, BYTE_INDEXED); TOOK_CYCLES(4); break;
		/* 0xA5 BITA indexed */
		case 0xa5: OP_BIT(reg_a, BYTE_INDEXED); TOOK_CYCLES(4); break;
		/* 0xA6 LDA indexed */
		case 0xa6: OP_LD(reg_a, BYTE_INDEXED); TOOK_CYCLES(4); break;
		/* 0xA7 STA indexed */
		case 0xa7: OP_ST(reg_a, EA_INDEXED); TOOK_CYCLES(4); break;
		/* 0xA8 EORA indexed */
		case 0xa8: OP_EOR(reg_a, BYTE_INDEXED); TOOK_CYCLES(4); break;
		/* 0xA9 ADCA indexed */
		case 0xa9: OP_ADC(reg_a, BYTE_INDEXED); TOOK_CYCLES(4); break;
		/* 0xAA ORA indexed */
		case 0xaa: OP_OR(reg_a, BYTE_INDEXED); TOOK_CYCLES(4); break;
		/* 0xAB ADDA indexed */
		case 0xab: OP_ADD(reg_a, BYTE_INDEXED); TOOK_CYCLES(4); break;
		/* 0xAC CMPX indexed */
		case 0xac: OP_CMP16(reg_x, WORD_INDEXED); TOOK_CYCLES(6); break;
		/* 0xAD JSR indexed */
		case 0xad: OP_JSR(EA_INDEXED); TOOK_CYCLES(7); break;
		/* 0xAE LDX indexed */
		case 0xae: OP_LD16(reg_x, WORD_INDEXED); TOOK_CYCLES(5); break;
		/* 0xAF STX indexed */
		case 0xaf: OP_ST16(reg_x, EA_INDEXED); TOOK_CYCLES(5); break;
		/* 0xB0 SUBA extended */
		case 0xb0: OP_SUB(reg_a, BYTE_EXTENDED); TOOK_CYCLES(5); break;
		/* 0xB1 CMPA extended */
		case 0xb1: OP_CMP(reg_a, BYTE_EXTENDED); TOOK_CYCLES(5); break;
		/* 0xB2 SBCA extended */
		case 0xb2: OP_SBC(reg_a, BYTE_EXTENDED); TOOK_CYCLES(5); break;
		/* 0xB3 SUBD extended */
		case 0xb3: OP_SUB16(reg_d, WORD_EXTENDED); TOOK_CYCLES(7); break;
		/* 0xB4 ANDA extended */
		case 0xb4: OP_AND(reg_a, BYTE_EXTENDED); TOOK_CYCLES(5); break;
		/* 0xB5 BITA extended */
		case 0xb5: OP_BIT(reg_a, BYTE_EXTENDED); TOOK_CYCLES(5); break;
		/* 0xB6 LDA extended */
		case 0xb6: OP_LD(reg_a, BYTE_EXTENDED); TOOK_CYCLES(5); break;
		/* 0xB7 STA extended */
		case 0xb7: OP_ST(reg_a, EA_EXTENDED); TOOK_CYCLES(5); break;
		/* 0xB8 EORA extended */
		case 0xb8: OP_EOR(reg_a, BYTE_EXTENDED); TOOK_CYCLES(5); break;
		/* 0xB9 ADCA extended */
		case 0xb9: OP_ADC(reg_a, BYTE_EXTENDED); TOOK_CYCLES(5); break;
		/* 0xBA ORA extended */
		case 0xba: OP_OR(reg_a, BYTE_EXTENDED); TOOK_CYCLES(5); break;
		/* 0xBB ADDA extended */
		case 0xbb: OP_ADD(reg_a, BYTE_EXTENDED); TOOK_CYCLES(5); break;
		/* 0xBC CMPX extended */
		case 0xbc: OP_CMP16(reg_x, WORD_EXTENDED); TOOK_CYCLES(6); break;
		/* 0xBD JSR extended */
		case 0xbd: OP_JSR(EA_EXTENDED); TOOK_CYCLES(8); break;
		/* 0xBE LDX extended */
		case 0xbe: OP_LD16(reg_x, WORD_EXTENDED); TOOK_CYCLES(6); break;
		/* 0xBF STX extended */
		case 0xbf: OP_ST16(reg_x, EA_EXTENDED); TOOK_CYCLES(6); break;
		/* 0xC0 SUBB immediate */
		case 0xc0: OP_SUB(reg_b, BYTE_IMMEDIATE); TOOK_CYCLES(2); break;
		/* 0xC1 CMPB immediate */
		case 0xc1: OP_CMP(reg_b, BYTE_IMMEDIATE); TOOK_CYCLES(2); break;
		/* 0xC2 SBCB immediate */
		case 0xc2: OP_SBC(reg_b, BYTE_IMMEDIATE); TOOK_CYCLES(2); break;
		/* 0xC3 ADDD immediate */
		case 0xc3: OP_ADD16(reg_d, WORD_IMMEDIATE); TOOK_CYCLES(4); break;
		/* 0xC4 ANDB immediate */
		case 0xc4: OP_AND(reg_b, BYTE_IMMEDIATE); TOOK_CYCLES(2); break;
		/* 0xC5 BITB immediate */
		case 0xc5: OP_BIT(reg_b, BYTE_IMMEDIATE); TOOK_CYCLES(2); break;
		/* 0xC6 LDB immediate */
		case 0xc6: OP_LD(reg_b, BYTE_IMMEDIATE); TOOK_CYCLES(2); break;
		/* 0xC8 EORB immediate */
		case 0xc8: OP_EOR(reg_b, BYTE_IMMEDIATE); TOOK_CYCLES(2); break;
		/* 0xC9 ADCB immediate */
		case 0xc9: OP_ADC(reg_b, BYTE_IMMEDIATE); TOOK_CYCLES(2); break;
		/* 0xCA ORB immediate */
		case 0xca: OP_OR(reg_b, BYTE_IMMEDIATE); TOOK_CYCLES(2); break;
		/* 0xCB ADDB immediate */
		case 0xcb: OP_ADD(reg_b, BYTE_IMMEDIATE); TOOK_CYCLES(2); break;
		/* 0xCC LDD immediate */
		case 0xcc: OP_LD16(reg_d, WORD_IMMEDIATE); TOOK_CYCLES(3); break;
		/* 0xCE LDU immediate */
		case 0xce: OP_LD16(reg_u, WORD_IMMEDIATE); TOOK_CYCLES(3); break;
		/* 0xD0 SUBB direct */
		case 0xd0: OP_SUB(reg_b, BYTE_DIRECT); TOOK_CYCLES(4); break;
		/* 0xD1 CMPB direct */
		case 0xd1: OP_CMP(reg_b, BYTE_DIRECT); TOOK_CYCLES(4); break;
		/* 0xD2 SBCB direct */
		case 0xd2: OP_SBC(reg_b, BYTE_DIRECT); TOOK_CYCLES(4); break;
		/* 0xD3 ADDD direct */
		case 0xd3: OP_ADD16(reg_d, WORD_DIRECT); TOOK_CYCLES(6); break;
		/* 0xD4 ANDB direct */
		case 0xd4: OP_AND(reg_b, BYTE_DIRECT); TOOK_CYCLES(4); break;
		/* 0xD5 BITB direct */
		case 0xd5: OP_BIT(reg_b, BYTE_DIRECT); TOOK_CYCLES(4); break;
		/* 0xD6 LDB direct */
		case 0xd6: OP_LD(reg_b, BYTE_DIRECT); TOOK_CYCLES(4); break;
		/* 0xD7 STB direct */
		case 0xd7: OP_ST(reg_b, EA_DIRECT); TOOK_CYCLES(4); break;
		/* 0xD8 EORB direct */
		case 0xd8: OP_EOR(reg_b, BYTE_DIRECT); TOOK_CYCLES(4); break;
		/* 0xD9 ADCB direct */
		case 0xd9: OP_ADC(reg_b, BYTE_DIRECT); TOOK_CYCLES(4); break;
		/* 0xDA ORB direct */
		case 0xda: OP_OR(reg_b, BYTE_DIRECT); TOOK_CYCLES(4); break;
		/* 0xDB ADDB direct */
		case 0xdb: OP_ADD(reg_b, BYTE_DIRECT); TOOK_CYCLES(4); break;
		/* 0xDC LDD direct */
		case 0xdc: OP_LD16(reg_d, WORD_DIRECT); TOOK_CYCLES(5); break;
		/* 0xDD STD direct */
		case 0xdd: OP_ST16(reg_d, EA_DIRECT); TOOK_CYCLES(5); break;
		/* 0xDE LDU direct */
		case 0xde: OP_LD16(reg_u, WORD_DIRECT); TOOK_CYCLES(5); break;
		/* 0xDF STU direct */
		case 0xdf: OP_ST16(reg_u, EA_DIRECT); TOOK_CYCLES(5); break;
		/* 0xE0 SUBB indexed */
		case 0xe0: OP_SUB(reg_b, BYTE_INDEXED); TOOK_CYCLES(4); break;
		/* 0xE1 CMPB indexed */
		case 0xe1: OP_CMP(reg_b, BYTE_INDEXED); TOOK_CYCLES(4); break;
		/* 0xE2 SBCB indexed */
		case 0xe2: OP_SBC(reg_b, BYTE_INDEXED); TOOK_CYCLES(4); break;
		/* 0xE3 ADDD indexed */
		case 0xe3: OP_ADD16(reg_d, WORD_INDEXED); TOOK_CYCLES(6); break;
		/* 0xE4 ANDB indexed */
		case 0xe4: OP_AND(reg_b, BYTE_INDEXED); TOOK_CYCLES(4); break;
		/* 0xE5 BITB indexed */
		case 0xe5: OP_BIT(reg_b, BYTE_INDEXED); TOOK_CYCLES(4); break;
		/* 0xE6 LDB indexed */
		case 0xe6: OP_LD(reg_b, BYTE_INDEXED); TOOK_CYCLES(4); break;
		/* 0xE7 STB indexed */
		case 0xe7: OP_ST(reg_b, EA_INDEXED); TOOK_CYCLES(4); break;
		/* 0xE8 EORB indexed */
		case 0xe8: OP_EOR(reg_b, BYTE_INDEXED); TOOK_CYCLES(4); break;
		/* 0xE9 ADCB indexed */
		case 0xe9: OP_ADC(reg_b, BYTE_INDEXED); TOOK_CYCLES(4); break;
		/* 0xEA ORB indexed */
		case 0xea: OP_OR(reg_b, BYTE_INDEXED); TOOK_CYCLES(4); break;
		/* 0xEB ADDB indexed */
		case 0xeb: OP_ADD(reg_b, BYTE_INDEXED); TOOK_CYCLES(4); break;
		/* 0xEC LDD indexed */
		case 0xec: OP_LD16(reg_d, WORD_INDEXED); TOOK_CYCLES(5); break;
		/* 0xED STD indexed */
		case 0xed: OP_ST16(reg_d, EA_INDEXED); TOOK_CYCLES(5); break;
		/* 0xEE LDU indexed */
		case 0xee: OP_LD16(reg_u, WORD_INDEXED); TOOK_CYCLES(5); break;
		/* 0xEF STU indexed */
		case 0xef: OP_ST16(reg_u, EA_INDEXED); TOOK_CYCLES(5); break;
		/* 0xF0 SUBB extended */
		case 0xf0: OP_SUB(reg_b, BYTE_EXTENDED); TOOK_CYCLES(5); break;
		/* 0xF1 CMPB extended */
		case 0xf1: OP_CMP(reg_b, BYTE_EXTENDED); TOOK_CYCLES(5); break;
		/* 0xF2 SBCB extended */
		case 0xf2: OP_SBC(reg_b, BYTE_EXTENDED); TOOK_CYCLES(5); break;
		/* 0xF3 ADDD extended */
		case 0xf3: OP_ADD16(reg_d, WORD_EXTENDED); TOOK_CYCLES(7); break;
		/* 0xF4 ANDB extended */
		case 0xf4: OP_AND(reg_b, BYTE_EXTENDED); TOOK_CYCLES(5); break;
		/* 0xF5 BITB extended */
		case 0xf5: OP_BIT(reg_b, BYTE_EXTENDED); TOOK_CYCLES(5); break;
		/* 0xF6 LDB extended */
		case 0xf6: OP_LD(reg_b, BYTE_EXTENDED); TOOK_CYCLES(5); break;
		/* 0xF7 STB extended */
		case 0xf7: OP_ST(reg_b, EA_EXTENDED); TOOK_CYCLES(5); break;
		/* 0xF8 EORB extended */
		case 0xf8: OP_EOR(reg_b, BYTE_EXTENDED); TOOK_CYCLES(5); break;
		/* 0xF9 ADCB extended */
		case 0xf9: OP_ADC(reg_b, BYTE_EXTENDED); TOOK_CYCLES(5); break;
		/* 0xFA ORB extended */
		case 0xfa: OP_OR(reg_b, BYTE_EXTENDED); TOOK_CYCLES(5); break;
		/* 0xFB ADDB extended */
		case 0xfb: OP_ADD(reg_b, BYTE_EXTENDED); TOOK_CYCLES(5); break;
		/* 0xFC LDD extended */
		case 0xfc: OP_LD16(reg_d, WORD_EXTENDED); TOOK_CYCLES(6); break;
		/* 0xFD STD extended */
		case 0xfd: OP_ST16(reg_d, EA_EXTENDED); TOOK_CYCLES(6); break;
		/* 0xFE LDU extended */
		case 0xfe: OP_LD16(reg_u, WORD_EXTENDED); TOOK_CYCLES(6); break;
		/* 0xFF STU extended */
		case 0xff: OP_ST16(reg_u, EA_EXTENDED); TOOK_CYCLES(6); break;
		/* Illegal instruction */
		default: TOOK_CYCLES(1); break;
	}
}

static void switched_block_page2(void) {
	unsigned int op;
	BYTE_IMMEDIATE(0,op);
	switch(op) {
		/* 0x1021 LBRN relative */
		case 0x21: { uint_least16_t RA; LONG_RELATIVE(RA); TAKEN_CYCLES(5); return; } break;
		/* 0x1022 LBHI relative */
		case 0x22: BRANCHL(!(reg_cc & (CC_Z|CC_C)),5); break;
		/* 0x1023 LBLS relative */
		case 0x23: BRANCHL(reg_cc & (CC_Z|CC_C),5); break;
		/* 0x1024 LBHS,LBCC relative */
		case 0x24: BRANCHL(!(reg_cc & CC_C),5); break;
		/* 0x1025 LBLO,LBCS relative */
		case 0x25: BRANCHL(reg_cc & CC_C,5); break;
		/* 0x1026 LBNE relative */
		case 0x26: BRANCHL(!(reg_cc & CC_Z),5); break;
		/* 0x1027 LBEQ relative */
		case 0x27: BRANCHL(reg_cc & CC_Z,5); break;
		/* 0x1028 LBVC relative */
		case 0x28: BRANCHL(!(reg_cc & CC_V),5); break;
		/* 0x1029 LBVS relative */
		case 0x29: BRANCHL(reg_cc & CC_V,5); break;
		/* 0x102A LBPL relative */
		case 0x2a: BRANCHL(!(reg_cc & CC_N),5); break;
		/* 0x102B LBMI relative */
		case 0x2b: BRANCHL(reg_cc & CC_N,5); break;
		/* 0x102C LBGE relative */
		case 0x2c: BRANCHL(!N_EOR_V,5); break;
		/* 0x102D LBLT relative */
		case 0x2d: BRANCHL(N_EOR_V,5); break;
		/* 0x102E LBGT relative */
		case 0x2e: BRANCHL(!(N_EOR_V || reg_cc & CC_Z),5); break;
		/* 0x102F LBLE relative */
		case 0x2f: BRANCHL(N_EOR_V || (reg_cc & CC_Z),5); break;
		/* 0x103F SWI2 inherent */
		case 0x3f: reg_cc |= CC_E;
			PUSHWORD(reg_s, reg_pc);
			PUSHWORD(reg_s, reg_u);
			PUSHWORD(reg_s, reg_y);
			PUSHWORD(reg_s, reg_x);
			PUSHBYTE(reg_s, reg_dp);
			PUSHBYTE(reg_s, reg_b);
			PUSHBYTE(reg_s, reg_a);
			PUSHBYTE(reg_s, reg_cc);
			reg_pc = fetch_word(0xfff4);
			TAKEN_CYCLES(20);
			return; break;
		/* 0x1083 CMPD immediate */
		case 0x83: OP_CMP16(reg_d, WORD_IMMEDIATE); TAKEN_CYCLES(5);
			return; break;
		/* 0x108C CMPY immediate */
		case 0x8c: OP_CMP16(reg_y, WORD_IMMEDIATE); TAKEN_CYCLES(5);
			return; break;
		/* 0x108E LDY immediate */
		case 0x8e: OP_LD16(reg_y, WORD_IMMEDIATE); TAKEN_CYCLES(4);
			return; break;
		/* 0x1093 CMPD direct */
		case 0x93: OP_CMP16(reg_d, WORD_DIRECT); TAKEN_CYCLES(7);
			break; return;
		/* 0x109C CMPY direct */
		case 0x9c: OP_CMP16(reg_y, WORD_DIRECT); TAKEN_CYCLES(7);
			break; return;
		/* 0x109E LDY direct */
		case 0x9e: OP_LD16(reg_y, WORD_DIRECT); TAKEN_CYCLES(6);
			break; return;
		/* 0x109F STY direct */
		case 0x9f: OP_ST16(reg_y, EA_DIRECT); TAKEN_CYCLES(6);
			break; return;
		/* 0x10A3 CMPD indexed */
		case 0xa3: OP_CMP16(reg_d, WORD_INDEXED); TAKEN_CYCLES(7);
			break; return;
		/* 0x10AC CMPY indexed */
		case 0xac: OP_CMP16(reg_y, WORD_INDEXED); TAKEN_CYCLES(7);
			break; return;
		/* 0x10AE LDY indexed */
		case 0xae: OP_LD16(reg_y, WORD_INDEXED); TAKEN_CYCLES(6);
			break; return;
		/* 0x10AF STY indexed */
		case 0xaf: OP_ST16(reg_y, EA_INDEXED); TAKEN_CYCLES(6);
			break; return;
		/* 0x10B3 CMPD extended */
		case 0xb3: OP_CMP16(reg_d, WORD_EXTENDED); TAKEN_CYCLES(8);
			return; break;
		/* 0x10BC CMPY extended */
		case 0xbc: OP_CMP16(reg_y, WORD_EXTENDED); TAKEN_CYCLES(8);
			return; break;
		/* 0x10BE LDY extended */
		case 0xbe: OP_LD16(reg_y, WORD_EXTENDED); TAKEN_CYCLES(7);
			return; break;
		/* 0x10BF STY extended */
		case 0xbf: OP_ST16(reg_y, EA_EXTENDED); TAKEN_CYCLES(7);
			return; break;
		/* 0x10CE LDS immediate */
		case 0xce: OP_LD16(reg_s, WORD_IMMEDIATE); TAKEN_CYCLES(4);
			return; break;
		/* 0x10DE LDS direct */
		case 0xde: OP_LD16(reg_s, WORD_DIRECT); TAKEN_CYCLES(6);
			return; break;
		/* 0x10DF STS direct */
		case 0xdf: OP_ST16(reg_s, EA_DIRECT); TAKEN_CYCLES(6);
			return; break;
		/* 0x10EE LDS indexed */
		case 0xee: OP_LD16(reg_s, WORD_INDEXED); TAKEN_CYCLES(6);
			return; break;
		/* 0x10EF STS indexed */
		case 0xef: OP_ST16(reg_s, EA_INDEXED); TAKEN_CYCLES(6);
			return; break;
		/* 0x10FE LDS extended */
		case 0xfe: OP_LD16(reg_s, WORD_EXTENDED); TAKEN_CYCLES(7);
			return; break;
		/* 0x10FF STS extended */
		case 0xff: OP_ST16(reg_s, EA_EXTENDED); TAKEN_CYCLES(7);
			return; break;
		/* Illegal instruction */
		default: TAKEN_CYCLES(2); return; break;
	}
}

static void switched_block_page3(void) {
	unsigned int op;
	BYTE_IMMEDIATE(0,op);
	switch(op) {
		/* 0x113F SWI3 inherent */
		case 0x3f: reg_cc |= CC_E;
			PUSHWORD(reg_s, reg_pc);
			PUSHWORD(reg_s, reg_u);
			PUSHWORD(reg_s, reg_y);
			PUSHWORD(reg_s, reg_x);
			PUSHBYTE(reg_s, reg_dp);
			PUSHBYTE(reg_s, reg_b);
			PUSHBYTE(reg_s, reg_a);
			PUSHBYTE(reg_s, reg_cc);
			reg_pc = fetch_word(0xfff2);
			TAKEN_CYCLES(20);
			return; break;
		/* 0x1183 CMPU immediate */
		case 0x83: OP_CMP16(reg_u, WORD_IMMEDIATE); TAKEN_CYCLES(5);
			return; break;
		/* 0x118C CMPS immediate */
		case 0x8c: OP_CMP16(reg_s, WORD_IMMEDIATE); TAKEN_CYCLES(5);
			return; break;
		/* 0x1193 CMPU direct */
		case 0x93: OP_CMP16(reg_u, WORD_DIRECT); TAKEN_CYCLES(7);
			return; break;
		/* 0x119C CMPS direct */
		case 0x9c: OP_CMP16(reg_s, WORD_DIRECT); TAKEN_CYCLES(7);
			return; break;
		/* 0x11A3 CMPU indexed */
		case 0xa3: OP_CMP16(reg_u, WORD_INDEXED); TAKEN_CYCLES(7);
			return; break;
		/* 0x11AC CMPS indexed */
		case 0xac: OP_CMP16(reg_s, WORD_INDEXED); TAKEN_CYCLES(7);
			return; break;
		/* 0x11B3 CMPU extended */
		case 0xb3: OP_CMP16(reg_u, WORD_EXTENDED); TAKEN_CYCLES(8);
			return; break;
		/* 0x11BC CMPS extended */
		case 0xbc: OP_CMP16(reg_s, WORD_EXTENDED); TAKEN_CYCLES(8);
			return; break;
		/* Illegal instruction */
		default: TAKEN_CYCLES(2); return; break;
	}
}

/* ------------------------------------------------------------------------- */

/* 0x1E EXG immediate */
static void op_exg(void) {
	unsigned int postbyte;
	uint_least16_t source = 0xffff, dest = 0xffff;
	BYTE_IMMEDIATE(0,postbyte);
	switch ((postbyte & 0xf0) >> 4) {
		case 0x00: source = reg_d; break;
		case 0x01: source = reg_x; break;
		case 0x02: source = reg_y; break;
		case 0x03: source = reg_u; break;
		case 0x04: source = reg_s; break;
		case 0x05: source = reg_pc; break;
		case 0x08: source = 0xff00 | (reg_a & 0xff); break;
		case 0x09: source = 0xff00 | (reg_b & 0xff); break;
		case 0x0a: source = 0xff00 | (reg_cc & 0xff); break;
		case 0x0b: source = 0xff00 | (reg_dp & 0xff); break;
		default: break;
	}
	switch (postbyte & 0x0f) {
		case 0x00: dest = reg_d; reg_d = source; break;
		case 0x01: dest = reg_x; reg_x = source; break;
		case 0x02: dest = reg_y; reg_y = source; break;
		case 0x03: dest = reg_u; reg_u = source; break;
		case 0x04: dest = reg_s; reg_s = source; break;
		case 0x05: dest = reg_pc; reg_pc = source; break;
		case 0x08: dest = 0xff00 | (reg_a & 0xff); reg_a = source; break;
		case 0x09: dest = 0xff00 | (reg_b & 0xff); reg_b = source; break;
		case 0x0a: dest = 0xff00 | (reg_cc & 0xff); reg_cc = source; break;
		case 0x0b: dest = 0xff00 | (reg_dp & 0xff); reg_dp = source; break;
		default: break;
	}
	switch ((postbyte & 0xf0) >> 4) {
		case 0x00: reg_d = dest; break;
		case 0x01: reg_x = dest; break;
		case 0x02: reg_y = dest; break;
		case 0x03: reg_u = dest; break;
		case 0x04: reg_s = dest; break;
		case 0x05: reg_pc = dest; break;
		case 0x08: reg_a = dest; break;
		case 0x09: reg_b = dest; break;
		case 0x0a: reg_cc = dest; break;
		case 0x0b: reg_dp = dest; break;
		default: break;
	}
	TAKEN_CYCLES(8);
}

/* 0x1F TFR immediate */
static void op_tfr(void) {
	unsigned int postbyte;
	uint_least16_t source = 0xffff;
	BYTE_IMMEDIATE(0,postbyte);
	switch ((postbyte & 0xf0) >> 4) {
		case 0x00: source = reg_d; break;
		case 0x01: source = reg_x; break;
		case 0x02: source = reg_y; break;
		case 0x03: source = reg_u; break;
		case 0x04: source = reg_s; break;
		case 0x05: source = reg_pc; break;
		case 0x08: source = 0xff00 | (reg_a & 0xff); break;
		case 0x09: source = 0xff00 | (reg_b & 0xff); break;
		case 0x0a: source = 0xff00 | (reg_cc & 0xff); break;
		case 0x0b: source = 0xff00 | (reg_dp & 0xff); break;
		default: break;
	}
	switch (postbyte & 0x0f) {
		case 0x00: reg_d = source; break;
		case 0x01: reg_x = source; break;
		case 0x02: reg_y = source; break;
		case 0x03: reg_u = source; break;
		case 0x04: reg_s = source; break;
		case 0x05: reg_pc = source; break;
		case 0x08: reg_a = source; break;
		case 0x09: reg_b = source; break;
		case 0x0a: reg_cc = source; break;
		case 0x0b: reg_dp = source; break;
		default: break;
	}
	TAKEN_CYCLES(6);
}
