/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2007  Ciaran Anscomb
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

#include "types.h"
#include "logging.h"
#include "m6809.h"
#include "m6809_dasm.h"
#include "machine.h"
#include "sam.h"
#include "xroar.h"

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

#define CLR_HNZVC	do { reg_cc &= ~(CC_H|CC_N|CC_Z|CC_V|CC_C); } while (0)
#define CLR_NZV		do { reg_cc &= ~(CC_N|CC_Z|CC_V); } while (0)
#define CLR_NZVC	do { reg_cc &= ~(CC_N|CC_Z|CC_V|CC_C); } while (0)
#define CLR_Z		do { reg_cc &= ~(CC_Z); } while (0)
#define CLR_NZC		do { reg_cc &= ~(CC_N|CC_Z|CC_C); } while (0)
#define CLR_NVC		do { reg_cc &= ~(CC_N|CC_V|CC_C); } while (0)
#define CLR_ZC		do { reg_cc &= ~(CC_Z|CC_C); } while (0)

#define SET_Z(a)	do { if (!(a)) reg_cc |= CC_Z; } while (0)
#define SET_N8(a)	do { reg_cc |= (a&0x80)>>4; } while (0)
#define SET_N16(a)	do { reg_cc |= (a&0x8000)>>12; } while (0)
#define SET_H(a,b,r)	do { reg_cc |= ((a^b^r)&0x10)<<1; } while (0)
#define SET_C8(a)	do { reg_cc |= (a&0x100)>>8; } while (0)
#define SET_C16(a)	do { reg_cc |= (a&0x10000)>>16; } while (0)
#define SET_V8(a,b,r)	do { reg_cc |= ((a^b^r^(r>>1))&0x80)>>6; } while (0)
#define SET_V16(a,b,r)  do { reg_cc |= ((a^b^r^(r>>1))&0x8000)>>14; } while (0)
#define SET_NZ8(a)		do { SET_N8(a); SET_Z(a&0xff); } while (0)
#define SET_NZ16(a)		do { SET_N16(a);SET_Z(a&0xffff); } while (0)
#define SET_NZC8(a)		do { SET_N8(a); SET_Z(a&0xff);SET_C8(a); } while (0)
#define SET_NZVC8(a,b,r)	do { SET_N8(r); SET_Z(r&0xff);SET_V8(a,b,r);SET_C8(r); } while (0)
#define SET_NZVC16(a,b,r)	do { SET_N16(r);SET_Z(r&0xffff);SET_V16(a,b,r);SET_C16(r); } while (0)

/* CPU fetch/store goes via SAM */
#define fetch_byte(a) (sam_read_byte(a))
#define fetch_word(a) (fetch_byte(a) << 8 | fetch_byte((a)+1))
#define store_byte(a,v) do { sam_store_byte((a),(v)); } while (0)
/* This one only used to try and get correct timing: */
#define peek_byte(a) sam_peek_byte(a)

#define EA_DIRECT(a)	do { a = reg_dp << 8 | fetch_byte(reg_pc); IF_TRACE(m6809_dasm_byte(a & 0xff, reg_pc)) reg_pc += 1; TAKEN_CYCLES(1); } while (0)
#define EA_EXTENDED(a)	do { a = fetch_byte(reg_pc) << 8 | fetch_byte(reg_pc+1); IF_TRACE(m6809_dasm_byte(a >> 8, reg_pc); m6809_dasm_byte(a & 0xff, reg_pc + 1)) reg_pc += 2; TAKEN_CYCLES(1); } while (0)

/* These macros are designed to be "passed as an argument" to the op-code
 * macros.  */
#define BYTE_IMMEDIATE(a,v)	{ v = fetch_byte(reg_pc); IF_TRACE(m6809_dasm_byte(v, reg_pc)) reg_pc++; }
#define BYTE_DIRECT(a,v)	{ EA_DIRECT(a); v = fetch_byte(a); }
#define BYTE_INDEXED(a,v)	{ EA_INDEXED(a); v = fetch_byte(a); }
#define BYTE_EXTENDED(a,v)	{ EA_EXTENDED(a); v = fetch_byte(a); }
#define WORD_IMMEDIATE(a,v)	{ v = fetch_byte(reg_pc) << 8 | fetch_byte(reg_pc+1); IF_TRACE(m6809_dasm_byte(v >> 8, reg_pc); m6809_dasm_byte(v & 0xff, reg_pc + 1)) reg_pc += 2; }
#define WORD_DIRECT(a,v)	{ EA_DIRECT(a); v = fetch_byte(a) << 8 | fetch_byte(a+1); }
#define WORD_INDEXED(a,v)	{ EA_INDEXED(a); v = fetch_byte(a) << 8 | fetch_byte(a+1); }
#define WORD_EXTENDED(a,v)	{ EA_EXTENDED(a); v = fetch_byte(a) << 8 | fetch_byte(a+1); }

#define SHORT_RELATIVE(r)	{ BYTE_IMMEDIATE(0,r); r = sex(r); }
#define LONG_RELATIVE(r)	WORD_IMMEDIATE(0,r)

#define TAKEN_CYCLES(c) current_cycle += ((c) * sam_topaddr_cycles)

#define PUSHWORD(s,r)	{ s -= 2; store_byte(s+1, r & 0xff); store_byte(s, r >> 8); }
#define PUSHBYTE(s,r)	{ s--; store_byte(s, r); }
#define PULLBYTE(s,r)	{ r = fetch_byte(s); s++; }
#define PULLWORD(s,r)	{ r = fetch_byte(s) << 8; r |= fetch_byte(s+1); s += 2; }

#define PUSHR(s,a) { \
		unsigned int postbyte; \
		BYTE_IMMEDIATE(0,postbyte); \
		TAKEN_CYCLES(2); \
		peek_byte(s); \
		if (postbyte & 0x80) { PUSHWORD(s, reg_pc); } \
		if (postbyte & 0x40) { PUSHWORD(s, a); } \
		if (postbyte & 0x20) { PUSHWORD(s, reg_y); } \
		if (postbyte & 0x10) { PUSHWORD(s, reg_x); } \
		if (postbyte & 0x08) { PUSHBYTE(s, reg_dp); } \
		if (postbyte & 0x04) { PUSHBYTE(s, reg_b); } \
		if (postbyte & 0x02) { PUSHBYTE(s, reg_a); } \
		if (postbyte & 0x01) { PUSHBYTE(s, reg_cc); } \
	}

#define PULLR(s,a) { \
		unsigned int postbyte; \
		BYTE_IMMEDIATE(0,postbyte); \
		TAKEN_CYCLES(2); \
		if (postbyte & 0x01) { PULLBYTE(s, reg_cc); } \
		if (postbyte & 0x02) { PULLBYTE(s, reg_a); } \
		if (postbyte & 0x04) { PULLBYTE(s, reg_b); } \
		if (postbyte & 0x08) { PULLBYTE(s, reg_dp); } \
		if (postbyte & 0x10) { PULLWORD(s, reg_x); } \
		if (postbyte & 0x20) { PULLWORD(s, reg_y); } \
		if (postbyte & 0x40) { PULLWORD(s, a); } \
		if (postbyte & 0x80) { PULLWORD(s, reg_pc); } \
		peek_byte(s); \
	}

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

#define TEST_INTERRUPT(i,m,v,e,cm,cyc,nom) do { \
		if (!skip_register_push) /* SYNC */ \
			wait_for_interrupt = 0; \
		if (!(reg_cc & m)) { \
			IF_TRACE(LOG_DEBUG(0, "Interrupt " #i " taken\n")) \
			wait_for_interrupt = 0; \
			INTERRUPT_REGISTER_PUSH(e); \
			skip_register_push = 0; \
			reg_cc |= (cm); reg_pc = fetch_word(v); \
			TAKEN_CYCLES(cyc); \
		} \
	} while (0)

#define ARM_NMI do { nmi_armed = 1; } while (0)
#define DISARM_NMI do { nmi_armed = 0; } while (0)

/* MPU registers */
static unsigned int register_cc;
static uint8_t register_a, register_b;
static unsigned int register_dp;
static uint16_t	register_x, register_y, register_u, register_s;
static uint16_t register_pc;

#define reg_d ((reg_a&0xff) << 8 | (reg_b&0xff))
#define set_reg_d(v) do { reg_a = ((v)>>8)&0xff; reg_b = (v)&0xff; } while (0)

/* MPU interrupt state variables */
int halt, nmi, firq, irq;
static int wait_for_interrupt;
static int skip_register_push;
static int nmi_armed;

#define sex5(v) ((int)(((v) & 0x0f) - ((v) & 0x10)))
#define sex(v) ((int)(((v) & 0x7f) - ((v) & 0x80)))

/* ------------------------------------------------------------------------- */

#define EA_ROFF0(b,r) case (b): ea = (r); peek_byte(reg_pc)
#define EA_ROFF5P(b,r,o) case ((b)+(o)): ea = (r) + (o); peek_byte(reg_pc); TAKEN_CYCLES(1)
#define EA_ROFF5N(b,r,o) case ((b)+32+(o)): ea = (r) + (o); peek_byte(reg_pc); TAKEN_CYCLES(1)
#define EA_ROFF8(b,r) case (b): BYTE_IMMEDIATE(0,ea); ea = sex(ea) + (r); TAKEN_CYCLES(1)
#define EA_ROFF16(b,r) case (b): WORD_IMMEDIATE(0,ea); ea += (r); peek_byte(reg_pc); TAKEN_CYCLES(2)
#define EA_ROFFA(b,r) case (b): ea = (r) + sex(reg_a); peek_byte(reg_pc); TAKEN_CYCLES(1)
#define EA_ROFFB(b,r) case (b): ea = (r) + sex(reg_b); peek_byte(reg_pc); TAKEN_CYCLES(1)
#define EA_ROFFD(b,r) case (b): ea = (r) + reg_d; peek_byte(reg_pc); peek_byte(reg_pc+1); peek_byte(reg_pc+2); TAKEN_CYCLES(2)
#define EA_RI1(b,r) case (b): ea = (r); r += 1; peek_byte(reg_pc); TAKEN_CYCLES(2)
#define EA_RI2(b,r) case (b): ea = (r); r += 2; peek_byte(reg_pc); TAKEN_CYCLES(3)
#define EA_RD1(b,r) case (b): r -= 1; ea = (r); peek_byte(reg_pc); TAKEN_CYCLES(2)
#define EA_RD2(b,r) case (b): r -= 2; ea = (r); peek_byte(reg_pc); TAKEN_CYCLES(3)
#define EA_PCOFF8(b,r) case (b): BYTE_IMMEDIATE(0,ea); ea = sex(ea) + reg_pc; TAKEN_CYCLES(1)
#define EA_PCOFF16(b,r) case (b): WORD_IMMEDIATE(0,ea); ea += reg_pc; peek_byte(reg_pc); TAKEN_CYCLES(3)
/* Illegal instruction, but seems to work on a real 6809: */
#define EA_EXT case 0x8f: WORD_IMMEDIATE(0,ea); peek_byte(reg_pc); break
#define EA_EXTIND case 0x9f: WORD_IMMEDIATE(0,ea); peek_byte(reg_pc); ea = fetch_byte(ea) << 8 | fetch_byte(ea+1); TAKEN_CYCLES(1); break

#define EA_ROFF5(b,r) \
	case (b)+0: case (b)+1: case (b)+2: case (b)+3: case (b)+4: \
	case (b)+5: case (b)+6: case (b)+7: case (b)+8: case (b)+9: \
	case (b)+10: case (b)+11: case (b)+12: case (b)+13: case (b)+14: \
	case (b)+15: case (b)+16: case (b)+17: case (b)+18: case (b)+19: \
	case (b)+20: case (b)+21: case (b)+22: case (b)+23: case (b)+24: \
	case (b)+25: case (b)+26: case (b)+27: case (b)+28: case (b)+29: \
	case (b)+30: case (b)+31: \
		peek_byte(reg_pc); \
		TAKEN_CYCLES(1); \
		ea = (r) + sex5(postbyte)

#define EA_ALLR(m,b) m((b), reg_x); break; m((b)+0x20, reg_y); break; \
	m((b)+0x40, reg_u); break; m((b)+0x60, reg_s); break
#define EA_ALLRI(m,b) EA_IND(m,(b),reg_x); break; \
	EA_IND(m,(b)+0x20,reg_y); break; \
	EA_IND(m,(b)+0x40,reg_u); break; \
	EA_IND(m,(b)+0x60,reg_s); break
#define EA_IND(m,b,r) m(b,r); ea = fetch_byte(ea) << 8 | fetch_byte(ea+1); TAKEN_CYCLES(1)

#define EA_INDEXED(a) do { a = ea_indexed(); } while (0)

/* ------------------------------------------------------------------------- */

/* Operation templates */

#define OP_NEG(a) { uint_least16_t octet, result; a(addr,octet); result = ~(octet) + 1; CLR_NZVC; SET_NZVC8(0, octet, result); TAKEN_CYCLES(1); store_byte(addr,result); }
#define OP_COM(a) { uint_least16_t octet; a(addr,octet); octet = ~(octet); CLR_NZV; SET_NZ8(octet); reg_cc |= CC_C; TAKEN_CYCLES(1); store_byte(addr,octet); }
#define OP_LSR(a) { uint_least16_t octet; a(addr,octet); CLR_NZC; reg_cc |= (octet & CC_C); octet &= 0xff; octet >>= 1; SET_Z(octet); TAKEN_CYCLES(1); store_byte(addr,octet); }
#define OP_ROR(a) { uint_least16_t octet, result; a(addr,octet); result = (reg_cc & CC_C) << 7; CLR_NZC; reg_cc |= octet & CC_C; result |= (octet & 0xff) >> 1; SET_NZ8(result); TAKEN_CYCLES(1); store_byte(addr,result); }
#define OP_ASR(a) { uint_least16_t octet; CLR_NZC; a(addr,octet); reg_cc |= (octet & CC_C); octet = (octet & 0x80) | ((octet & 0xff) >> 1); SET_NZ8(octet); TAKEN_CYCLES(1); store_byte(addr,octet); }
#define OP_ASL(a) { uint_least16_t octet,result; a(addr,octet); result = octet << 1; CLR_NZVC; SET_NZVC8(octet, octet, result); TAKEN_CYCLES(1); store_byte(addr,result); }
#define OP_ROL(a) { uint_least16_t octet,result; a(addr,octet); result = (octet<<1)|(reg_cc & 1); CLR_NZVC; SET_NZVC8(octet, octet, result); TAKEN_CYCLES(1); store_byte(addr,result); }
#define OP_DEC(a) { uint_least16_t octet; a(addr,octet); octet--; octet &= 0xff; CLR_NZV; SET_NZ8(octet); if (octet == 0x7f) reg_cc |= CC_V; TAKEN_CYCLES(1); store_byte(addr,octet); }
#define OP_INC(a) { uint_least16_t octet; a(addr,octet); octet++; octet &= 0xff; CLR_NZV; SET_NZ8(octet); if (octet == 0x80) reg_cc |= CC_V; TAKEN_CYCLES(1); store_byte(addr,octet); }
#define OP_TST(a) { uint_least16_t octet; a(addr,octet); CLR_NZV; SET_NZ8(octet); TAKEN_CYCLES(2); }
#define OP_JMP(a) { a(addr); reg_pc = addr; }
#define OP_CLR(a) { uint_least16_t octet; a(addr,octet); TAKEN_CYCLES(1); store_byte(addr,0); CLR_NVC; reg_cc |= CC_Z; }

#define OP_NEGR(r) { uint_least16_t result = ~(r) + 1; CLR_NZVC; SET_NZVC8(0, r, result); r = result; peek_byte(reg_pc); }
#define OP_COMR(r) { r = ~(r); CLR_NZV; SET_NZ8(r); reg_cc |= CC_C; peek_byte(reg_pc); }
#define OP_LSRR(r) { CLR_NZC; reg_cc |= (r & CC_C); r >>= 1; SET_Z(r); peek_byte(reg_pc); }
#define OP_RORR(r) { uint_least16_t result = (reg_cc & CC_C) << 7; CLR_NZC; reg_cc |= r & CC_C; result |= (r & 0xff) >> 1; SET_NZ8(result); r = result; peek_byte(reg_pc); }
#define OP_ASRR(r) { CLR_NZC; reg_cc |= (r & CC_C); r = (r & 0x80) | ((r & 0xff) >> 1); SET_NZ8(r); peek_byte(reg_pc); }
#define OP_ASLR(r) { uint_least16_t result = r << 1; CLR_NZVC; SET_NZVC8(r, r, result); r = result; peek_byte(reg_pc); }
#define OP_ROLR(r) { uint_least16_t result = (reg_cc & CC_C) | (r << 1); CLR_NZVC; SET_NZVC8(r, r, result); r = result; peek_byte(reg_pc); }
/* Note: this used to be "r--; r &= 0xff;", but gcc optimises too much away */
#define OP_DECR(r) { r = (r - 1) & 0xff; CLR_NZV; SET_NZ8(r); if (r == 0x7f) reg_cc |= CC_V; peek_byte(reg_pc); }
#define OP_INCR(r) { r = (r + 1) & 0xff; CLR_NZV; SET_NZ8(r); if (r == 0x80) reg_cc |= CC_V; peek_byte(reg_pc); }
#define OP_TSTR(r) { CLR_NZV; SET_NZ8(r); peek_byte(reg_pc); }
#define OP_CLRR(r) { r = 0; CLR_NVC; reg_cc |= CC_Z; peek_byte(reg_pc); }

#define OP_SUBD(a) { uint_least32_t tmp = reg_d, octet, result; a(addr,octet); result = tmp - octet; CLR_NZVC; SET_NZVC16(tmp, octet, result); set_reg_d(result); TAKEN_CYCLES(1); }
#define OP_ADDD(a) { uint_least32_t tmp = reg_d, octet, result; a(addr,octet); result = tmp + octet; CLR_NZVC; SET_NZVC16(tmp, octet, result); set_reg_d(result); TAKEN_CYCLES(1); }
#define OP_CMPD(a) { uint_least32_t tmp = reg_d, octet, result; a(addr,octet); result = tmp - octet; CLR_NZVC; SET_NZVC16(tmp, octet, result); TAKEN_CYCLES(1); }
#define OP_LDD(a) { uint_least16_t tmp; a(addr,tmp); CLR_NZV; SET_NZ16(tmp); set_reg_d(tmp); }
#define OP_STD(a) { uint_least16_t tmp = reg_d; a(addr); store_byte(addr, reg_a); store_byte(addr+1, reg_b); CLR_NZV; SET_NZ16(tmp); }

#define OP_SUB16(r,a) { uint_least32_t octet, result; a(addr,octet); result = r - octet; CLR_NZVC; SET_NZVC16(r, octet, result); r = result; TAKEN_CYCLES(1); }
#define OP_ADD16(r,a) { uint_least32_t octet, result; a(addr,octet); result = r + octet; CLR_NZVC; SET_NZVC16(r, octet, result); r = result; TAKEN_CYCLES(1); }
#define OP_CMP16(r,a) { uint_least32_t octet, result; a(addr,octet); result = r - octet; CLR_NZVC; SET_NZVC16(r, octet, result); TAKEN_CYCLES(1); }
#define OP_LD16(r,a) { a(addr,r); CLR_NZV; SET_NZ16(r); }
#define OP_ST16(r,a) { a(addr); store_byte(addr, r >> 8); store_byte(addr+1, r & 0xff); CLR_NZV; SET_NZ16(r); }
#define OP_JSR(a) { a(addr); peek_byte(addr); PUSHWORD(reg_s, reg_pc); reg_pc = addr; }

#define OP_SUB(r,a) { uint_least16_t octet, result; a(addr,octet); result = r - octet; CLR_NZVC; SET_NZVC8(r, octet, result); r = result; }
#define OP_CMP(r,a) { uint_least16_t octet, result; a(addr,octet); result = r - octet; CLR_NZVC; SET_NZVC8(r, octet, result); }
#define OP_SBC(r,a) { uint_least16_t octet, result; a(addr,octet); result = r - octet - (reg_cc & CC_C); CLR_NZVC; SET_NZVC8(r, octet, result); r = result; }
#define OP_AND(r,a) { uint_least16_t octet; a(addr,octet); r &= octet; CLR_NZV; SET_NZ8(r); }
#define OP_BIT(r,a) { uint_least16_t octet, result; a(addr,octet); result = r & octet; CLR_NZV; SET_NZ8(result); }
#define OP_LD(r,a) { a(addr,r); CLR_NZV; SET_NZ8(r); }
#define OP_ST(r,a) { a(addr); store_byte(addr, r); CLR_NZV; SET_NZ8(r); }
#define OP_EOR(r,a) { uint_least16_t octet; a(addr,octet); r ^= octet; CLR_NZV; SET_NZ8(r); }
#define OP_ADC(r,a) { uint_least16_t octet, result; a(addr,octet); result = r + octet + (reg_cc & CC_C); CLR_HNZVC; SET_NZVC8(r, octet, result); SET_H(r, octet, result); r = result; }
#define OP_OR(r,a) { uint_least16_t octet; a(addr,octet); r |= octet; CLR_NZV; SET_NZ8(r); }
#define OP_ADD(r,a) { uint_least16_t octet, result; a(addr,octet); result = r + octet; CLR_HNZVC; SET_NZVC8(r, octet, result); SET_H(r, octet, result); r = result; }

#define BRANCHS(cond) { uint_least16_t RA; SHORT_RELATIVE(RA); \
		TAKEN_CYCLES(1); if (cond) { reg_pc += RA; } \
	}
#define BRANCHL(cond) { uint_least16_t RA; LONG_RELATIVE(RA); \
		if (cond) { reg_pc += RA; TAKEN_CYCLES(2); } \
		else { TAKEN_CYCLES(1); } \
	}

/* ------------------------------------------------------------------------- */

void m6809_init(void) {
}

void m6809_reset(void) {
	halt = nmi = firq = irq = 0;
	DISARM_NMI;
	wait_for_interrupt = 0;
	skip_register_push = 0;
	register_cc = 0xff;
	register_a = register_b = 0;
	register_dp = 0;
	register_x = register_y = register_u = register_s = 0;
	register_pc = fetch_word(0xfffe);
	m6809_dasm_reset();
}

void m6809_cycle(Cycle until) {
	__label__ restore_regs, switched_block_page2, switched_block_page3;
	uint_least16_t addr;
	unsigned int reg_cc = register_cc;
	uint8_t reg_a = register_a;
	uint8_t reg_b = register_b;
	unsigned int reg_dp = register_dp;
	uint16_t reg_x = register_x;
	uint16_t reg_y = register_y;
	uint16_t reg_u = register_u;
	uint16_t reg_s = register_s;
	uint16_t reg_pc = register_pc;

	/* Bit of a GCC-ism here: */
	auto uint16_t ea_indexed(void);
	uint16_t ea_indexed(void) {
		unsigned int ea;
		uint8_t postbyte;
		BYTE_IMMEDIATE(0,postbyte);
		switch (postbyte) {
			EA_ALLR(EA_ROFF5,   0x00);
			EA_ALLR(EA_RI1,     0x80);
			EA_ALLR(EA_RI2,     0x81); EA_ALLRI(EA_RI2,     0x91);
			EA_ALLR(EA_RD1,     0x82);
			EA_ALLR(EA_RD2,     0x83); EA_ALLRI(EA_RD2,     0x93);
			EA_ALLR(EA_ROFF0,   0x84); EA_ALLRI(EA_ROFF0,   0x94);
			EA_ALLR(EA_ROFFB,   0x85); EA_ALLRI(EA_ROFFB,   0x95);
			EA_ALLR(EA_ROFFA,   0x86); EA_ALLRI(EA_ROFFA,   0x96);
			EA_ALLR(EA_ROFF8,   0x88); EA_ALLRI(EA_ROFF8,   0x98);
			EA_ALLR(EA_ROFF16,  0x89); EA_ALLRI(EA_ROFF16,  0x99);
			EA_ALLR(EA_ROFFD,   0x8b); EA_ALLRI(EA_ROFFD,   0x9b);
			EA_ALLR(EA_PCOFF8,  0x8c); EA_ALLRI(EA_PCOFF8,  0x9c);
			EA_ALLR(EA_PCOFF16, 0x8d); EA_ALLRI(EA_PCOFF16, 0x9d);
			EA_EXT; EA_EXTIND;
			default: ea = 0; break;
		}
		return ea;
	}

	if (halt && !wait_for_interrupt) {
		while ((int)(current_cycle - until) < 0)
			TAKEN_CYCLES(1);
		goto restore_regs;
	}
	if (nmi_armed && nmi) {
		DISARM_NMI;
		TEST_INTERRUPT(nmi, 0, 0xfffc, CC_E, CC_F|CC_I, 7, "NMI");
		/* NMI is latched.  Assume that means latched only on
		 * high-to-low transition... */
		nmi = 0;
	}
	if (firq) {
		TEST_INTERRUPT(firq, CC_F, 0xfff6, 0, CC_F|CC_I, 7, "FIRQ");
		/* FIRQ and IRQ aren't latched: can remain set */
	}
	if (irq) {
		TEST_INTERRUPT(irq, CC_I, 0xfff8, CC_E, CC_I, 7, "IRQ");
		/* FIRQ and IRQ aren't latched: can remain set */
	}
	if (halt || wait_for_interrupt) {
		while ((int)(current_cycle - until) < 0)
			TAKEN_CYCLES(1);
		goto restore_regs;
	}

	while ((int)(current_cycle - until) < 0 && !wait_for_interrupt && !halt) {
		IF_TRACE(LOG_DEBUG(0, "%04x| ", reg_pc));
		/* Fetch op-code and process */
		{
			unsigned int op;
			BYTE_IMMEDIATE(0,op);
			switch(op) {
				/* 0x00 NEG direct */
				case 0x00:
				case 0x01: OP_NEG(BYTE_DIRECT); break;
				/* 0x01 NEGCOM direct (illegal) */
				case 0x02:
					   if (reg_cc & CC_C) {
						   OP_COM(BYTE_DIRECT);
					   } else {
						   OP_NEG(BYTE_DIRECT);
					   }
					   break;
				/* 0x03 COM direct */
				case 0x03: OP_COM(BYTE_DIRECT); break;
				/* 0x04 LSR direct */
				case 0x04:
				case 0x05: OP_LSR(BYTE_DIRECT); break;
				/* 0x06 ROR direct */
				case 0x06: OP_ROR(BYTE_DIRECT); break;
				/* 0x07 ASR direct */
				case 0x07: OP_ASR(BYTE_DIRECT); break;
				/* 0x08 ASL,LSL direct */
				case 0x08: OP_ASL(BYTE_DIRECT); break;
				/* 0x09 ROL direct */
				case 0x09: OP_ROL(BYTE_DIRECT); break;
				/* 0x0A DEC direct */
				case 0x0A:
				case 0x0B: OP_DEC(BYTE_DIRECT); break;
				/* 0x0C INC direct */
				case 0x0C: OP_INC(BYTE_DIRECT); break;
				/* 0x0D TST direct */
				case 0x0D: OP_TST(BYTE_DIRECT); break;
				/* 0x0E JMP direct */
				case 0x0E: OP_JMP(EA_DIRECT); break;
				/* 0x0F CLR direct */
				case 0x0F: OP_CLR(BYTE_DIRECT); break;
				/* 0x10 Page 2 */
				case 0x10: goto switched_block_page2; break;
				/* 0x11 Page 3 */
				case 0x11: goto switched_block_page3; break;
				/* 0x12 NOP inherent */
				case 0x12: peek_byte(reg_pc); break;
				/* 0x13 SYNC inherent */
				case 0x13:
					wait_for_interrupt = 1;
					peek_byte(reg_pc);
					TAKEN_CYCLES(3);
					break;
				/* 0x16 LBRA relative */
				case 0x16: BRANCHL(1); break;
				/* 0x17 LBSR relative */
				case 0x17: {
					uint_least16_t RA;
					uint16_t dest;
					LONG_RELATIVE(RA);
					dest = reg_pc + RA;
					TAKEN_CYCLES(1);
					peek_byte(dest);
					TAKEN_CYCLES(1);
					PUSHWORD(reg_s, reg_pc);
					reg_pc = dest;
				} break;
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
					peek_byte(reg_pc);
				} break;
				/* 0x1A ORCC immediate */
				case 0x1a: {
					unsigned int v;
					BYTE_IMMEDIATE(0,v);
					reg_cc |= v;
					peek_byte(reg_pc);
				} break;
				/* 0x1C ANDCC immediate */
				case 0x1c: {
					unsigned int v;
					BYTE_IMMEDIATE(0,v);
					reg_cc &= v;
					peek_byte(reg_pc);
				} break;
				/* 0x1D SEX inherent */
				case 0x1d: {
					uint_least16_t tmp;
					reg_a = (reg_b&0x80)?0xff:0;
					tmp = reg_d;
					CLR_NZV;
					SET_NZ16(tmp);
					peek_byte(reg_pc);
					}
					break;
				/* 0x1E EXG immediate */
				case 0x1e: {
					uint8_t postbyte;
					unsigned int tmp;
					BYTE_IMMEDIATE(0,postbyte);
					switch (postbyte) {
						case 0x01: case 0x10: tmp = reg_x; reg_x = reg_d; set_reg_d(tmp); break;
						case 0x02: case 0x20: tmp = reg_y; reg_y = reg_d; set_reg_d(tmp); break;
						case 0x03: case 0x30: tmp = reg_u; reg_u = reg_d; set_reg_d(tmp); break;
						case 0x04: case 0x40: tmp = reg_s; reg_s = reg_d; set_reg_d(tmp); break;
						case 0x05: case 0x50: tmp = reg_pc; reg_pc = reg_d; set_reg_d(tmp); break;
						case 0x06: case 0x60: reg_a = reg_b = 0xff; break;
						case 0x07: case 0x70: reg_a = reg_b = 0xff; break;
						case 0x08: case 0x80: reg_b = reg_a; reg_a = 0xff; break;
						case 0x09: case 0x90: reg_a = 0xff; break;
						case 0x0a: case 0xa0: tmp = reg_cc; reg_cc = reg_b; reg_a = 0xff; reg_b = tmp; break;
						case 0x0b: case 0xb0: tmp = reg_dp; reg_dp = reg_b; reg_a = 0xff; reg_b = tmp; break;
						case 0x0c: case 0xc0: reg_a = reg_b = 0xff; break;
						case 0x0d: case 0xd0: reg_a = reg_b = 0xff; break;
						case 0x0e: case 0xe0: reg_a = reg_b = 0xff; break;
						case 0x0f: case 0xf0: reg_a = reg_b = 0xff; break;
						case 0x12: case 0x21: tmp = reg_y; reg_y = reg_x; reg_x = tmp; break;
						case 0x13: case 0x31: tmp = reg_u; reg_u = reg_x; reg_x = tmp; break;
						case 0x14: case 0x41: tmp = reg_s; reg_s = reg_x; reg_x = tmp; break;
						case 0x15: case 0x51: tmp = reg_pc; reg_pc = reg_x; reg_x = tmp; break;
						case 0x16: case 0x61: reg_x = 0xffff; break;
						case 0x17: case 0x71: reg_x = 0xffff; break;
						case 0x18: case 0x81: tmp = reg_a; reg_a = reg_x & 0xff; reg_x = 0xff00 | tmp; break;
						case 0x19: case 0x91: tmp = reg_b; reg_b = reg_x & 0xff; reg_x = 0xff00 | tmp; break;
						case 0x1a: case 0xa1: tmp = reg_cc; reg_cc = reg_x & 0xff; reg_x = 0xff00 | tmp; break;
						case 0x1b: case 0xb1: tmp = reg_dp; reg_dp = reg_x & 0xff; reg_x = 0xff00 | tmp; break;
						case 0x1c: case 0xc1: reg_x = 0xffff; break;
						case 0x1d: case 0xd1: reg_x = 0xffff; break;
						case 0x1e: case 0xe1: reg_x = 0xffff; break;
						case 0x1f: case 0xf1: reg_x = 0xffff; break;
						case 0x23: case 0x32: tmp = reg_u; reg_u = reg_y; reg_y = tmp; break;
						case 0x24: case 0x42: tmp = reg_s; reg_s = reg_y; reg_y = tmp; break;
						case 0x25: case 0x52: tmp = reg_pc; reg_pc = reg_y; reg_y = tmp; break;
						case 0x26: case 0x62: reg_y = 0xffff; break;
						case 0x27: case 0x72: reg_y = 0xffff; break;
						case 0x28: case 0x82: tmp = reg_a; reg_a = reg_y & 0xff; reg_y = 0xff00 | tmp; break;
						case 0x29: case 0x92: tmp = reg_b; reg_b = reg_y & 0xff; reg_y = 0xff00 | tmp; break;
						case 0x2a: case 0xa2: tmp = reg_cc; reg_cc = reg_y & 0xff; reg_y = 0xff00 | tmp; break;
						case 0x2b: case 0xb2: tmp = reg_dp; reg_dp = reg_y & 0xff; reg_y = 0xff00 | tmp; break;
						case 0x2c: case 0xc2: reg_y = 0xffff; break;
						case 0x2d: case 0xd2: reg_y = 0xffff; break;
						case 0x2e: case 0xe2: reg_y = 0xffff; break;
						case 0x2f: case 0xf2: reg_y = 0xffff; break;
						case 0x34: case 0x43: tmp = reg_s; reg_s = reg_u; reg_u = tmp; break;
						case 0x35: case 0x53: tmp = reg_pc; reg_pc = reg_u; reg_u = tmp; break;
						case 0x36: case 0x63: reg_u = 0xffff; break;
						case 0x37: case 0x73: reg_u = 0xffff; break;
						case 0x38: case 0x83: tmp = reg_a; reg_a = reg_u & 0xff; reg_u = 0xff00 | tmp; break;
						case 0x39: case 0x93: tmp = reg_b; reg_b = reg_u & 0xff; reg_u = 0xff00 | tmp; break;
						case 0x3a: case 0xa3: tmp = reg_cc; reg_cc = reg_u & 0xff; reg_u = 0xff00 | tmp; break;
						case 0x3b: case 0xb3: tmp = reg_dp; reg_dp = reg_u & 0xff; reg_u = 0xff00 | tmp; break;
						case 0x3c: case 0xc3: reg_u = 0xffff; break;
						case 0x3d: case 0xd3: reg_u = 0xffff; break;
						case 0x3e: case 0xe3: reg_u = 0xffff; break;
						case 0x3f: case 0xf3: reg_u = 0xffff; break;
						case 0x45: case 0x54: tmp = reg_pc; reg_pc = reg_s; reg_s = tmp; break;
						case 0x46: case 0x64: reg_s = 0xffff; break;
						case 0x47: case 0x74: reg_s = 0xffff; break;
						case 0x48: case 0x84: tmp = reg_a; reg_a = reg_s & 0xff; reg_s = 0xff00 | tmp; break;
						case 0x49: case 0x94: tmp = reg_b; reg_b = reg_s & 0xff; reg_s = 0xff00 | tmp; break;
						case 0x4a: case 0xa4: tmp = reg_cc; reg_cc = reg_s & 0xff; reg_s = 0xff00 | tmp; break;
						case 0x4b: case 0xb4: tmp = reg_dp; reg_dp = reg_s & 0xff; reg_s = 0xff00 | tmp; break;
						case 0x4c: case 0xc4: reg_s = 0xffff; break;
						case 0x4d: case 0xd4: reg_s = 0xffff; break;
						case 0x4e: case 0xe4: reg_s = 0xffff; break;
						case 0x4f: case 0xf4: reg_s = 0xffff; break;
						case 0x56: case 0x65: reg_pc = 0xffff; break;
						case 0x57: case 0x75: reg_pc = 0xffff; break;
						case 0x58: case 0x85: tmp = reg_a; reg_a = reg_pc & 0xff; reg_pc = 0xff00 | tmp; break;
						case 0x59: case 0x95: tmp = reg_b; reg_b = reg_pc & 0xff; reg_pc = 0xff00 | tmp; break;
						case 0x5a: case 0xa5: tmp = reg_cc; reg_cc = reg_pc & 0xff; reg_pc = 0xff00 | tmp; break;
						case 0x5b: case 0xb5: tmp = reg_dp; reg_dp = reg_pc & 0xff; reg_pc = 0xff00 | tmp; break;
						case 0x5c: case 0xc5: reg_pc = 0xffff; break;
						case 0x5d: case 0xd5: reg_pc = 0xffff; break;
						case 0x5e: case 0xe5: reg_pc = 0xffff; break;
						case 0x5f: case 0xf5: reg_pc = 0xffff; break;
						case 0x68: case 0x86: reg_a = 0xff; break;
						case 0x69: case 0x96: reg_b = 0xff; break;
						case 0x6a: case 0xa6: reg_cc = 0xff; break;
						case 0x6b: case 0xb6: reg_dp = 0xff; break;
						case 0x78: case 0x87: reg_a = 0xff; break;
						case 0x79: case 0x97: reg_b = 0xff; break;
						case 0x7a: case 0xa7: reg_cc = 0xff; break;
						case 0x7b: case 0xb7: reg_dp = 0xff; break;
						case 0x89: case 0x98: tmp = reg_b; reg_b = reg_a; reg_a = tmp; break;
						case 0x8a: case 0xa8: tmp = reg_cc; reg_cc = reg_a; reg_a = tmp; break;
						case 0x8b: case 0xb8: tmp = reg_dp; reg_dp = reg_a; reg_a = tmp; break;
						case 0x8c: case 0xc8: reg_a = 0xff; break;
						case 0x8d: case 0xd8: reg_a = 0xff; break;
						case 0x8e: case 0xe8: reg_a = 0xff; break;
						case 0x8f: case 0xf8: reg_a = 0xff; break;
						case 0x9a: case 0xa9: tmp = reg_cc; reg_cc = reg_b; reg_b = tmp; break;
						case 0x9b: case 0xb9: tmp = reg_dp; reg_dp = reg_b; reg_b = tmp; break;
						case 0x9c: case 0xc9: reg_b = 0xff; break;
						case 0x9d: case 0xd9: reg_b = 0xff; break;
						case 0x9e: case 0xe9: reg_b = 0xff; break;
						case 0x9f: case 0xf9: reg_b = 0xff; break;
						case 0xab: case 0xba: tmp = reg_dp; reg_dp = reg_cc; reg_cc = tmp; break;
						case 0xac: case 0xca: reg_cc = 0xff; break;
						case 0xad: case 0xda: reg_cc = 0xff; break;
						case 0xae: case 0xea: reg_cc = 0xff; break;
						case 0xaf: case 0xfa: reg_cc = 0xff; break;
						case 0xbc: case 0xcb: reg_dp = 0xff; break;
						case 0xbd: case 0xdb: reg_dp = 0xff; break;
						case 0xbe: case 0xeb: reg_dp = 0xff; break;
						case 0xbf: case 0xfb: reg_dp = 0xff; break;
						default: break;
					}
					TAKEN_CYCLES(6);
				} break;
				/* 0x1F TFR immediate */
				case 0x1f: {
					uint8_t postbyte;
					BYTE_IMMEDIATE(0,postbyte);
					switch (postbyte) {
						case 0x01: reg_x = reg_d; break;
						case 0x02: reg_y = reg_d; break;
						case 0x03: reg_u = reg_d; break;
						case 0x04: reg_s = reg_d; break;
						case 0x05: reg_pc = reg_d; break;
						case 0x08: reg_a = reg_b; break;
						case 0x09: break;
						case 0x0a: reg_cc = reg_b; break;
						case 0x0b: reg_dp = reg_b; break;
						case 0x10: set_reg_d(reg_x); break;
						case 0x12: reg_y = reg_x; break;
						case 0x13: reg_u = reg_x; break;
						case 0x14: reg_s = reg_x; break;
						case 0x15: reg_pc = reg_x; break;
						case 0x18: reg_a = reg_x & 0xff; break;
						case 0x19: reg_b = reg_x & 0xff; break;
						case 0x1a: reg_cc = reg_x & 0xff; break;
						case 0x1b: reg_dp = reg_x & 0xff; break;
						case 0x20: set_reg_d(reg_y); break;
						case 0x21: reg_x = reg_y; break;
						case 0x23: reg_u = reg_y; break;
						case 0x24: reg_s = reg_y; break;
						case 0x25: reg_pc = reg_y; break;
						case 0x28: reg_a = reg_y & 0xff; break;
						case 0x29: reg_b = reg_y & 0xff; break;
						case 0x2a: reg_cc = reg_y & 0xff; break;
						case 0x2b: reg_dp = reg_y & 0xff; break;
						case 0x30: set_reg_d(reg_u); break;
						case 0x31: reg_x = reg_u; break;
						case 0x32: reg_y = reg_u; break;
						case 0x34: reg_s = reg_u; break;
						case 0x35: reg_pc = reg_u; break;
						case 0x38: reg_a = reg_u & 0xff; break;
						case 0x39: reg_b = reg_u & 0xff; break;
						case 0x3a: reg_cc = reg_u & 0xff; break;
						case 0x3b: reg_dp = reg_u & 0xff; break;
						case 0x40: set_reg_d(reg_s); break;
						case 0x41: reg_x = reg_s; break;
						case 0x42: reg_y = reg_s; break;
						case 0x43: reg_u = reg_s; break;
						case 0x45: reg_pc = reg_s; break;
						case 0x48: reg_a = reg_s & 0xff; break;
						case 0x49: reg_b = reg_s & 0xff; break;
						case 0x4a: reg_cc = reg_s & 0xff; break;
						case 0x4b: reg_dp = reg_s & 0xff; break;
						case 0x50: set_reg_d(reg_pc); break;
						case 0x51: reg_x = reg_pc; break;
						case 0x52: reg_y = reg_pc; break;
						case 0x53: reg_u = reg_pc; break;
						case 0x54: reg_s = reg_pc; break;
						case 0x58: reg_a = reg_pc & 0xff; break;
						case 0x59: reg_b = reg_pc & 0xff; break;
						case 0x5a: reg_cc = reg_pc & 0xff; break;
						case 0x5b: reg_dp = reg_pc & 0xff; break;
						case 0x60: reg_a = reg_b = 0xff; break;
						case 0x61: reg_x = 0xffff; break;
						case 0x62: reg_y = 0xffff; break;
						case 0x63: reg_u = 0xffff; break;
						case 0x64: reg_s = 0xffff; break;
						case 0x65: reg_pc = 0xffff; break;
						case 0x68: reg_a = 0xff; break;
						case 0x69: reg_b = 0xff; break;
						case 0x6a: reg_cc = 0xff; break;
						case 0x6b: reg_dp = 0xff; break;
						case 0x70: reg_a = reg_b = 0xff; break;
						case 0x71: reg_x = 0xffff; break;
						case 0x72: reg_y = 0xffff; break;
						case 0x73: reg_u = 0xffff; break;
						case 0x74: reg_s = 0xffff; break;
						case 0x75: reg_pc = 0xffff; break;
						case 0x78: reg_a = 0xff; break;
						case 0x79: reg_b = 0xff; break;
						case 0x7a: reg_cc = 0xff; break;
						case 0x7b: reg_dp = 0xff; break;
						case 0x80: reg_b = reg_a; reg_a = 0xff; break;
						case 0x81: reg_x = 0xff00 | reg_a; break;
						case 0x82: reg_y = 0xff00 | reg_a; break;
						case 0x83: reg_u = 0xff00 | reg_a; break;
						case 0x84: reg_s = 0xff00 | reg_a; break;
						case 0x85: reg_pc = 0xff00 | reg_a; break;
						case 0x89: reg_b = reg_a; break;
						case 0x8a: reg_cc = reg_a; break;
						case 0x8b: reg_dp = reg_a; break;
						case 0x90: reg_a = 0xff; break;
						case 0x91: reg_x = 0xff00 | reg_b; break;
						case 0x92: reg_y = 0xff00 | reg_b; break;
						case 0x93: reg_u = 0xff00 | reg_b; break;
						case 0x94: reg_s = 0xff00 | reg_b; break;
						case 0x95: reg_pc = 0xff00 | reg_b; break;
						case 0x98: reg_a = reg_b; break;
						case 0x9a: reg_cc = reg_b; break;
						case 0x9b: reg_dp = reg_b; break;
						case 0xa0: reg_a = 0xff; reg_b = reg_cc; break;
						case 0xa1: reg_x = 0xff00 | reg_cc; break;
						case 0xa2: reg_y = 0xff00 | reg_cc; break;
						case 0xa3: reg_u = 0xff00 | reg_cc; break;
						case 0xa4: reg_s = 0xff00 | reg_cc; break;
						case 0xa5: reg_pc = 0xff00 | reg_cc; break;
						case 0xa8: reg_a = reg_cc; break;
						case 0xa9: reg_b = reg_cc; break;
						case 0xab: reg_dp = reg_cc; break;
						case 0xb0: reg_a = 0xff; reg_b = reg_dp; break;
						case 0xb1: reg_x = 0xff00 | reg_dp; break;
						case 0xb2: reg_y = 0xff00 | reg_dp; break;
						case 0xb3: reg_u = 0xff00 | reg_dp; break;
						case 0xb4: reg_s = 0xff00 | reg_dp; break;
						case 0xb5: reg_pc = 0xff00 | reg_dp; break;
						case 0xb8: reg_a = reg_dp; break;
						case 0xb9: reg_b = reg_dp; break;
						case 0xba: reg_cc = reg_dp; break;
						case 0xc0: reg_a = reg_b = 0xff; break;
						case 0xc1: reg_x = 0xffff; break;
						case 0xc2: reg_y = 0xffff; break;
						case 0xc3: reg_u = 0xffff; break;
						case 0xc4: reg_s = 0xffff; break;
						case 0xc5: reg_pc = 0xffff; break;
						case 0xc8: reg_a = 0xff; break;
						case 0xc9: reg_b = 0xff; break;
						case 0xca: reg_cc = 0xff; break;
						case 0xcb: reg_dp = 0xff; break;
						case 0xd0: reg_a = reg_b = 0xff; break;
						case 0xd1: reg_x = 0xffff; break;
						case 0xd2: reg_y = 0xffff; break;
						case 0xd3: reg_u = 0xffff; break;
						case 0xd4: reg_s = 0xffff; break;
						case 0xd5: reg_pc = 0xffff; break;
						case 0xd8: reg_a = 0xff; break;
						case 0xd9: reg_b = 0xff; break;
						case 0xda: reg_cc = 0xff; break;
						case 0xdb: reg_dp = 0xff; break;
						case 0xe0: reg_a = reg_b = 0xff; break;
						case 0xe1: reg_x = 0xffff; break;
						case 0xe2: reg_y = 0xffff; break;
						case 0xe3: reg_u = 0xffff; break;
						case 0xe4: reg_s = 0xffff; break;
						case 0xe5: reg_pc = 0xffff; break;
						case 0xe8: reg_a = 0xff; break;
						case 0xe9: reg_b = 0xff; break;
						case 0xea: reg_cc = 0xff; break;
						case 0xeb: reg_dp = 0xff; break;
						case 0xf0: reg_a = reg_b = 0xff; break;
						case 0xf1: reg_x = 0xffff; break;
						case 0xf2: reg_y = 0xffff; break;
						case 0xf3: reg_u = 0xffff; break;
						case 0xf4: reg_s = 0xffff; break;
						case 0xf5: reg_pc = 0xffff; break;
						case 0xf8: reg_a = 0xff; break;
						case 0xf9: reg_b = 0xff; break;
						case 0xfa: reg_cc = 0xff; break;
						case 0xfb: reg_dp = 0xff; break;
						default: break;
					}
					TAKEN_CYCLES(4);
				} break;
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
					TAKEN_CYCLES(1);
					break;
				/* 0x31 LEAY indexed */
				case 0x31: EA_INDEXED(reg_y);
					CLR_Z;
					SET_Z(reg_y);
					TAKEN_CYCLES(1);
					break;
				/* 0x32 LEAS indexed */
				case 0x32: EA_INDEXED(reg_s);
					TAKEN_CYCLES(1);
					ARM_NMI;  /* XXX: Really? */
					break;
				/* 0x33 LEAU indexed */
				case 0x33: EA_INDEXED(reg_u);
					TAKEN_CYCLES(1);
					break;
				/* 0x34 PSHS immediate */
				case 0x34: PUSHR(reg_s, reg_u); break;
				/* 0x35 PULS immediate */
				case 0x35: PULLR(reg_s, reg_u); break;
				/* 0x36 PSHU immediate */
				case 0x36: PUSHR(reg_u, reg_s); break;
				/* 0x37 PULU immediate */
				case 0x37: PULLR(reg_u, reg_s); break;
				/* 0x39 RTS inherent */
				case 0x39:
					peek_byte(reg_pc);
					PULLWORD(reg_s, reg_pc);
					TAKEN_CYCLES(1);
					break;
				/* 0x3A ABX inherent */
				case 0x3a:
					reg_x += reg_b;
					peek_byte(reg_pc);
					TAKEN_CYCLES(1);
					break;
				/* 0x3B RTI inherent */
				case 0x3b:
					peek_byte(reg_pc);
					PULLBYTE(reg_s, reg_cc);
					if (reg_cc & CC_E) {
						PULLBYTE(reg_s, reg_a);
						PULLBYTE(reg_s, reg_b);
						PULLBYTE(reg_s, reg_dp);
						PULLWORD(reg_s, reg_x);
						PULLWORD(reg_s, reg_y);
						PULLWORD(reg_s, reg_u);
						PULLWORD(reg_s, reg_pc);
					} else {
						PULLWORD(reg_s, reg_pc);
					}
					ARM_NMI;
					peek_byte(reg_s);
					break;
				/* 0x3C CWAI immediate */
				case 0x3c: {
					unsigned int v;
					BYTE_IMMEDIATE(0,v);
					reg_cc &= v;
					peek_byte(reg_pc);
					TAKEN_CYCLES(1);
					//INTERRUPT_REGISTER_PUSH(1);
					wait_for_interrupt = 1;
					skip_register_push = 1;
					TAKEN_CYCLES(1);
				} break;
				/* 0x3D MUL inherent */
				case 0x3d: {
					uint16_t tmp = reg_a * reg_b;
					set_reg_d(tmp);
					CLR_ZC;
					SET_Z(tmp);
					if (tmp & 0x80)
						reg_cc |= CC_C;
					peek_byte(reg_pc);
					TAKEN_CYCLES(9);
					}
					break;
				/* 0x3E RESET (undocumented) */
				case 0x3e:
					m6809_reset();
					TAKEN_CYCLES(1);
					break;
				/* 0x3F SWI inherent */
				case 0x3f:
					reg_cc |= CC_E;
					peek_byte(reg_pc);
					TAKEN_CYCLES(1);
					PUSHWORD(reg_s, reg_pc);
					PUSHWORD(reg_s, reg_u);
					PUSHWORD(reg_s, reg_y);
					PUSHWORD(reg_s, reg_x);
					PUSHBYTE(reg_s, reg_dp);
					PUSHBYTE(reg_s, reg_b);
					PUSHBYTE(reg_s, reg_a);
					PUSHBYTE(reg_s, reg_cc);
					reg_cc |= CC_F|CC_I;
					TAKEN_CYCLES(1);
					reg_pc = fetch_word(0xfffa);
					TAKEN_CYCLES(1);
					break;
				/* 0x40 NEGA inherent */
				case 0x40:
				case 0x41: OP_NEGR(reg_a); break;
				/* 0x43 COMA inherent */
				case 0x42:
				case 0x43: OP_COMR(reg_a); break;
				/* 0x44 LSRA inherent */
				case 0x44:
				case 0x45: OP_LSRR(reg_a); break;
				/* 0x46 RORA inherent */
				case 0x46: OP_RORR(reg_a); break;
				/* 0x47 ASRA inherent */
				case 0x47: OP_ASRR(reg_a); break;
				/* 0x48 ASLA,LSLA inherent */
				case 0x48: OP_ASLR(reg_a); break;
				/* 0x49 ROLA inherent */
				case 0x49: OP_ROLR(reg_a); break;
				/* 0x4A DECA inherent */
				case 0x4a:
				case 0x4b: OP_DECR(reg_a); break;
				/* 0x4C INCA inherent */
				case 0x4c: OP_INCR(reg_a); break;
				/* 0x4D TSTA inherent */
				case 0x4d: OP_TSTR(reg_a); break;
				/* 0x4F CLRA inherent */
				case 0x4f: OP_CLRR(reg_a); break;
				/* 0x50 NEGB inherent */
				case 0x50:
				case 0x51: OP_NEGR(reg_b); break;
				/* 0x53 COMB inherent */
				case 0x52:
				case 0x53: OP_COMR(reg_b); break;
				/* 0x54 LSRB inherent */
				case 0x54:
				case 0x55: OP_LSRR(reg_b); break;
				/* 0x56 RORB inherent */
				case 0x56: OP_RORR(reg_b); break;
				/* 0x57 ASRB inherent */
				case 0x57: OP_ASRR(reg_b); break;
				/* 0x58 ASLB,LSLB inherent */
				case 0x58: OP_ASLR(reg_b); break;
				/* 0x59 ROLB inherent */
				case 0x59: OP_ROLR(reg_b); break;
				/* 0x5A DECB inherent */
				case 0x5a:
				case 0x5b: OP_DECR(reg_b); break;
				/* 0x5C INCB inherent */
				case 0x5c: OP_INCR(reg_b); break;
				/* 0x5D TSTB inherent */
				case 0x5d: OP_TSTR(reg_b); break;
				/* 0x5F CLRB inherent */
				case 0x5e:
				case 0x5f: OP_CLRR(reg_b); break;
				/* 0x60 NEG indexed */
				case 0x60:
				case 0x61: OP_NEG(BYTE_INDEXED); break;
				/* 0x63 COM indexed */
				case 0x62:
				case 0x63: OP_COM(BYTE_INDEXED); break;
				/* 0x64 LSR indexed */
				case 0x64:
				case 0x65: OP_LSR(BYTE_INDEXED); break;
				/* 0x66 ROR indexed */
				case 0x66: OP_ROR(BYTE_INDEXED); break;
				/* 0x67 ASR indexed */
				case 0x67: OP_ASR(BYTE_INDEXED); break;
				/* 0x68 ASL,LSL indexed */
				case 0x68: OP_ASL(BYTE_INDEXED); break;
				/* 0x69 ROL indexed */
				case 0x69: OP_ROL(BYTE_INDEXED); break;
				/* 0x6A DEC indexed */
				case 0x6a:
				case 0x6b: OP_DEC(BYTE_INDEXED); break;
				/* 0x6C INC indexed */
				case 0x6c: OP_INC(BYTE_INDEXED); break;
				/* 0x6D TST indexed */
				case 0x6d: OP_TST(BYTE_INDEXED); break;
				/* 0x6E JMP indexed */
				case 0x6e: OP_JMP(EA_INDEXED); break;
				/* 0x6F CLR indexed */
				case 0x6f: OP_CLR(BYTE_INDEXED); break;
				/* 0x70 NEG extended */
				case 0x70:
				case 0x71: OP_NEG(BYTE_EXTENDED); break;
				/* 0x73 COM extended */
				case 0x72:
				case 0x73: OP_COM(BYTE_EXTENDED); break;
				/* 0x74 LSR extended */
				case 0x74:
				case 0x75: OP_LSR(BYTE_EXTENDED); break;
				/* 0x76 ROR extended */
				case 0x76: OP_ROR(BYTE_EXTENDED); break;
				/* 0x77 ASR extended */
				case 0x77: OP_ASR(BYTE_EXTENDED); break;
				/* 0x78 ASL,LSL extended */
				case 0x78: OP_ASL(BYTE_EXTENDED); break;
				/* 0x79 ROL extended */
				case 0x79: OP_ROL(BYTE_EXTENDED); break;
				/* 0x7A DEC extended */
				case 0x7a:
				case 0x7b: OP_DEC(BYTE_EXTENDED); break;
				/* 0x7C INC extended */
				case 0x7c: OP_INC(BYTE_EXTENDED); break;
				/* 0x7D TST extended */
				case 0x7d: OP_TST(BYTE_EXTENDED); break;
				/* 0x7E JMP extended */
				case 0x7e: OP_JMP(EA_EXTENDED); break;
				/* 0x7F CLR extended */
				case 0x7f: OP_CLR(BYTE_EXTENDED); break;
				/* 0x80 SUBA immediate */
				case 0x80: OP_SUB(reg_a, BYTE_IMMEDIATE); break;
				/* 0x81 CMPA immediate */
				case 0x81: OP_CMP(reg_a, BYTE_IMMEDIATE); break;
				/* 0x82 SBCA immediate */
				case 0x82: OP_SBC(reg_a, BYTE_IMMEDIATE); break;
				/* 0x83 SUBD immediate */
				case 0x83: OP_SUBD(WORD_IMMEDIATE); break;
				/* 0x84 ANDA immediate */
				case 0x84: OP_AND(reg_a, BYTE_IMMEDIATE); break;
				/* 0x85 BITA immediate */
				case 0x85: OP_BIT(reg_a, BYTE_IMMEDIATE); break;
				/* 0x86 LDA immediate */
				case 0x86: OP_LD(reg_a, BYTE_IMMEDIATE); break;
				/* 0x88 EORA immediate */
				case 0x88: OP_EOR(reg_a, BYTE_IMMEDIATE); break;
				/* 0x89 ADCA immediate */
				case 0x89: OP_ADC(reg_a, BYTE_IMMEDIATE); break;
				/* 0x8A ORA immediate */
				case 0x8a: OP_OR(reg_a, BYTE_IMMEDIATE); break;
				/* 0x8B ADDA immediate */
				case 0x8b: OP_ADD(reg_a, BYTE_IMMEDIATE); break;
				/* 0x8C CMPX immediate */
				case 0x8c: OP_CMP16(reg_x, WORD_IMMEDIATE); break;
				/* 0x8D BSR relative */
				case 0x8d: {
					uint_least16_t RA;
					uint16_t dest;
					SHORT_RELATIVE(RA);
					dest = reg_pc + RA;
					peek_byte(dest);
					TAKEN_CYCLES(1);
					PUSHWORD(reg_s, reg_pc);
					reg_pc = dest;
				}
				break;
				/* 0x8E LDX immediate */
				case 0x8e: OP_LD16(reg_x, WORD_IMMEDIATE); break;
				/* 0x90 SUBA direct */
				case 0x90: OP_SUB(reg_a, BYTE_DIRECT); break;
				/* 0x91 CMPA direct */
				case 0x91: OP_CMP(reg_a, BYTE_DIRECT); break;
				/* 0x92 SBCA direct */
				case 0x92: OP_SBC(reg_a, BYTE_DIRECT); break;
				/* 0x93 SUBD direct */
				case 0x93: OP_SUBD(WORD_DIRECT); break;
				/* 0x94 ANDA direct */
				case 0x94: OP_AND(reg_a, BYTE_DIRECT); break;
				/* 0x95 BITA direct */
				case 0x95: OP_BIT(reg_a, BYTE_DIRECT); break;
				/* 0x96 LDA direct */
				case 0x96: OP_LD(reg_a, BYTE_DIRECT); break;
				/* 0x97 STA direct */
				case 0x97: OP_ST(reg_a, EA_DIRECT); break;
				/* 0x98 EORA direct */
				case 0x98: OP_EOR(reg_a, BYTE_DIRECT); break;
				/* 0x99 ADCA direct */
				case 0x99: OP_ADC(reg_a, BYTE_DIRECT); break;
				/* 0x9A ORA direct */
				case 0x9a: OP_OR(reg_a, BYTE_DIRECT); break;
				/* 0x9B ADDA direct */
				case 0x9b: OP_ADD(reg_a, BYTE_DIRECT); break;
				/* 0x9C CMPX direct */
				case 0x9c: OP_CMP16(reg_x, WORD_DIRECT); break;
				/* 0x9D JSR direct */
				case 0x9d: OP_JSR(EA_DIRECT); break;
				/* 0x9E LDX direct */
				case 0x9e: OP_LD16(reg_x, WORD_DIRECT); break;
				/* 0x9F STX direct */
				case 0x9f: OP_ST16(reg_x, EA_DIRECT); break;
				/* 0xA0 SUBA indexed */
				case 0xa0: OP_SUB(reg_a, BYTE_INDEXED); break;
				/* 0xA1 CMPA indexed */
				case 0xa1: OP_CMP(reg_a, BYTE_INDEXED); break;
				/* 0xA2 SBCA indexed */
				case 0xa2: OP_SBC(reg_a, BYTE_INDEXED); break;
				/* 0xA3 SUBD indexed */
				case 0xa3: OP_SUBD(WORD_INDEXED); break;
				/* 0xA4 ANDA indexed */
				case 0xa4: OP_AND(reg_a, BYTE_INDEXED); break;
				/* 0xA5 BITA indexed */
				case 0xa5: OP_BIT(reg_a, BYTE_INDEXED); break;
				/* 0xA6 LDA indexed */
				case 0xa6: OP_LD(reg_a, BYTE_INDEXED); break;
				/* 0xA7 STA indexed */
				case 0xa7: OP_ST(reg_a, EA_INDEXED); break;
				/* 0xA8 EORA indexed */
				case 0xa8: OP_EOR(reg_a, BYTE_INDEXED); break;
				/* 0xA9 ADCA indexed */
				case 0xa9: OP_ADC(reg_a, BYTE_INDEXED); break;
				/* 0xAA ORA indexed */
				case 0xaa: OP_OR(reg_a, BYTE_INDEXED); break;
				/* 0xAB ADDA indexed */
				case 0xab: OP_ADD(reg_a, BYTE_INDEXED); break;
				/* 0xAC CMPX indexed */
				case 0xac: OP_CMP16(reg_x, WORD_INDEXED); break;
				/* 0xAD JSR indexed */
				case 0xad: OP_JSR(EA_INDEXED); break;
				/* 0xAE LDX indexed */
				case 0xae: OP_LD16(reg_x, WORD_INDEXED); break;
				/* 0xAF STX indexed */
				case 0xaf: OP_ST16(reg_x, EA_INDEXED); break;
				/* 0xB0 SUBA extended */
				case 0xb0: OP_SUB(reg_a, BYTE_EXTENDED); break;
				/* 0xB1 CMPA extended */
				case 0xb1: OP_CMP(reg_a, BYTE_EXTENDED); break;
				/* 0xB2 SBCA extended */
				case 0xb2: OP_SBC(reg_a, BYTE_EXTENDED); break;
				/* 0xB3 SUBD extended */
				case 0xb3: OP_SUBD(WORD_EXTENDED); break;
				/* 0xB4 ANDA extended */
				case 0xb4: OP_AND(reg_a, BYTE_EXTENDED); break;
				/* 0xB5 BITA extended */
				case 0xb5: OP_BIT(reg_a, BYTE_EXTENDED); break;
				/* 0xB6 LDA extended */
				case 0xb6: OP_LD(reg_a, BYTE_EXTENDED); break;
				/* 0xB7 STA extended */
				case 0xb7: OP_ST(reg_a, EA_EXTENDED); break;
				/* 0xB8 EORA extended */
				case 0xb8: OP_EOR(reg_a, BYTE_EXTENDED); break;
				/* 0xB9 ADCA extended */
				case 0xb9: OP_ADC(reg_a, BYTE_EXTENDED); break;
				/* 0xBA ORA extended */
				case 0xba: OP_OR(reg_a, BYTE_EXTENDED); break;
				/* 0xBB ADDA extended */
				case 0xbb: OP_ADD(reg_a, BYTE_EXTENDED); break;
				/* 0xBC CMPX extended */
				case 0xbc: OP_CMP16(reg_x, WORD_EXTENDED); break;
				/* 0xBD JSR extended */
				case 0xbd: OP_JSR(EA_EXTENDED); break;
				/* 0xBE LDX extended */
				case 0xbe: OP_LD16(reg_x, WORD_EXTENDED); break;
				/* 0xBF STX extended */
				case 0xbf: OP_ST16(reg_x, EA_EXTENDED); break;
				/* 0xC0 SUBB immediate */
				case 0xc0: OP_SUB(reg_b, BYTE_IMMEDIATE); break;
				/* 0xC1 CMPB immediate */
				case 0xc1: OP_CMP(reg_b, BYTE_IMMEDIATE); break;
				/* 0xC2 SBCB immediate */
				case 0xc2: OP_SBC(reg_b, BYTE_IMMEDIATE); break;
				/* 0xC3 ADDD immediate */
				case 0xc3: OP_ADDD(WORD_IMMEDIATE); break;
				/* 0xC4 ANDB immediate */
				case 0xc4: OP_AND(reg_b, BYTE_IMMEDIATE); break;
				/* 0xC5 BITB immediate */
				case 0xc5: OP_BIT(reg_b, BYTE_IMMEDIATE); break;
				/* 0xC6 LDB immediate */
				case 0xc6: OP_LD(reg_b, BYTE_IMMEDIATE); break;
				/* 0xC8 EORB immediate */
				case 0xc8: OP_EOR(reg_b, BYTE_IMMEDIATE); break;
				/* 0xC9 ADCB immediate */
				case 0xc9: OP_ADC(reg_b, BYTE_IMMEDIATE); break;
				/* 0xCA ORB immediate */
				case 0xca: OP_OR(reg_b, BYTE_IMMEDIATE); break;
				/* 0xCB ADDB immediate */
				case 0xcb: OP_ADD(reg_b, BYTE_IMMEDIATE); break;
				/* 0xCC LDD immediate */
				case 0xcc: OP_LDD(WORD_IMMEDIATE); break;
				/* 0xCE LDU immediate */
				case 0xce: OP_LD16(reg_u, WORD_IMMEDIATE); break;
				/* 0xD0 SUBB direct */
				case 0xd0: OP_SUB(reg_b, BYTE_DIRECT); break;
				/* 0xD1 CMPB direct */
				case 0xd1: OP_CMP(reg_b, BYTE_DIRECT); break;
				/* 0xD2 SBCB direct */
				case 0xd2: OP_SBC(reg_b, BYTE_DIRECT); break;
				/* 0xD3 ADDD direct */
				case 0xd3: OP_ADDD(WORD_DIRECT); break;
				/* 0xD4 ANDB direct */
				case 0xd4: OP_AND(reg_b, BYTE_DIRECT); break;
				/* 0xD5 BITB direct */
				case 0xd5: OP_BIT(reg_b, BYTE_DIRECT); break;
				/* 0xD6 LDB direct */
				case 0xd6: OP_LD(reg_b, BYTE_DIRECT); break;
				/* 0xD7 STB direct */
				case 0xd7: OP_ST(reg_b, EA_DIRECT); break;
				/* 0xD8 EORB direct */
				case 0xd8: OP_EOR(reg_b, BYTE_DIRECT); break;
				/* 0xD9 ADCB direct */
				case 0xd9: OP_ADC(reg_b, BYTE_DIRECT); break;
				/* 0xDA ORB direct */
				case 0xda: OP_OR(reg_b, BYTE_DIRECT); break;
				/* 0xDB ADDB direct */
				case 0xdb: OP_ADD(reg_b, BYTE_DIRECT); break;
				/* 0xDC LDD direct */
				case 0xdc: OP_LDD(WORD_DIRECT); break;
				/* 0xDD STD direct */
				case 0xdd: OP_STD(EA_DIRECT); break;
				/* 0xDE LDU direct */
				case 0xde: OP_LD16(reg_u, WORD_DIRECT); break;
				/* 0xDF STU direct */
				case 0xdf: OP_ST16(reg_u, EA_DIRECT); break;
				/* 0xE0 SUBB indexed */
				case 0xe0: OP_SUB(reg_b, BYTE_INDEXED); break;
				/* 0xE1 CMPB indexed */
				case 0xe1: OP_CMP(reg_b, BYTE_INDEXED); break;
				/* 0xE2 SBCB indexed */
				case 0xe2: OP_SBC(reg_b, BYTE_INDEXED); break;
				/* 0xE3 ADDD indexed */
				case 0xe3: OP_ADDD(WORD_INDEXED); break;
				/* 0xE4 ANDB indexed */
				case 0xe4: OP_AND(reg_b, BYTE_INDEXED); break;
				/* 0xE5 BITB indexed */
				case 0xe5: OP_BIT(reg_b, BYTE_INDEXED); break;
				/* 0xE6 LDB indexed */
				case 0xe6: OP_LD(reg_b, BYTE_INDEXED); break;
				/* 0xE7 STB indexed */
				case 0xe7: OP_ST(reg_b, EA_INDEXED); break;
				/* 0xE8 EORB indexed */
				case 0xe8: OP_EOR(reg_b, BYTE_INDEXED); break;
				/* 0xE9 ADCB indexed */
				case 0xe9: OP_ADC(reg_b, BYTE_INDEXED); break;
				/* 0xEA ORB indexed */
				case 0xea: OP_OR(reg_b, BYTE_INDEXED); break;
				/* 0xEB ADDB indexed */
				case 0xeb: OP_ADD(reg_b, BYTE_INDEXED); break;
				/* 0xEC LDD indexed */
				case 0xec: OP_LDD(WORD_INDEXED); break;
				/* 0xED STD indexed */
				case 0xed: OP_STD(EA_INDEXED); break;
				/* 0xEE LDU indexed */
				case 0xee: OP_LD16(reg_u, WORD_INDEXED); break;
				/* 0xEF STU indexed */
				case 0xef: OP_ST16(reg_u, EA_INDEXED); break;
				/* 0xF0 SUBB extended */
				case 0xf0: OP_SUB(reg_b, BYTE_EXTENDED); break;
				/* 0xF1 CMPB extended */
				case 0xf1: OP_CMP(reg_b, BYTE_EXTENDED); break;
				/* 0xF2 SBCB extended */
				case 0xf2: OP_SBC(reg_b, BYTE_EXTENDED); break;
				/* 0xF3 ADDD extended */
				case 0xf3: OP_ADDD(WORD_EXTENDED); break;
				/* 0xF4 ANDB extended */
				case 0xf4: OP_AND(reg_b, BYTE_EXTENDED); break;
				/* 0xF5 BITB extended */
				case 0xf5: OP_BIT(reg_b, BYTE_EXTENDED); break;
				/* 0xF6 LDB extended */
				case 0xf6: OP_LD(reg_b, BYTE_EXTENDED); break;
				/* 0xF7 STB extended */
				case 0xf7: OP_ST(reg_b, EA_EXTENDED); break;
				/* 0xF8 EORB extended */
				case 0xf8: OP_EOR(reg_b, BYTE_EXTENDED); break;
				/* 0xF9 ADCB extended */
				case 0xf9: OP_ADC(reg_b, BYTE_EXTENDED); break;
				/* 0xFA ORB extended */
				case 0xfa: OP_OR(reg_b, BYTE_EXTENDED); break;
				/* 0xFB ADDB extended */
				case 0xfb: OP_ADD(reg_b, BYTE_EXTENDED); break;
				/* 0xFC LDD extended */
				case 0xfc: OP_LDD(WORD_EXTENDED); break;
				/* 0xFD STD extended */
				case 0xfd: OP_STD(EA_EXTENDED); break;
				/* 0xFE LDU extended */
				case 0xfe: OP_LD16(reg_u, WORD_EXTENDED); break;
				/* 0xFF STU extended */
				case 0xff: OP_ST16(reg_u, EA_EXTENDED); break;
				/* Illegal instruction */
				default: TAKEN_CYCLES(1); break;
			}
			IF_TRACE(LOG_DEBUG(0, "\tcc=%02x a=%02x b=%02x dp=%02x x=%04x y=%04x u=%04x s=%04x\n", reg_cc, reg_a, reg_b, reg_dp, reg_x, reg_y, reg_u, reg_s));
			continue;
switched_block_page2:
			BYTE_IMMEDIATE(0,op);
			switch(op) {
				/* 0x10 Page 2 */
				case 0x10: goto switched_block_page2; break;
				/* 0x11 Page 3 */
				case 0x11: goto switched_block_page3; break;
				/* 0x1021 LBRN relative */
				case 0x21: BRANCHL(0); break;
				/* 0x1022 LBHI relative */
				case 0x22: BRANCHL(!(reg_cc & (CC_Z|CC_C))); break;
				/* 0x1023 LBLS relative */
				case 0x23: BRANCHL(reg_cc & (CC_Z|CC_C)); break;
				/* 0x1024 LBHS,LBCC relative */
				case 0x24: BRANCHL(!(reg_cc & CC_C)); break;
				/* 0x1025 LBLO,LBCS relative */
				case 0x25: BRANCHL(reg_cc & CC_C); break;
				/* 0x1026 LBNE relative */
				case 0x26: BRANCHL(!(reg_cc & CC_Z)); break;
				/* 0x1027 LBEQ relative */
				case 0x27: BRANCHL(reg_cc & CC_Z); break;
				/* 0x1028 LBVC relative */
				case 0x28: BRANCHL(!(reg_cc & CC_V)); break;
				/* 0x1029 LBVS relative */
				case 0x29: BRANCHL(reg_cc & CC_V); break;
				/* 0x102A LBPL relative */
				case 0x2a: BRANCHL(!(reg_cc & CC_N)); break;
				/* 0x102B LBMI relative */
				case 0x2b: BRANCHL(reg_cc & CC_N); break;
				/* 0x102C LBGE relative */
				case 0x2c: BRANCHL(!N_EOR_V); break;
				/* 0x102D LBLT relative */
				case 0x2d: BRANCHL(N_EOR_V); break;
				/* 0x102E LBGT relative */
				case 0x2e: BRANCHL(!(N_EOR_V || reg_cc & CC_Z)); break;
				/* 0x102F LBLE relative */
				case 0x2f: BRANCHL(N_EOR_V || (reg_cc & CC_Z)); break;
				/* 0x103F SWI2 inherent */
				case 0x3f:
					reg_cc |= CC_E;
					peek_byte(reg_pc);
					TAKEN_CYCLES(1);
					PUSHWORD(reg_s, reg_pc);
					PUSHWORD(reg_s, reg_u);
					PUSHWORD(reg_s, reg_y);
					PUSHWORD(reg_s, reg_x);
					PUSHBYTE(reg_s, reg_dp);
					PUSHBYTE(reg_s, reg_b);
					PUSHBYTE(reg_s, reg_a);
					PUSHBYTE(reg_s, reg_cc);
					TAKEN_CYCLES(1);
					reg_pc = fetch_word(0xfff4);
					TAKEN_CYCLES(1);
					break;
				/* 0x1083 CMPD immediate */
				case 0x83: OP_CMPD(WORD_IMMEDIATE); break;
				/* 0x108C CMPY immediate */
				case 0x8c: OP_CMP16(reg_y, WORD_IMMEDIATE); break;
				/* 0x108E LDY immediate */
				case 0x8e: OP_LD16(reg_y, WORD_IMMEDIATE); break;
				/* 0x1093 CMPD direct */
				case 0x93: OP_CMPD(WORD_DIRECT); break;
				/* 0x109C CMPY direct */
				case 0x9c: OP_CMP16(reg_y, WORD_DIRECT); break;
				/* 0x109E LDY direct */
				case 0x9e: OP_LD16(reg_y, WORD_DIRECT); break;
				/* 0x109F STY direct */
				case 0x9f: OP_ST16(reg_y, EA_DIRECT); break;
				/* 0x10A3 CMPD indexed */
				case 0xa3: OP_CMPD(WORD_INDEXED); break;
				/* 0x10AC CMPY indexed */
				case 0xac: OP_CMP16(reg_y, WORD_INDEXED); break;
				/* 0x10AE LDY indexed */
				case 0xae: OP_LD16(reg_y, WORD_INDEXED); break;
				/* 0x10AF STY indexed */
				case 0xaf: OP_ST16(reg_y, EA_INDEXED); break;
				/* 0x10B3 CMPD extended */
				case 0xb3: OP_CMPD(WORD_EXTENDED); break;
				/* 0x10BC CMPY extended */
				case 0xbc: OP_CMP16(reg_y, WORD_EXTENDED); break;
				/* 0x10BE LDY extended */
				case 0xbe: OP_LD16(reg_y, WORD_EXTENDED); break;
				/* 0x10BF STY extended */
				case 0xbf: OP_ST16(reg_y, EA_EXTENDED); break;
				/* 0x10CE LDS immediate */
				case 0xce: OP_LD16(reg_s, WORD_IMMEDIATE); ARM_NMI; break;
				/* 0x10DE LDS direct */
				case 0xde: OP_LD16(reg_s, WORD_DIRECT); ARM_NMI; break;
				/* 0x10DF STS direct */
				case 0xdf: OP_ST16(reg_s, EA_DIRECT); break;
				/* 0x10EE LDS indexed */
				case 0xee: OP_LD16(reg_s, WORD_INDEXED); ARM_NMI; break;
				/* 0x10EF STS indexed */
				case 0xef: OP_ST16(reg_s, EA_INDEXED); break;
				/* 0x10FE LDS extended */
				case 0xfe: OP_LD16(reg_s, WORD_EXTENDED); ARM_NMI; break;
				/* 0x10FF STS extended */
				case 0xff: OP_ST16(reg_s, EA_EXTENDED); break;
				/* Illegal instruction */
				default: TAKEN_CYCLES(1); break;
			}
			IF_TRACE(LOG_DEBUG(0, "\tcc=%02x a=%02x b=%02x dp=%02x x=%04x y=%04x u=%04x s=%04x\n", reg_cc, reg_a, reg_b, reg_dp, reg_x, reg_y, reg_u, reg_s));
			continue;
switched_block_page3:
			BYTE_IMMEDIATE(0,op);
			switch(op) {
				/* 0x10 Page 2 */
				case 0x10: goto switched_block_page2; break;
				/* 0x11 Page 3 */
				case 0x11: goto switched_block_page3; break;
				/* 0x113F SWI3 inherent */
				case 0x3f:
					reg_cc |= CC_E;
					peek_byte(reg_pc);
					TAKEN_CYCLES(1);
					PUSHWORD(reg_s, reg_pc);
					PUSHWORD(reg_s, reg_u);
					PUSHWORD(reg_s, reg_y);
					PUSHWORD(reg_s, reg_x);
					PUSHBYTE(reg_s, reg_dp);
					PUSHBYTE(reg_s, reg_b);
					PUSHBYTE(reg_s, reg_a);
					PUSHBYTE(reg_s, reg_cc);
					TAKEN_CYCLES(1);
					reg_pc = fetch_word(0xfff2);
					TAKEN_CYCLES(1);
					break;
				/* 0x1183 CMPU immediate */
				case 0x83: OP_CMP16(reg_u, WORD_IMMEDIATE); break;
				/* 0x118C CMPS immediate */
				case 0x8c: OP_CMP16(reg_s, WORD_IMMEDIATE); break;
				/* 0x1193 CMPU direct */
				case 0x93: OP_CMP16(reg_u, WORD_DIRECT); break;
				/* 0x119C CMPS direct */
				case 0x9c: OP_CMP16(reg_s, WORD_DIRECT); break;
				/* 0x11A3 CMPU indexed */
				case 0xa3: OP_CMP16(reg_u, WORD_INDEXED); break;
				/* 0x11AC CMPS indexed */
				case 0xac: OP_CMP16(reg_s, WORD_INDEXED); break;
				/* 0x11B3 CMPU extended */
				case 0xb3: OP_CMP16(reg_u, WORD_EXTENDED); break;
				/* 0x11BC CMPS extended */
				case 0xbc: OP_CMP16(reg_s, WORD_EXTENDED); break;
				/* Illegal instruction */
				default: TAKEN_CYCLES(1); break;
			}
			IF_TRACE(LOG_DEBUG(0, "\tcc=%02x a=%02x b=%02x dp=%02x x=%04x y=%04x u=%04x s=%04x\n", reg_cc, reg_a, reg_b, reg_dp, reg_x, reg_y, reg_u, reg_s));
			continue;
		}
	}

restore_regs:
	register_cc = reg_cc;
	register_a = reg_a;
	register_b = reg_b;
	register_dp = reg_dp;
	register_x = reg_x;
	register_y = reg_y;
	register_u = reg_u;
	register_s = reg_s;
	register_pc = reg_pc;
}

void m6809_get_state(M6809State *state) {
	state->reg_cc = register_cc;
	state->reg_a = register_a;
	state->reg_b = register_b;
	state->reg_dp = register_dp;
	state->reg_x = register_x;
	state->reg_y = register_y;
	state->reg_u = register_u;
	state->reg_s = register_s;
	state->reg_pc = register_pc;
	state->halt = halt;
	state->nmi = nmi;
	state->firq = firq;
	state->irq = irq;
	state->wait_for_interrupt = wait_for_interrupt;
	state->skip_register_push = skip_register_push;
	state->nmi_armed = nmi_armed;
}

void m6809_set_state(M6809State *state) {
	register_cc = state->reg_cc;
	register_a = state->reg_a;
	register_b = state->reg_b;
	register_dp = state->reg_dp;
	register_x = state->reg_x;
	register_y = state->reg_y;
	register_u = state->reg_u;
	register_s = state->reg_s;
	register_pc = state->reg_pc;
	halt = state->halt;
	nmi = state->nmi;
	firq = state->firq;
	irq = state->irq;
	wait_for_interrupt = state->wait_for_interrupt;
	skip_register_push = state->skip_register_push;
	nmi_armed = state->nmi_armed;
}

/* Kept for old snapshots */
void m6809_set_registers(uint8_t *regs) {
	register_cc = regs[0];
	register_a = regs[1];
	register_b = regs[2];
	register_dp = regs[3];
	register_x = regs[4] << 8 | regs[5];
	register_y = regs[6] << 8 | regs[7];
	register_u = regs[8] << 8 | regs[9];
	register_s = regs[10] << 8 | regs[11];
	register_pc = regs[12] << 8 | regs[13];
}

void m6809_jump(uint_least16_t pc) {
	register_pc = pc & 0xffff;
}
