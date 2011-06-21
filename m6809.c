/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2011  Ciaran Anscomb
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
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

#include <stdlib.h>

#include "types.h"
#include "m6809.h"

/* Condition Code manipulation macros */

#define CC_E 0x80
#define CC_F 0x40
#define CC_H 0x20
#define CC_I 0x10
#define CC_N 0x08
#define CC_Z 0x04
#define CC_V 0x02
#define CC_C 0x01
#define N_EOR_V ((reg_cc & CC_N)^((reg_cc & CC_V)<<2))

#define CLR_HNZVC       do { reg_cc &= ~(CC_H|CC_N|CC_Z|CC_V|CC_C); } while (0)
#define CLR_NZV         do { reg_cc &= ~(CC_N|CC_Z|CC_V); } while (0)
#define CLR_NZVC        do { reg_cc &= ~(CC_N|CC_Z|CC_V|CC_C); } while (0)
#define CLR_Z           do { reg_cc &= ~(CC_Z); } while (0)
#define CLR_NZC         do { reg_cc &= ~(CC_N|CC_Z|CC_C); } while (0)
#define CLR_NVC         do { reg_cc &= ~(CC_N|CC_V|CC_C); } while (0)
#define CLR_ZC          do { reg_cc &= ~(CC_Z|CC_C); } while (0)

#define SET_Z(a)        do { if (!(a)) reg_cc |= CC_Z; } while (0)
#define SET_N8(a)       do { reg_cc |= (a&0x80)>>4; } while (0)
#define SET_N16(a)      do { reg_cc |= (a&0x8000)>>12; } while (0)
#define SET_H(a,b,r)    do { reg_cc |= ((a^b^r)&0x10)<<1; } while (0)
#define SET_C8(a)       do { reg_cc |= (a&0x100)>>8; } while (0)
#define SET_C16(a)      do { reg_cc |= (a&0x10000)>>16; } while (0)
#define SET_V8(a,b,r)   do { reg_cc |= ((a^b^r^(r>>1))&0x80)>>6; } while (0)
#define SET_V16(a,b,r)  do { reg_cc |= ((a^b^r^(r>>1))&0x8000)>>14; } while (0)
#define SET_NZ8(a)              do { SET_N8(a); SET_Z(a&0xff); } while (0)
#define SET_NZ16(a)             do { SET_N16(a);SET_Z(a&0xffff); } while (0)
#define SET_NZC8(a)             do { SET_N8(a); SET_Z(a&0xff);SET_C8(a); } while (0)
#define SET_NZVC8(a,b,r)        do { SET_N8(r); SET_Z(r&0xff);SET_V8(a,b,r);SET_C8(r); } while (0)
#define SET_NZVC16(a,b,r)       do { SET_N16(r);SET_Z(r&0xffff);SET_V16(a,b,r);SET_C16(r); } while (0)

#define SET_NZ_D() do { SET_N8(reg_a); if (!reg_a && !reg_b) reg_cc |= CC_Z; } while (0)

/* CPU fetch/store goes via SAM */
#define fetch_byte(a) (cycle++, m6809_read_cycle(a))
#define fetch_word(a) (fetch_byte(a) << 8 | fetch_byte((a)+1))
#define store_byte(a,v) do { m6809_write_cycle(a,v); cycle++; } while (0)
/* This one only used to try and get correct timing: */
#define peek_byte(a) do { (void)m6809_read_cycle(a); } while (0)

#define EA_DIRECT(a)    do { a = reg_dp << 8 | fetch_byte(reg_pc); reg_pc += 1; TAKEN_CYCLES(1); } while (0)
#define EA_EXTENDED(a)  do { a = fetch_byte(reg_pc) << 8 | fetch_byte(reg_pc+1); reg_pc += 2; TAKEN_CYCLES(1); } while (0)

/* These macros are designed to be "passed as an argument" to the op-code
 * macros.  */
#define BYTE_IMMEDIATE(a,v)     { (void)a; v = fetch_byte(reg_pc); reg_pc++; }
#define BYTE_DIRECT(a,v)        { EA_DIRECT(a); v = fetch_byte(a); }
#define BYTE_INDEXED(a,v)       { EA_INDEXED(a); v = fetch_byte(a); }
#define BYTE_EXTENDED(a,v)      { EA_EXTENDED(a); v = fetch_byte(a); }
#define WORD_IMMEDIATE(a,v)     { (void)a; v = fetch_byte(reg_pc) << 8 | fetch_byte(reg_pc+1); reg_pc += 2; }
#define WORD_DIRECT(a,v)        { EA_DIRECT(a); v = fetch_byte(a) << 8 | fetch_byte(a+1); }
#define WORD_INDEXED(a,v)       { EA_INDEXED(a); v = fetch_byte(a) << 8 | fetch_byte(a+1); }
#define WORD_EXTENDED(a,v)      { EA_EXTENDED(a); v = fetch_byte(a) << 8 | fetch_byte(a+1); }

#define SHORT_RELATIVE(r)       { BYTE_IMMEDIATE(0,r); r = sex(r); }
#define LONG_RELATIVE(r)        WORD_IMMEDIATE(0,r)

#define TAKEN_CYCLES(c) do { m6809_nvma_cycles(c); cycle += c; } while (0)

#define PUSHWORD(s,r)   { s -= 2; store_byte(s+1, r); store_byte(s, r >> 8); }
#define PUSHBYTE(s,r)   { s--; store_byte(s, r); }
#define PULLBYTE(s,r)   { r = fetch_byte(s); s++; }
#define PULLWORD(s,r)   { r = fetch_byte(s) << 8; r |= fetch_byte(s+1); s += 2; }

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

#define PUSH_IRQ_REGISTERS_NO_E() do { \
		TAKEN_CYCLES(1); \
		PUSHWORD(reg_s, reg_pc); \
		PUSHWORD(reg_s, reg_u); \
		PUSHWORD(reg_s, reg_y); \
		PUSHWORD(reg_s, reg_x); \
		PUSHBYTE(reg_s, reg_dp); \
		PUSHBYTE(reg_s, reg_b); \
		PUSHBYTE(reg_s, reg_a); \
		PUSHBYTE(reg_s, reg_cc); \
	} while (0)

#define PUSH_IRQ_REGISTERS() do { \
		reg_cc |= CC_E; \
		PUSH_IRQ_REGISTERS_NO_E(); \
	} while (0)

#define PUSH_FIRQ_REGISTERS() do { \
		reg_cc &= ~CC_E; \
		TAKEN_CYCLES(1); \
		PUSHWORD(reg_s, reg_pc); \
		PUSHBYTE(reg_s, reg_cc); \
	} while (0)

#define TAKE_INTERRUPT(i,cm,v) do { \
		reg_cc |= (cm); \
		TAKEN_CYCLES(1); \
		if (m6809_interrupt_hook) { \
			m6809_interrupt_hook(v); \
		} \
		reg_pc = fetch_word(v); \
		TAKEN_CYCLES(1); \
	} while (0)

#define ARM_NMI do { nmi_armed = 1; } while (0)
#define DISARM_NMI do { nmi_armed = 0; } while (0)

int m6809_running;

/* MPU registers */
static unsigned int reg_cc;
static uint8_t reg_a;
static uint8_t reg_b;
static unsigned int reg_dp;
static uint16_t reg_x;
static uint16_t reg_y;
static uint16_t reg_u;
static uint16_t reg_s;
static uint16_t reg_pc;

/* MPU interrupt state variables */
static unsigned int halt, nmi, firq, irq;
static unsigned int halt_cycle, nmi_cycle, firq_cycle, irq_cycle;
static int nmi_armed;
static unsigned int cycle;

/* MPU state.  Represents current position in the high-level flow chart
 * from the data sheet (figure 14). */
static enum m6809_cpu_state cpu_state;

#define reg_d ((reg_a << 8) | reg_b)
#define set_reg_d(v) do { reg_a = (v)>>8; reg_b = (v); } while (0)

#define sex5(v) ((int)(((v) & 0x0f) - ((v) & 0x10)))
#define sex(v) ((int)(((v) & 0x7f) - ((v) & 0x80)))

/* External handlers */
unsigned int (*m6809_read_cycle)(unsigned int addr);
void (*m6809_write_cycle)(unsigned int addr, unsigned int value);
void (*m6809_nvma_cycles)(int cycles);
void (*m6809_sync)(void);
void (*m6809_instruction_hook)(M6809State *state);
void (*m6809_instruction_posthook)(M6809State *state);
void (*m6809_interrupt_hook)(unsigned int vector);

/* ------------------------------------------------------------------------- */

#define EA_ROFF0(b,r) case (b): ea = (r); peek_byte(reg_pc)
#define EA_ROFF5P(b,r,o) case ((b)+(o)): ea = (r) + (o); peek_byte(reg_pc); TAKEN_CYCLES(1)
#define EA_ROFF5N(b,r,o) case ((b)+32+(o)): ea = (r) + (o); peek_byte(reg_pc); TAKEN_CYCLES(1)
#define EA_ROFF8(b,r) case (b): BYTE_IMMEDIATE(0,ea); ea = sex(ea) + (r); TAKEN_CYCLES(1)
#define EA_ROFF16(b,r) case (b): WORD_IMMEDIATE(0,ea); ea += (r); TAKEN_CYCLES(3)
#define EA_ROFFA(b,r) case (b): ea = (r) + sex(reg_a); peek_byte(reg_pc); TAKEN_CYCLES(1)
#define EA_ROFFB(b,r) case (b): ea = (r) + sex(reg_b); peek_byte(reg_pc); TAKEN_CYCLES(1)
#define EA_ROFFD(b,r) case (b): ea = (r) + reg_d; peek_byte(reg_pc); peek_byte(reg_pc+1); TAKEN_CYCLES(3)
#define EA_RI1(b,r) case (b): ea = (r); r += 1; peek_byte(reg_pc); TAKEN_CYCLES(2)
#define EA_RI2(b,r) case (b): ea = (r); r += 2; peek_byte(reg_pc); TAKEN_CYCLES(3)
#define EA_RD1(b,r) case (b): r -= 1; ea = (r); peek_byte(reg_pc); TAKEN_CYCLES(2)
#define EA_RD2(b,r) case (b): r -= 2; ea = (r); peek_byte(reg_pc); TAKEN_CYCLES(3)
#define EA_PCOFFFF(b,r) case (b): ea = reg_pc | 0xff
#define EA_PCOFF8(b,r) case (b): BYTE_IMMEDIATE(0,ea); ea = sex(ea) + reg_pc; TAKEN_CYCLES(1)
#define EA_PCOFF16(b,r) case (b): WORD_IMMEDIATE(0,ea); ea += reg_pc; peek_byte(reg_pc); TAKEN_CYCLES(3)
/* Without indirection this is an illegal instruction, but it seems to work
 * on a real 6809: */
#define EA_EXT(b,r) case (b): WORD_IMMEDIATE(0,ea); TAKEN_CYCLES(1)

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

/* Convention for the following macros:
 *   fb  - passed-in fetch byte macro
 *   fw  - passed-in fetch word macro
 *   ea  - passed-in effective address macro
 *   r   - register
 *   a   - computed address
 *   d   - fetched data
 *   tmp - temporary result
 */

#define OP_NEG(fb) { unsigned int a, d, tmp; fb(a,d); tmp = ~(d) + 1; CLR_NZVC; SET_NZVC8(0, d, tmp); TAKEN_CYCLES(1); store_byte(a,tmp); }
#define OP_COM(fb) { unsigned int a, d; fb(a,d); d = ~(d); CLR_NZV; SET_NZ8(d); reg_cc |= CC_C; TAKEN_CYCLES(1); store_byte(a,d); }
#define OP_LSR(fb) { unsigned int a, d; fb(a,d); CLR_NZC; reg_cc |= (d & CC_C); d &= 0xff; d >>= 1; SET_Z(d); TAKEN_CYCLES(1); store_byte(a,d); }
#define OP_ROR(fb) { unsigned int a, d, tmp; fb(a,d); tmp = (reg_cc & CC_C) << 7; CLR_NZC; reg_cc |= d & CC_C; tmp |= (d & 0xff) >> 1; SET_NZ8(tmp); TAKEN_CYCLES(1); store_byte(a,tmp); }
#define OP_ASR(fb) { unsigned int a, d; CLR_NZC; fb(a,d); reg_cc |= (d & CC_C); d = (d & 0x80) | ((d & 0xff) >> 1); SET_NZ8(d); TAKEN_CYCLES(1); store_byte(a,d); }
#define OP_ASL(fb) { unsigned int a, d, tmp; fb(a,d); tmp = d << 1; CLR_NZVC; SET_NZVC8(d, d, tmp); TAKEN_CYCLES(1); store_byte(a,tmp); }
#define OP_ROL(fb) { unsigned int a, d, tmp; fb(a,d); tmp = (d<<1)|(reg_cc & 1); CLR_NZVC; SET_NZVC8(d, d, tmp); TAKEN_CYCLES(1); store_byte(a,tmp); }
#define OP_DEC(fb) { unsigned int a, d; fb(a,d); d--; d &= 0xff; CLR_NZV; SET_NZ8(d); if (d == 0x7f) reg_cc |= CC_V; TAKEN_CYCLES(1); store_byte(a,d); }
#define OP_INC(fb) { unsigned int a, d; fb(a,d); d++; d &= 0xff; CLR_NZV; SET_NZ8(d); if (d == 0x80) reg_cc |= CC_V; TAKEN_CYCLES(1); store_byte(a,d); }
#define OP_TST(fb) { unsigned int a, d; fb(a,d); CLR_NZV; SET_NZ8(d); TAKEN_CYCLES(2); }
#define OP_JMP(ea) { unsigned int a; ea(a); reg_pc = a; }
#define OP_CLR(fb) { unsigned int a, d; fb(a,d); TAKEN_CYCLES(1); store_byte(a,0); CLR_NVC; reg_cc |= CC_Z; }

#define OP_NEGR(r) { unsigned int tmp = ~(r) + 1; CLR_NZVC; SET_NZVC8(0, r, tmp); r = tmp; peek_byte(reg_pc); }
#define OP_COMR(r) { r = ~(r); CLR_NZV; SET_NZ8(r); reg_cc |= CC_C; peek_byte(reg_pc); }
#define OP_LSRR(r) { CLR_NZC; reg_cc |= (r & CC_C); r >>= 1; SET_Z(r); peek_byte(reg_pc); }
#define OP_RORR(r) { unsigned int tmp = (reg_cc & CC_C) << 7; CLR_NZC; reg_cc |= r & CC_C; tmp |= r >> 1; SET_NZ8(tmp); r = tmp; peek_byte(reg_pc); }
#define OP_ASRR(r) { CLR_NZC; reg_cc |= (r & CC_C); r = (r & 0x80) | (r >> 1); SET_NZ8(r); peek_byte(reg_pc); }
#define OP_ASLR(r) { unsigned int tmp = r << 1; CLR_NZVC; SET_NZVC8(r, r, tmp); r = tmp; peek_byte(reg_pc); }
#define OP_ROLR(r) { unsigned int tmp = (reg_cc & CC_C) | (r << 1); CLR_NZVC; SET_NZVC8(r, r, tmp); r = tmp; peek_byte(reg_pc); }
/* Note: this used to be "r--; r &= 0xff;", but gcc optimises too much away */
#define OP_DECR(r) { r = r - 1; CLR_NZV; SET_NZ8(r); if (r == 0x7f) reg_cc |= CC_V; peek_byte(reg_pc); }
#define OP_INCR(r) { r = r + 1; CLR_NZV; SET_NZ8(r); if (r == 0x80) reg_cc |= CC_V; peek_byte(reg_pc); }
#define OP_TSTR(r) { CLR_NZV; SET_NZ8(r); peek_byte(reg_pc); }
#define OP_CLRR(r) { r = 0; CLR_NVC; reg_cc |= CC_Z; peek_byte(reg_pc); }

#define OP_SUBD(fw) { unsigned int a, d, tmp, dreg = reg_d; fw(a,d); tmp = dreg - d; CLR_NZVC; SET_NZVC16(dreg, d, tmp); set_reg_d(tmp); TAKEN_CYCLES(1); }
#define OP_ADDD(fw) { unsigned int a, d, tmp, dreg = reg_d; fw(a,d); tmp = dreg + d; CLR_NZVC; SET_NZVC16(dreg, d, tmp); set_reg_d(tmp); TAKEN_CYCLES(1); }
#define OP_CMPD(fw) { unsigned int a, d, tmp, dreg = reg_d; fw(a,d); tmp = dreg - d; CLR_NZVC; SET_NZVC16(dreg, d, tmp); TAKEN_CYCLES(1); }
#define OP_LDD(ea) { unsigned int a; ea(a); reg_a = fetch_byte(a); reg_b = fetch_byte(a+1); CLR_NZV; SET_NZ_D(); }
#define OP_LDD_IMM() { BYTE_IMMEDIATE(0,reg_a); BYTE_IMMEDIATE(0,reg_b); CLR_NZV; SET_NZ_D(); }
#define OP_STD(ea) { unsigned int a; ea(a); store_byte(a, reg_a); store_byte(a+1, reg_b); CLR_NZV; SET_NZ_D(); }

/* #define OP_SUB16(r,fw) { unsigned int a, d, tmp; fw(a,d); tmp = r - d; CLR_NZVC; SET_NZVC16(r, d, tmp); r = tmp; TAKEN_CYCLES(1); } */
/* #define OP_ADD16(r,fw) { unsigned int a, d, tmp; fw(a,d); tmp = r + d; CLR_NZVC; SET_NZVC16(r, d, tmp); r = tmp; TAKEN_CYCLES(1); } */
#define OP_CMP16(r,fw) { unsigned int a, d, tmp; fw(a,d); tmp = r - d; CLR_NZVC; SET_NZVC16(r, d, tmp); TAKEN_CYCLES(1); }
#define OP_LD16(r,fw) { unsigned int a; fw(a,r); CLR_NZV; SET_NZ16(r); }
#define OP_ST16(r,ea) { unsigned int a; ea(a); store_byte(a, r >> 8); store_byte(a+1, r); CLR_NZV; SET_NZ16(r); }
/* Illegal instruction, only part-working: */
#define OP_ST16_IMM(r) { (void)fetch_byte(reg_pc); reg_pc++; store_byte(reg_pc, r); reg_pc++; CLR_NZV; reg_cc |= CC_N; }
#define OP_JSR(ea) { unsigned int a; ea(a); peek_byte(a); TAKEN_CYCLES(1); PUSHWORD(reg_s, reg_pc); reg_pc = a; }

#define OP_SUB(r,fb) { unsigned int a, d, tmp; fb(a,d); tmp = r - d; CLR_NZVC; SET_NZVC8(r, d, tmp); r = tmp; }
#define OP_CMP(r,fb) { unsigned int a, d, tmp; fb(a,d); tmp = r - d; CLR_NZVC; SET_NZVC8(r, d, tmp); }
#define OP_SBC(r,fb) { unsigned int a, d, tmp; fb(a,d); tmp = r - d - (reg_cc & CC_C); CLR_NZVC; SET_NZVC8(r, d, tmp); r = tmp; }
#define OP_AND(r,fb) { unsigned int a, d; fb(a,d); r &= d; CLR_NZV; SET_NZ8(r); }
#define OP_BIT(r,fb) { unsigned int a, d, tmp; fb(a,d); tmp = r & d; CLR_NZV; SET_NZ8(tmp); }
#define OP_LD(r,fb) { unsigned int a; fb(a,r); CLR_NZV; SET_NZ8(r); }
#define OP_DISCARD(fb) { unsigned int a, tmp; fb(a,tmp); CLR_NZV; reg_cc |= CC_N; }
#define OP_ST(r,fb) { unsigned int a; fb(a); store_byte(a, r); CLR_NZV; SET_NZ8(r); }
#define OP_EOR(r,fb) { unsigned int a, d; fb(a,d); r ^= d; CLR_NZV; SET_NZ8(r); }
#define OP_ADC(r,fb) { unsigned int a, d, tmp; fb(a,d); tmp = r + d + (reg_cc & CC_C); CLR_HNZVC; SET_NZVC8(r, d, tmp); SET_H(r, d, tmp); r = tmp; }
#define OP_OR(r,fb) { unsigned int a, d; fb(a,d); r |= d; CLR_NZV; SET_NZ8(r); }
#define OP_ADD(r,fb) { unsigned int a, d, tmp; fb(a,d); tmp = r + d; CLR_HNZVC; SET_NZVC8(r, d, tmp); SET_H(r, d, tmp); r = tmp; }

#define BRANCHS(cond) do { \
		unsigned int offset; \
		SHORT_RELATIVE(offset); \
		TAKEN_CYCLES(1); \
		if (cond) { reg_pc += offset; } \
	} while (0)
#define BRANCHL(cond) do { \
		unsigned int offset; \
		LONG_RELATIVE(offset); \
		if (cond) { reg_pc += offset; TAKEN_CYCLES(2); } \
		else { TAKEN_CYCLES(1); } \
	} while (0)

/* ------------------------------------------------------------------------- */

static unsigned int ea_indexed(void) {
	unsigned int ea;
	unsigned int postbyte;
	BYTE_IMMEDIATE(0,postbyte);
	switch (postbyte) {
		EA_ALLR(EA_ROFF5,   0x00);
		EA_ALLR(EA_RI1,     0x80); EA_ALLRI(EA_RI1,     0x90);
		EA_ALLR(EA_RI2,     0x81); EA_ALLRI(EA_RI2,     0x91);
		EA_ALLR(EA_RD1,     0x82); EA_ALLRI(EA_RD1,     0x92);
		EA_ALLR(EA_RD2,     0x83); EA_ALLRI(EA_RD2,     0x93);
		EA_ALLR(EA_ROFF0,   0x84); EA_ALLRI(EA_ROFF0,   0x94);
		EA_ALLR(EA_ROFFB,   0x85); EA_ALLRI(EA_ROFFB,   0x95);
		EA_ALLR(EA_ROFFA,   0x86); EA_ALLRI(EA_ROFFA,   0x96);
		EA_ALLR(EA_ROFF0,   0x87); EA_ALLRI(EA_ROFF0,   0x97);
		EA_ALLR(EA_ROFF8,   0x88); EA_ALLRI(EA_ROFF8,   0x98);
		EA_ALLR(EA_ROFF16,  0x89); EA_ALLRI(EA_ROFF16,  0x99);
		EA_ALLR(EA_PCOFFFF, 0x8a); EA_ALLRI(EA_PCOFFFF, 0x9a);
		EA_ALLR(EA_ROFFD,   0x8b); EA_ALLRI(EA_ROFFD,   0x9b);
		EA_ALLR(EA_PCOFF8,  0x8c); EA_ALLRI(EA_PCOFF8,  0x9c);
		EA_ALLR(EA_PCOFF16, 0x8d); EA_ALLRI(EA_PCOFF16, 0x9d);
		EA_ALLR(EA_EXT,     0x8f); EA_ALLRI(EA_EXT,     0x9f);
		default: ea = 0; break;
	}
	return ea;
}

void m6809_init(void) {
	m6809_read_cycle = NULL;
	m6809_write_cycle = NULL;
	m6809_nvma_cycles = NULL;
	m6809_sync = NULL;
	m6809_instruction_hook = NULL;
	m6809_instruction_posthook = NULL;
	m6809_interrupt_hook = NULL;
	reg_cc = reg_a = reg_b = reg_dp = 0;
	reg_x = reg_y = reg_u = reg_s = 0;
	reg_pc = 0;
	cycle = 0;
	halt_cycle = nmi_cycle = firq_cycle = irq_cycle = 0;
	m6809_running = 0;
}

void m6809_reset(void) {
	halt = nmi = firq = irq = 0;
	DISARM_NMI;
	cpu_state = m6809_flow_reset;
}

/* Run CPU while m6809_running is true. */

void m6809_run(void) {

	while (m6809_running) {

		m6809_sync();

		switch (cpu_state) {

		case m6809_flow_reset:
			reg_dp = 0;
			reg_cc |= (CC_F | CC_I);
			nmi = 0;
			DISARM_NMI;
			cpu_state = m6809_flow_reset_check_halt;
			/* fall through */

		case m6809_flow_reset_check_halt:
			if (halt) {
				TAKEN_CYCLES(1);
				continue;
			}
			reg_pc = fetch_word(0xfffe);
			TAKEN_CYCLES(1);
			cpu_state = m6809_flow_label_a;
			continue;

		/* done_instruction case for backwards-compatibility */
		case m6809_flow_done_instruction:
		case m6809_flow_label_a:
			if (halt) {
				TAKEN_CYCLES(1);
				continue;
			}
			cpu_state = m6809_flow_label_b;
			/* fall through */

		case m6809_flow_label_b:
			if (nmi_armed && nmi  && (int)(cycle - nmi_cycle) > 0) {
				peek_byte(reg_pc);
				peek_byte(reg_pc);
				PUSH_IRQ_REGISTERS();
				cpu_state = m6809_flow_dispatch_irq;
				continue;
			}
			if (firq && !(reg_cc & CC_F) && (int)(cycle - firq_cycle) > 0) {
				peek_byte(reg_pc);
				peek_byte(reg_pc);
				PUSH_FIRQ_REGISTERS();
				cpu_state = m6809_flow_dispatch_irq;
				continue;
			}
			if (irq && !(reg_cc & CC_I) && (int)(cycle - irq_cycle) > 0) {
				peek_byte(reg_pc);
				peek_byte(reg_pc);
				PUSH_IRQ_REGISTERS();
				cpu_state = m6809_flow_dispatch_irq;
				continue;
			}
			cpu_state = m6809_flow_next_instruction;
			continue;

		case m6809_flow_dispatch_irq:
			if (nmi_armed && nmi) {
				nmi = 0;
				reg_cc |= (CC_F | CC_I);
				TAKE_INTERRUPT(nmi, CC_F|CC_I, 0xfffc);
				cpu_state = m6809_flow_label_a;
				continue;
			}
			if (firq && !(reg_cc & CC_F)) {
				reg_cc |= (CC_F | CC_I);
				TAKE_INTERRUPT(firq, CC_F|CC_I, 0xfff6);
				cpu_state = m6809_flow_label_a;
				continue;
			}
			if (irq && !(reg_cc & CC_I)) {
				reg_cc |= CC_I;
				TAKE_INTERRUPT(irq, CC_I, 0xfff8);
				cpu_state = m6809_flow_label_a;
				continue;
			}
			cpu_state = m6809_flow_cwai_check_halt;
			continue;

		case m6809_flow_cwai_check_halt:
			TAKEN_CYCLES(1);
			if (halt) {
				continue;
			}
			cpu_state = m6809_flow_dispatch_irq;
			continue;

		case m6809_flow_sync:
			if ((nmi  && (int)(cycle - nmi_cycle) > 0) ||
			    (firq && (int)(cycle - firq_cycle) > 0) ||
			    (irq  && (int)(cycle - irq_cycle) > 0)) {
				TAKEN_CYCLES(2);
				cpu_state = m6809_flow_label_b;
				continue;
			}
			TAKEN_CYCLES(1);
			if (halt)
				cpu_state = m6809_flow_sync_check_halt;
			continue;

		case m6809_flow_sync_check_halt:
			TAKEN_CYCLES(1);
			if (!halt) {
				cpu_state = m6809_flow_sync;
			}
			continue;

		case m6809_flow_next_instruction:
			{
			unsigned int op;
			/* Instruction fetch hook */
			if (m6809_instruction_hook) {
				M6809State state;
				state.reg_cc = reg_cc;
				state.reg_a  = reg_a;
				state.reg_b  = reg_b;
				state.reg_dp = reg_dp;
				state.reg_x  = reg_x;
				state.reg_y  = reg_y;
				state.reg_u  = reg_u;
				state.reg_s  = reg_s;
				state.reg_pc = reg_pc;
				m6809_instruction_hook(&state);
				reg_cc = state.reg_cc;
				reg_a = state.reg_a;
				reg_b = state.reg_b;
				reg_dp = state.reg_dp;
				reg_x = state.reg_x;
				reg_y = state.reg_y;
				reg_u = state.reg_u;
				reg_s = state.reg_s;
				reg_pc = state.reg_pc;
			}
			/* Fetch op-code and process */
			BYTE_IMMEDIATE(0,op);
			switch (op) {
			/* 0x00 NEG direct */
			case 0x00:
			case 0x01: /* (illegal) */
				OP_NEG(BYTE_DIRECT); break;
			/* 0x02 NEG/COM direct (illegal) */
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
			case 0x05: /* (illegal) */
				OP_LSR(BYTE_DIRECT); break;
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
			case 0x0B: /* (illegal) */
				OP_DEC(BYTE_DIRECT); break;
			/* 0x0C INC direct */
			case 0x0C: OP_INC(BYTE_DIRECT); break;
			/* 0x0D TST direct */
			case 0x0D: OP_TST(BYTE_DIRECT); break;
			/* 0x0E JMP direct */
			case 0x0E: OP_JMP(EA_DIRECT); break;
			/* 0x0F CLR direct */
			case 0x0F: OP_CLR(BYTE_DIRECT); break;
			/* 0x10 Page 2 */
			case 0x10:
				   cpu_state = m6809_flow_instruction_page_2;
				   continue;
			/* 0x11 Page 3 */
			case 0x11:
				   cpu_state = m6809_flow_instruction_page_3;
				   continue;
			/* 0x12 NOP inherent */
			case 0x12: peek_byte(reg_pc); break;
			/* 0x13 SYNC inherent */
			case 0x13:
				peek_byte(reg_pc);
				cpu_state = m6809_flow_sync;
				goto done_instruction;
			/* 0x14, 0x15 HCF? (illegal) */
			case 0x14:
			case 0x15:
				cpu_state = m6809_flow_hcf;
				goto done_instruction;
			/* 0x16 LBRA relative */
			case 0x16: BRANCHL(1); break;
			/* 0x17 LBSR relative */
			case 0x17: {
				unsigned int new_pc;
				LONG_RELATIVE(new_pc);
				new_pc = reg_pc + new_pc;
				TAKEN_CYCLES(4);
				PUSHWORD(reg_s, reg_pc);
				reg_pc = new_pc;
			} break;
			/* 0x18 Shift CC with mask inherent (illegal) */
			case 0x18:
				reg_cc = (reg_cc << 1) & (CC_H | CC_Z);
				TAKEN_CYCLES(1);
				peek_byte(reg_pc);
				break;
			/* 0x19 DAA inherent */
			case 0x19: {
				unsigned int tmp = 0;
				if ((reg_a&0x0f) >= 0x0a || reg_cc & CC_H) tmp |= 0x06;
				if (reg_a >= 0x90 && (reg_a&0x0f) >= 0x0a) tmp |= 0x60;
				if (reg_a >= 0xa0 || reg_cc & CC_C) tmp |= 0x60;
				tmp += reg_a;
				reg_a = tmp;
				CLR_NZVC;
				SET_NZC8(tmp);
				peek_byte(reg_pc);
			} break;
			/* 0x1A ORCC immediate */
			case 0x1a: {
				unsigned int data;
				BYTE_IMMEDIATE(0,data);
				reg_cc |= data;
				peek_byte(reg_pc);
			} break;
			/* 0x1B NOP inherent (illegal) */
			case 0x1b: peek_byte(reg_pc); break;
			/* 0x1C ANDCC immediate */
			case 0x1c: {
				unsigned int data;
				BYTE_IMMEDIATE(0,data);
				reg_cc &= data;
				peek_byte(reg_pc);
			} break;
			/* 0x1D SEX inherent */
			case 0x1d:
				reg_a = (reg_b & 0x80) ? 0xff : 0;
				CLR_NZV;
				SET_NZ_D();
				peek_byte(reg_pc);
				break;
			/* 0x1E EXG immediate */
			case 0x1e: {
				unsigned int postbyte;
				unsigned int tmp1, tmp2;
				BYTE_IMMEDIATE(0,postbyte);
				switch (postbyte >> 4) {
					case 0x0: tmp1 = reg_d; break;
					case 0x1: tmp1 = reg_x; break;
					case 0x2: tmp1 = reg_y; break;
					case 0x3: tmp1 = reg_u; break;
					case 0x4: tmp1 = reg_s; break;
					case 0x5: tmp1 = reg_pc; break;
					case 0x8: tmp1 = reg_a | 0xff00; break;
					case 0x9: tmp1 = reg_b | 0xff00; break;
					case 0xa: tmp1 = reg_cc | 0xff00; break;
					case 0xb: tmp1 = reg_dp | 0xff00; break;
					default:  tmp1 = 0xffff; break;
				}
				switch (postbyte & 0xf) {
					case 0x0: tmp2 = reg_d; set_reg_d(tmp1); break;
					case 0x1: tmp2 = reg_x; reg_x = tmp1; break;
					case 0x2: tmp2 = reg_y; reg_y = tmp1; break;
					case 0x3: tmp2 = reg_u; reg_u = tmp1; break;
					case 0x4: tmp2 = reg_s; reg_s = tmp1; break;
					case 0x5: tmp2 = reg_pc; reg_pc = tmp1; break;
					case 0x8: tmp2 = reg_a | 0xff00; reg_a = tmp1; break;
					case 0x9: tmp2 = reg_b | 0xff00; reg_b = tmp1; break;
					case 0xa: tmp2 = reg_cc | 0xff00; reg_cc = tmp1 & 0xff; break;
					case 0xb: tmp2 = reg_dp | 0xff00; reg_dp = tmp1 & 0xff; break;
					default:  tmp2 = 0xffff; break;
				}
				switch (postbyte >> 4) {
					case 0x0: set_reg_d(tmp2); break;
					case 0x1: reg_x = tmp2; break;
					case 0x2: reg_y = tmp2; break;
					case 0x3: reg_u = tmp2; break;
					case 0x4: reg_s = tmp2; break;
					case 0x5: reg_pc = tmp2; break;
					case 0x8: reg_a = tmp2; break;
					case 0x9: reg_b = tmp2; break;
					case 0xa: reg_cc = tmp2 & 0xff; break;
					case 0xb: reg_dp = tmp2 & 0xff; break;
				}
				TAKEN_CYCLES(6);
			} break;
			/* 0x1F TFR immediate */
			case 0x1f: {
				unsigned int postbyte;
				unsigned int tmp1;
				BYTE_IMMEDIATE(0,postbyte);
				switch (postbyte >> 4) {
					case 0x0: tmp1 = reg_d; break;
					case 0x1: tmp1 = reg_x; break;
					case 0x2: tmp1 = reg_y; break;
					case 0x3: tmp1 = reg_u; break;
					case 0x4: tmp1 = reg_s; break;
					case 0x5: tmp1 = reg_pc; break;
					case 0x8: tmp1 = reg_a | 0xff00; break;
					case 0x9: tmp1 = reg_b | 0xff00; break;
					case 0xa: tmp1 = reg_cc | 0xff00; break;
					case 0xb: tmp1 = reg_dp | 0xff00; break;
					default:  tmp1 = 0xffff; break;
				}
				switch (postbyte & 0xf) {
					case 0x0: set_reg_d(tmp1); break;
					case 0x1: reg_x = tmp1; break;
					case 0x2: reg_y = tmp1; break;
					case 0x3: reg_u = tmp1; break;
					case 0x4: reg_s = tmp1; break;
					case 0x5: reg_pc = tmp1; break;
					case 0x8: reg_a = tmp1; break;
					case 0x9: reg_b = tmp1; break;
					case 0xa: reg_cc = tmp1 & 0xff; break;
					case 0xb: reg_dp = tmp1 & 0xff; break;
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
			/* 0x38 ANDCC immediate (illegal) */
			case 0x38: {
				unsigned int data;
				BYTE_IMMEDIATE(0,data);
				reg_cc &= data;
				peek_byte(reg_pc);
				/* Differs from legal 0x1c version by
				 * taking one more cycle: */
				TAKEN_CYCLES(1);
			} break;
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
				unsigned int data;
				BYTE_IMMEDIATE(0,data);
				reg_cc &= data;
				peek_byte(reg_pc);
				TAKEN_CYCLES(1);
				PUSH_IRQ_REGISTERS();
				TAKEN_CYCLES(1);
				cpu_state = m6809_flow_dispatch_irq;
				goto done_instruction;
			} break;
			/* 0x3D MUL inherent */
			case 0x3d: {
				unsigned int tmp = reg_a * reg_b;
				set_reg_d(tmp);
				CLR_ZC;
				SET_Z(tmp);
				if (tmp & 0x80)
					reg_cc |= CC_C;
				peek_byte(reg_pc);
				TAKEN_CYCLES(9);
			} break;
			/* 0x3e RESET (illegal) */
			case 0x3e:
				peek_byte(reg_pc);
				PUSH_IRQ_REGISTERS_NO_E();
				TAKE_INTERRUPT(swi, CC_F|CC_I, 0xfffe);
				break;
			/* 0x3F SWI inherent */
			case 0x3f:
				peek_byte(reg_pc);
				PUSH_IRQ_REGISTERS();
				TAKE_INTERRUPT(swi, CC_F|CC_I, 0xfffa);
				break;
			/* 0x40 NEGA inherent */
			case 0x40:
			case 0x41: OP_NEGR(reg_a); break;
			/* 0x42 NEG/COM inherent (illegal) */
			case 0x42:
				   if (reg_cc & CC_C) {
					   OP_COMR(reg_a);
				   } else {
					   OP_NEGR(reg_a);
				   }
				   break;
			/* 0x43 COMA inherent */
			case 0x43: OP_COMR(reg_a); break;
			/* 0x44 LSRA inherent */
			case 0x44:
			case 0x45: /* (illegal) */
				OP_LSRR(reg_a); break;
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
			case 0x4b: /* (illegal) */
				OP_DECR(reg_a); break;
			/* 0x4C INCA inherent */
			case 0x4c: OP_INCR(reg_a); break;
			/* 0x4D TSTA inherent */
			case 0x4d: OP_TSTR(reg_a); break;
			/* 0x4F CLRA inherent */
			case 0x4e: /* (illegal) */
			case 0x4f:
				OP_CLRR(reg_a); break;
			/* 0x50 NEGB inherent */
			case 0x50:
			case 0x51: OP_NEGR(reg_b); break;
			/* 0x52 NEG/COM inherent (illegal) */
			case 0x52:
				   if (reg_cc & CC_C) {
					   OP_COMR(reg_b);
				   } else {
					   OP_NEGR(reg_b);
				   }
				   break;
			/* 0x53 COMB inherent */
			case 0x53: OP_COMR(reg_b); break;
			/* 0x54 LSRB inherent */
			case 0x54:
			case 0x55: /* (illegal) */
				OP_LSRR(reg_b); break;
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
			case 0x5b: /* (illegal) */
				OP_DECR(reg_b); break;
			/* 0x5C INCB inherent */
			case 0x5c: OP_INCR(reg_b); break;
			/* 0x5D TSTB inherent */
			case 0x5d: OP_TSTR(reg_b); break;
			/* 0x5F CLRB inherent */
			case 0x5e: /* (illegal) */
			case 0x5f:
				OP_CLRR(reg_b); break;
			/* 0x60 NEG indexed */
			case 0x60:
			case 0x61: OP_NEG(BYTE_INDEXED); break;
			/* 0x62 NEG/COM indexed (illegal) */
			case 0x62:
				   if (reg_cc & CC_C) {
					   OP_COM(BYTE_INDEXED);
				   } else {
					   OP_NEG(BYTE_INDEXED);
				   }
				   break;
			/* 0x63 COM indexed */
			case 0x63: OP_COM(BYTE_INDEXED); break;
			/* 0x64 LSR indexed */
			case 0x64:
			case 0x65: /* (illegal) */
				OP_LSR(BYTE_INDEXED); break;
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
			case 0x6b: /* (illegal) */
				OP_DEC(BYTE_INDEXED); break;
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
			/* 0x72 NEG/COM indexed (illegal) */
			case 0x72:
				   if (reg_cc & CC_C) {
					   OP_COM(BYTE_EXTENDED);
				   } else {
					   OP_NEG(BYTE_EXTENDED);
				   }
				   break;
			/* 0x73 COM extended */
			case 0x73: OP_COM(BYTE_EXTENDED); break;
			/* 0x74 LSR extended */
			case 0x74:
			case 0x75: /* (illegal) */
				OP_LSR(BYTE_EXTENDED); break;
			/* 0x76 ROR extended */
			case 0x76: OP_ROR(BYTE_EXTENDED); break;
			/* 0x77 ASR extended */
			case 0x77: OP_ASR(BYTE_EXTENDED); break;
			/* 0x78 ASL,LSL extended */
			case 0x78: OP_ASL(BYTE_EXTENDED); break;
			/* 0x79 ROL extended */
			case 0x79: OP_ROL(BYTE_EXTENDED); break;
			/* 0x7A DEC extended */
			case 0x7a: /* (illegal) */
			case 0x7b:
				OP_DEC(BYTE_EXTENDED); break;
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
			/* 0x87 Discard immediate (illegal) */
			case 0x87: OP_DISCARD(BYTE_IMMEDIATE); break;
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
				unsigned int new_pc;
				SHORT_RELATIVE(new_pc);
				new_pc = reg_pc + new_pc;
				TAKEN_CYCLES(3);
				PUSHWORD(reg_s, reg_pc);
				reg_pc = new_pc;
			} break;
			/* 0x8E LDX immediate */
			case 0x8e: OP_LD16(reg_x, WORD_IMMEDIATE); break;
			/* 0x8f STX immediate (illegal) */
			case 0x8f: OP_ST16_IMM(reg_x); break;
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
			/* 0xc7 Discard immediate (illegal) */
			case 0xc7: OP_DISCARD(BYTE_IMMEDIATE); break;
			/* 0xC8 EORB immediate */
			case 0xc8: OP_EOR(reg_b, BYTE_IMMEDIATE); break;
			/* 0xC9 ADCB immediate */
			case 0xc9: OP_ADC(reg_b, BYTE_IMMEDIATE); break;
			/* 0xCA ORB immediate */
			case 0xca: OP_OR(reg_b, BYTE_IMMEDIATE); break;
			/* 0xCB ADDB immediate */
			case 0xcb: OP_ADD(reg_b, BYTE_IMMEDIATE); break;
			/* 0xCC LDD immediate */
			case 0xcc: OP_LDD_IMM(); break;
			/* 0xcd HCF? (illegal) */
			case 0xcd:
				cpu_state = m6809_flow_hcf;
				goto done_instruction;
			/* 0xCE LDU immediate */
			case 0xce: OP_LD16(reg_u, WORD_IMMEDIATE); break;
			/* 0xcf STU immediate (illegal) */
			case 0xcf: OP_ST16_IMM(reg_u); break;
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
			case 0xdc: OP_LDD(EA_DIRECT); break;
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
			case 0xec: OP_LDD(EA_INDEXED); break;
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
			case 0xfc: OP_LDD(EA_EXTENDED); break;
			/* 0xFD STD extended */
			case 0xfd: OP_STD(EA_EXTENDED); break;
			/* 0xFE LDU extended */
			case 0xfe: OP_LD16(reg_u, WORD_EXTENDED); break;
			/* 0xFF STU extended */
			case 0xff: OP_ST16(reg_u, EA_EXTENDED); break;
			/* Illegal instruction */
			default: TAKEN_CYCLES(1); break;
			}
			cpu_state = m6809_flow_label_a;
			goto done_instruction;
			}

		case m6809_flow_instruction_page_2:
			{
			unsigned int op;
			BYTE_IMMEDIATE(0,op);
			switch (op) {
			/* 0x10, 0x11 Page 2 */
			case 0x10:
			case 0x11:
				cpu_state = m6809_flow_instruction_page_2;
				continue;
			/* 0x1020 LBRA relative (illegal) */
			case 0x20: BRANCHL(1); break;
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
				peek_byte(reg_pc);
				PUSH_IRQ_REGISTERS();
				TAKE_INTERRUPT(swi2, 0, 0xfff4);
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
			cpu_state = m6809_flow_label_a;
			goto done_instruction;
			}

		case m6809_flow_instruction_page_3:
			{
			unsigned int op;
			BYTE_IMMEDIATE(0,op);
			switch (op) {
			/* 0x10, 0x11 Page 3 */
			case 0x10:
			case 0x11:
				cpu_state = m6809_flow_instruction_page_3;
				continue;
			/* 0x113F SWI3 inherent */
			case 0x3f:
				peek_byte(reg_pc);
				PUSH_IRQ_REGISTERS();
				TAKE_INTERRUPT(swi3, 0, 0xfff2);
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
			cpu_state = m6809_flow_label_a;
			goto done_instruction;
			}

		/* Certain illegal instructions cause the CPU to lock up: */
		case m6809_flow_hcf:
			TAKEN_CYCLES(1);
			continue;

done_instruction:
			/* Instruction post-hook */
			if (m6809_instruction_posthook) {
				M6809State state;
				state.reg_cc = reg_cc;
				state.reg_a  = reg_a;
				state.reg_b  = reg_b;
				state.reg_dp = reg_dp;
				state.reg_x  = reg_x;
				state.reg_y  = reg_y;
				state.reg_u  = reg_u;
				state.reg_s  = reg_s;
				state.reg_pc = reg_pc;
				m6809_instruction_posthook(&state);
			}
			continue;

		}
	}

}

void m6809_get_state(M6809State *state) {
	state->reg_cc = reg_cc;
	state->reg_a = reg_a;
	state->reg_b = reg_b;
	state->reg_dp = reg_dp;
	state->reg_x = reg_x;
	state->reg_y = reg_y;
	state->reg_u = reg_u;
	state->reg_s = reg_s;
	state->reg_pc = reg_pc;
	state->halt = halt;
	state->nmi = nmi;
	state->firq = firq;
	state->irq = irq;
	state->cpu_state = cpu_state;
	state->nmi_armed = nmi_armed;
}

void m6809_set_state(M6809State *state) {
	reg_cc = state->reg_cc;
	reg_a = state->reg_a;
	reg_b = state->reg_b;
	reg_dp = state->reg_dp;
	reg_x = state->reg_x;
	reg_y = state->reg_y;
	reg_u = state->reg_u;
	reg_s = state->reg_s;
	reg_pc = state->reg_pc;
	halt = state->halt;
	nmi = state->nmi;
	firq = state->firq;
	irq = state->irq;
	cpu_state = state->cpu_state;
	nmi_armed = state->nmi_armed;
}

void m6809_jump(unsigned int pc) {
	reg_pc = pc;
}

void m6809_halt_set(void) {
	if (!halt) {
		halt_cycle = cycle + 1;
	}
	halt = 1;
}

void m6809_halt_clear(void) {
	halt = 0;
}

void m6809_nmi_set(void) {
	if (!nmi) {
		nmi_cycle = cycle + 1;
	}
	nmi = 1;
}

void m6809_nmi_clear(void) {
	nmi = 0;
}

void m6809_firq_set(void) {
	if (!firq) {
		firq_cycle = cycle + 1;
	}
	firq = 1;
}

void m6809_firq_clear(void) {
	firq = 0;
}

void m6809_irq_set(void) {
	if (!irq) {
		irq_cycle = cycle + 1;
	}
	irq = 1;
}

void m6809_irq_clear(void) {
	irq = 0;
}
