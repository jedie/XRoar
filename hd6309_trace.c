/*  Copyright 2003-2012 Ciaran Anscomb
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

#include "config.h"

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "hd6309.h"
#include "hd6309_trace.h"

enum {
	PAGE0 = 0, PAGE2 = 1, PAGE3 = 2, ILLEGAL,
	INHERENT, WORD_IMMEDIATE, QUAD_IMMEDIATE, IMMEDIATE, EXTENDED,
	DIRECT, INDEXED, RELATIVE, LONG_RELATIVE,
	STACKS, STACKU, REGISTER, IRQVECTOR, MEMBIT,
	INMEM_DIRECT, INMEM_INDEXED, INMEM_EXTENDED,
	TFMPP, TFMMM, TFMP0, TFM0P
};

static struct {
	const char *mnemonic;
	int type;
} instructions[3][256] = {

	{
		/* 0x00 - 0x0F */
		{ "NEG", DIRECT },
		{ "OIM", INMEM_DIRECT },
		{ "AIM", INMEM_DIRECT },
		{ "COM", DIRECT },
		{ "LSR", DIRECT },
		{ "EIM", INMEM_DIRECT },
		{ "ROR", DIRECT },
		{ "ASR", DIRECT },
		{ "LSL", DIRECT },
		{ "ROL", DIRECT },
		{ "DEC", DIRECT },
		{ "TIM", INMEM_DIRECT },
		{ "INC", DIRECT },
		{ "TST", DIRECT },
		{ "JMP", DIRECT },
		{ "CLR", DIRECT },
		/* 0x10 - 0x1F */
		{ "*", PAGE2 },
		{ "*", PAGE3 },
		{ "NOP", INHERENT },
		{ "SYNC", INHERENT },
		{ "SEXW", INHERENT },
		{ "*", ILLEGAL },
		{ "LBRA", LONG_RELATIVE },
		{ "LBSR", LONG_RELATIVE },
		{ "*", ILLEGAL },
		{ "DAA", INHERENT },
		{ "ORCC", IMMEDIATE },
		{ "*", ILLEGAL },
		{ "ANDCC", IMMEDIATE },
		{ "SEX", INHERENT },
		{ "EXG", REGISTER },
		{ "TFR", REGISTER },
		/* 0x20 - 0x2F */
		{ "BRA", RELATIVE },
		{ "BRN", RELATIVE },
		{ "BHI", RELATIVE },
		{ "BLS", RELATIVE },
		{ "BCC", RELATIVE },
		{ "BCS", RELATIVE },
		{ "BNE", RELATIVE },
		{ "BEQ", RELATIVE },
		{ "BVC", RELATIVE },
		{ "BVS", RELATIVE },
		{ "BPL", RELATIVE },
		{ "BMI", RELATIVE },
		{ "BGE", RELATIVE },
		{ "BLT", RELATIVE },
		{ "BGT", RELATIVE },
		{ "BLE", RELATIVE },
		/* 0x30 - 0x3F */
		{ "LEAX", INDEXED },
		{ "LEAY", INDEXED },
		{ "LEAS", INDEXED },
		{ "LEAU", INDEXED },
		{ "PSHS", STACKS },
		{ "PULS", STACKS },
		{ "PSHU", STACKU },
		{ "PULU", STACKU },
		{ "*", ILLEGAL },
		{ "RTS", INHERENT },
		{ "ABX", INHERENT },
		{ "RTI", INHERENT },
		{ "CWAI", IMMEDIATE },
		{ "MUL", INHERENT },
		{ "*", ILLEGAL },
		{ "SWI", INHERENT },
		/* 0x40 - 0x4F */
		{ "NEGA", INHERENT },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "COMA", INHERENT },
		{ "LSRA", INHERENT },
		{ "*", ILLEGAL },
		{ "RORA", INHERENT },
		{ "ASRA", INHERENT },
		{ "LSLA", INHERENT },
		{ "ROLA", INHERENT },
		{ "DECA", INHERENT },
		{ "*", ILLEGAL },
		{ "INCA", INHERENT },
		{ "TSTA", INHERENT },
		{ "*", ILLEGAL },
		{ "CLRA", INHERENT },
		/* 0x50 - 0x5F */
		{ "NEGB", INHERENT },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "COMB", INHERENT },
		{ "LSRB", INHERENT },
		{ "*", ILLEGAL },
		{ "RORB", INHERENT },
		{ "ASRB", INHERENT },
		{ "LSLB", INHERENT },
		{ "ROLB", INHERENT },
		{ "DECB", INHERENT },
		{ "*", ILLEGAL },
		{ "INCB", INHERENT },
		{ "TSTB", INHERENT },
		{ "*", ILLEGAL },
		{ "CLRB", INHERENT },
		/* 0x60 - 0x6F */
		{ "NEG", INDEXED },
		{ "OIM", INMEM_INDEXED },
		{ "AIM", INMEM_INDEXED },
		{ "COM", INDEXED },
		{ "LSR", INDEXED },
		{ "EIM", INMEM_INDEXED },
		{ "ROR", INDEXED },
		{ "ASR", INDEXED },
		{ "LSL", INDEXED },
		{ "ROL", INDEXED },
		{ "DEC", INDEXED },
		{ "TIM", INMEM_INDEXED },
		{ "INC", INDEXED },
		{ "TST", INDEXED },
		{ "JMP", INDEXED },
		{ "CLR", INDEXED },
		/* 0x70 - 0x7F */
		{ "NEG", EXTENDED },
		{ "OIM", INMEM_EXTENDED },
		{ "AIM", INMEM_EXTENDED },
		{ "COM", EXTENDED },
		{ "LSR", EXTENDED },
		{ "EIM", INMEM_EXTENDED },
		{ "ROR", EXTENDED },
		{ "ASR", EXTENDED },
		{ "LSL", EXTENDED },
		{ "ROL", EXTENDED },
		{ "DEC", EXTENDED },
		{ "TIM", INMEM_EXTENDED },
		{ "INC", EXTENDED },
		{ "TST", EXTENDED },
		{ "JMP", EXTENDED },
		{ "CLR", EXTENDED },
		/* 0x80 - 0x8F */
		{ "SUBA", IMMEDIATE },
		{ "CMPA", IMMEDIATE },
		{ "SBCA", IMMEDIATE },
		{ "SUBD", WORD_IMMEDIATE },
		{ "ANDA", IMMEDIATE },
		{ "BITA", IMMEDIATE },
		{ "LDA", IMMEDIATE },
		{ "*", ILLEGAL },
		{ "EORA", IMMEDIATE },
		{ "ADCA", IMMEDIATE },
		{ "ORA", IMMEDIATE },
		{ "ADDA", IMMEDIATE },
		{ "CMPX", WORD_IMMEDIATE },
		{ "BSR", RELATIVE },
		{ "LDX", WORD_IMMEDIATE },
		{ "*", ILLEGAL },
		/* 0x90 - 0x9F */
		{ "SUBA", DIRECT },
		{ "CMPA", DIRECT },
		{ "SBCA", DIRECT },
		{ "SUBD", DIRECT },
		{ "ANDA", DIRECT },
		{ "BITA", DIRECT },
		{ "LDA", DIRECT },
		{ "STA", DIRECT },
		{ "EORA", DIRECT },
		{ "ADCA", DIRECT },
		{ "ORA", DIRECT },
		{ "ADDA", DIRECT },
		{ "CMPX", DIRECT },
		{ "JSR", DIRECT },
		{ "LDX", DIRECT },
		{ "STX", DIRECT },
		/* 0xA0 - 0xAF */
		{ "SUBA", INDEXED },
		{ "CMPA", INDEXED },
		{ "SBCA", INDEXED },
		{ "SUBD", INDEXED },
		{ "ANDA", INDEXED },
		{ "BITA", INDEXED },
		{ "LDA", INDEXED },
		{ "STA", INDEXED },
		{ "EORA", INDEXED },
		{ "ADCA", INDEXED },
		{ "ORA", INDEXED },
		{ "ADDA", INDEXED },
		{ "CMPX", INDEXED },
		{ "JSR", INDEXED },
		{ "LDX", INDEXED },
		{ "STX", INDEXED },
		/* 0xB0 - 0xBF */
		{ "SUBA", EXTENDED },
		{ "CMPA", EXTENDED },
		{ "SBCA", EXTENDED },
		{ "SUBD", EXTENDED },
		{ "ANDA", EXTENDED },
		{ "BITA", EXTENDED },
		{ "LDA", EXTENDED },
		{ "STA", EXTENDED },
		{ "EORA", EXTENDED },
		{ "ADCA", EXTENDED },
		{ "ORA", EXTENDED },
		{ "ADDA", EXTENDED },
		{ "CMPX", EXTENDED },
		{ "JSR", EXTENDED },
		{ "LDX", EXTENDED },
		{ "STX", EXTENDED },
		/* 0xC0 - 0xCF */
		{ "SUBB", IMMEDIATE },
		{ "CMPB", IMMEDIATE },
		{ "SBCB", IMMEDIATE },
		{ "ADDD", WORD_IMMEDIATE },
		{ "ANDB", IMMEDIATE },
		{ "BITB", IMMEDIATE },
		{ "LDB", IMMEDIATE },
		{ "*", ILLEGAL },
		{ "EORB", IMMEDIATE },
		{ "ADCB", IMMEDIATE },
		{ "ORB", IMMEDIATE },
		{ "ADDB", IMMEDIATE },
		{ "LDD", WORD_IMMEDIATE },
		{ "LDQ", QUAD_IMMEDIATE },
		{ "LDU", WORD_IMMEDIATE },
		{ "*", ILLEGAL },
		/* 0xD0 - 0xDF */
		{ "SUBB", DIRECT },
		{ "CMPB", DIRECT },
		{ "SBCB", DIRECT },
		{ "ADDD", DIRECT },
		{ "ANDB", DIRECT },
		{ "BITB", DIRECT },
		{ "LDB", DIRECT },
		{ "STB", DIRECT },
		{ "EORB", DIRECT },
		{ "ADCB", DIRECT },
		{ "ORB", DIRECT },
		{ "ADDB", DIRECT },
		{ "LDD", DIRECT },
		{ "STD", DIRECT },
		{ "LDU", DIRECT },
		{ "STU", DIRECT },
		/* 0xE0 - 0xEF */
		{ "SUBB", INDEXED },
		{ "CMPB", INDEXED },
		{ "SBCB", INDEXED },
		{ "ADDD", INDEXED },
		{ "ANDB", INDEXED },
		{ "BITB", INDEXED },
		{ "LDB", INDEXED },
		{ "STB", INDEXED },
		{ "EORB", INDEXED },
		{ "ADCB", INDEXED },
		{ "ORB", INDEXED },
		{ "ADDB", INDEXED },
		{ "LDD", INDEXED },
		{ "STD", INDEXED },
		{ "LDU", INDEXED },
		{ "STU", INDEXED },
		/* 0xF0 - 0xFF */
		{ "SUBB", EXTENDED },
		{ "CMPB", EXTENDED },
		{ "SBCB", EXTENDED },
		{ "ADDD", EXTENDED },
		{ "ANDB", EXTENDED },
		{ "BITB", EXTENDED },
		{ "LDB", EXTENDED },
		{ "STB", EXTENDED },
		{ "EORB", EXTENDED },
		{ "ADCB", EXTENDED },
		{ "ORB", EXTENDED },
		{ "ADDB", EXTENDED },
		{ "LDD", EXTENDED },
		{ "STD", EXTENDED },
		{ "LDU", EXTENDED },
		{ "STU", EXTENDED }
	}, {

		/* 0x1000 - 0x100F */
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		/* 0x1010 - 0x101F */
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		/* 0x1020 - 0x102F */
		{ "*", ILLEGAL },
		{ "LBRN", LONG_RELATIVE },
		{ "LBHI", LONG_RELATIVE },
		{ "LBLS", LONG_RELATIVE },
		{ "LBCC", LONG_RELATIVE },
		{ "LBCS", LONG_RELATIVE },
		{ "LBNE", LONG_RELATIVE },
		{ "LBEQ", LONG_RELATIVE },
		{ "LBVC", LONG_RELATIVE },
		{ "LBVS", LONG_RELATIVE },
		{ "LBPL", LONG_RELATIVE },
		{ "LBMI", LONG_RELATIVE },
		{ "LBGE", LONG_RELATIVE },
		{ "LBLT", LONG_RELATIVE },
		{ "LBGT", LONG_RELATIVE },
		{ "LBLE", LONG_RELATIVE },
		/* 0x1030 - 0x103F */
		{ "ADDR", REGISTER },
		{ "ADCR", REGISTER },
		{ "SUBR", REGISTER },
		{ "SBCR", REGISTER },
		{ "ANDR", REGISTER },
		{ "ORR", REGISTER },
		{ "EORR", REGISTER },
		{ "CMPR", REGISTER },
		{ "PSHSW", INHERENT },
		{ "PULSW", INHERENT },
		{ "PSHUW", INHERENT },
		{ "PULUW", INHERENT },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "SWI2", INHERENT },
		/* 0x1040 - 0x104F */
		{ "NEGD", INHERENT },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "COMD", INHERENT },
		{ "LSRD", INHERENT },
		{ "*", ILLEGAL },
		{ "RORD", INHERENT },
		{ "ASRD", INHERENT },
		{ "LSLD", INHERENT },
		{ "ROLD", INHERENT },
		{ "DECD", INHERENT },
		{ "*", ILLEGAL },
		{ "INCD", INHERENT },
		{ "TSTD", INHERENT },
		{ "*", ILLEGAL },
		{ "CLRD", INHERENT },
		/* 0x1050 - 0x105F */
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "COMW", ILLEGAL },
		{ "LSRW", ILLEGAL },
		{ "*", ILLEGAL },
		{ "RORW", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "ROLW", ILLEGAL },
		{ "DECW", ILLEGAL },
		{ "*", ILLEGAL },
		{ "INCW", ILLEGAL },
		{ "TSTW", ILLEGAL },
		{ "*", ILLEGAL },
		{ "CLRW", ILLEGAL },
		/* 0x1060 - 0x106F */
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		/* 0x1070 - 0x107F */
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		/* 0x1080 - 0x108F */
		{ "SUBW", WORD_IMMEDIATE },
		{ "CMPW", WORD_IMMEDIATE },
		{ "SBCD", WORD_IMMEDIATE },
		{ "CMPD", WORD_IMMEDIATE },
		{ "ANDD", WORD_IMMEDIATE },
		{ "BITD", WORD_IMMEDIATE },
		{ "LDW", WORD_IMMEDIATE },
		{ "*", ILLEGAL },
		{ "EORD", WORD_IMMEDIATE },
		{ "ADCD", WORD_IMMEDIATE },
		{ "ORD", WORD_IMMEDIATE },
		{ "ADDW", WORD_IMMEDIATE },
		{ "CMPY", WORD_IMMEDIATE },
		{ "*", ILLEGAL },
		{ "LDY", WORD_IMMEDIATE },
		{ "*", ILLEGAL },
		/* 0x1090 - 0x109F */
		{ "SUBW", DIRECT },
		{ "CMPW", DIRECT },
		{ "SBCD", DIRECT },
		{ "CMPD", DIRECT },
		{ "ANDD", DIRECT },
		{ "BITD", DIRECT },
		{ "LDW", DIRECT },
		{ "STW", DIRECT },
		{ "EORD", DIRECT },
		{ "ADCD", DIRECT },
		{ "ORD", DIRECT },
		{ "ADDW", DIRECT },
		{ "CMPY", DIRECT },
		{ "*", ILLEGAL },
		{ "LDY", DIRECT },
		{ "STY", DIRECT },
		/* 0x10A0 - 0x10AF */
		{ "SUBW", INDEXED },
		{ "CMPW", INDEXED },
		{ "SBCD", INDEXED },
		{ "CMPD", INDEXED },
		{ "ANDD", INDEXED },
		{ "BITD", INDEXED },
		{ "LDW", INDEXED },
		{ "STW", INDEXED },
		{ "EORD", INDEXED },
		{ "ADCD", INDEXED },
		{ "ORD", INDEXED },
		{ "ADDW", INDEXED },
		{ "CMPY", INDEXED },
		{ "*", ILLEGAL },
		{ "LDY", INDEXED },
		{ "STY", INDEXED },
		/* 0x10B0 - 0x10BF */
		{ "SUBW", EXTENDED },
		{ "CMPW", EXTENDED },
		{ "SBCD", EXTENDED },
		{ "CMPD", EXTENDED },
		{ "ANDD", EXTENDED },
		{ "BITD", EXTENDED },
		{ "LDW", EXTENDED },
		{ "STW", EXTENDED },
		{ "EORD", EXTENDED },
		{ "ADCD", EXTENDED },
		{ "ORD", EXTENDED },
		{ "ADDW", EXTENDED },
		{ "CMPY", EXTENDED },
		{ "*", ILLEGAL },
		{ "LDY", EXTENDED },
		{ "STY", EXTENDED },
		/* 0x10C0 - 0x10CF */
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "LDS", WORD_IMMEDIATE },
		{ "*", ILLEGAL },
		/* 0x10D0 - 0x10DF */
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "LDQ", DIRECT },
		{ "STQ", DIRECT },
		{ "LDS", DIRECT },
		{ "STS", DIRECT },
		/* 0x10E0 - 0x10EF */
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "LDQ", INDEXED },
		{ "STQ", INDEXED },
		{ "LDS", INDEXED },
		{ "STS", INDEXED },
		/* 0x10F0 - 0x10FF */
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "LDQ", EXTENDED },
		{ "STQ", EXTENDED },
		{ "LDS", EXTENDED },
		{ "STS", EXTENDED }
	}, {

		/* 0x1100 - 0x110F */
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		/* 0x1110 - 0x111F */
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		/* 0x1120 - 0x112F */
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		/* 0x1130 - 0x113F */
		{ "BAND", MEMBIT },
		{ "BIAND", MEMBIT },
		{ "BOR", MEMBIT },
		{ "BIOR", MEMBIT },
		{ "BEOR", MEMBIT },
		{ "BIEOR", MEMBIT },
		{ "LDBT", MEMBIT },
		{ "STBT", MEMBIT },
		{ "TFM", TFMPP },
		{ "TFM", TFMMM },
		{ "TFM", TFMP0 },
		{ "TFM", TFM0P },
		{ "BITMD", IMMEDIATE },
		{ "LDMD", IMMEDIATE },
		{ "*", ILLEGAL },
		{ "SWI3", INHERENT },
		/* 0x1140 - 0x114F */
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "COME", INHERENT },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "DECE", INHERENT },
		{ "*", ILLEGAL },
		{ "INCE", INHERENT },
		{ "TSTE", INHERENT },
		{ "*", ILLEGAL },
		{ "CLRE", INHERENT },
		/* 0x1150 - 0x115F */
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "COMF", INHERENT },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "DECF", INHERENT },
		{ "*", ILLEGAL },
		{ "INCF", INHERENT },
		{ "TSTF", INHERENT },
		{ "*", ILLEGAL },
		{ "CLRF", INHERENT },
		/* 0x1160 - 0x116F */
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		/* 0x1170 - 0x117F */
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		/* 0x1180 - 0x118F */
		{ "SUBE", IMMEDIATE },
		{ "CMPE", IMMEDIATE },
		{ "*", ILLEGAL },
		{ "CMPU", WORD_IMMEDIATE },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "LDE", IMMEDIATE },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "ADDE", IMMEDIATE },
		{ "CMPS", WORD_IMMEDIATE },
		{ "DIVD", IMMEDIATE },
		{ "DIVQ", WORD_IMMEDIATE },
		{ "MULD", WORD_IMMEDIATE },
		/* 0x1190 - 0x119F */
		{ "SUBE", DIRECT },
		{ "CMPE", DIRECT },
		{ "*", ILLEGAL },
		{ "CMPU", DIRECT },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "LDE", DIRECT },
		{ "STE", DIRECT },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "ADDE", DIRECT },
		{ "CMPS", DIRECT },
		{ "DIVD", DIRECT },
		{ "DIVQ", DIRECT },
		{ "MULD", DIRECT },
		/* 0x11A0 - 0x11AF */
		{ "SUBE", INDEXED },
		{ "CMPE", INDEXED },
		{ "*", ILLEGAL },
		{ "CMPU", INDEXED },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "LDE", INDEXED },
		{ "STE", INDEXED },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "ADDE", INDEXED },
		{ "CMPS", INDEXED },
		{ "DIVD", INDEXED },
		{ "DIVQ", INDEXED },
		{ "MULD", INDEXED },
		/* 0x11B0 - 0x11BF */
		{ "SUBE", EXTENDED },
		{ "CMPE", EXTENDED },
		{ "*", ILLEGAL },
		{ "CMPU", EXTENDED },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "LDE", EXTENDED },
		{ "STE", EXTENDED },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "ADDE", EXTENDED },
		{ "CMPS", EXTENDED },
		{ "DIVD", EXTENDED },
		{ "DIVQ", EXTENDED },
		{ "MULD", EXTENDED },
		/* 0x11C0 - 0x11CF */
		{ "SUBF", IMMEDIATE },
		{ "CMPF", IMMEDIATE },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "LDF", IMMEDIATE },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "ADDF", IMMEDIATE },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		/* 0x11D0 - 0x11DF */
		{ "SUBF", DIRECT },
		{ "CMPF", DIRECT },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "LDF", DIRECT },
		{ "STF", DIRECT },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "ADDF", DIRECT },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		/* 0x11E0 - 0x11EF */
		{ "SUBF", INDEXED },
		{ "CMPF", INDEXED },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "LDF", INDEXED },
		{ "STF", INDEXED },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "ADDF", INDEXED },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		/* 0x11F0 - 0x11FF */
		{ "SUBF", EXTENDED },
		{ "CMPF", EXTENDED },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "LDF", EXTENDED },
		{ "STF", EXTENDED },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "ADDF", EXTENDED },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
		{ "*", ILLEGAL },
	}
};

#define INDEXED_WANT_REG   (1<<0)
#define INDEXED_WANT_BYTE  (1<<1)
#define INDEXED_WANT_WORD  (1<<2)

static struct {
	const char *fmt;
	int flags;
} indexed_modes[] = {
	{ "%s,%s+%s",      INDEXED_WANT_REG },
	{ "%s,%s++%s",     INDEXED_WANT_REG },
	{ "%s,-%s%s",      INDEXED_WANT_REG },
	{ "%s,--%s%s",     INDEXED_WANT_REG },
	{ "%s,%s%s",       INDEXED_WANT_REG },
	{ "%sB,%s%s",      INDEXED_WANT_REG },
	{ "%sA,%s%s",      INDEXED_WANT_REG },
	{ "%sE,%s%s",      INDEXED_WANT_REG },
	{ "%s$%02x,%s%s",  INDEXED_WANT_BYTE | INDEXED_WANT_REG },
	{ "%s$%04x,%s%s",  INDEXED_WANT_WORD | INDEXED_WANT_REG },
	{ "%sF,%s%s",      INDEXED_WANT_REG },
	{ "%sD,%s%s",      INDEXED_WANT_REG },
	{ "%s$%02x,PCR%s", INDEXED_WANT_BYTE },
	{ "%s$%04x,PCR%s", INDEXED_WANT_WORD },
	{ "%sW,%s%s",      INDEXED_WANT_REG },
	{ "%s$%04X%s",     INDEXED_WANT_WORD }
};

enum {
	WANT_INSTRUCTION, WANT_BYTE, WANT_BYTE2,
	WANT_WORD1, WANT_WORD2,
	WANT_IRQVEC1, WANT_IRQVEC2, WANT_PRINT
};

static const char *tfr_regs[16] = {
	"D", "X", "Y", "U", "S", "PC", "W", "V",
	"A", "B", "CC", "DP", "0", "0", "E", "F"
};
static const char *indexed_regs[4] = { "X", "Y", "U", "S" };
static const char *membit_regs[4] = { "CC", "A", "B", "*" };

static const char *irq_names[8] = {
	"[ILLEGAL]", "[SWI3]", "[SWI2]", "[FIRQ]",
	"[IRQ]", "[SWI]", "[NMI]", "[RESET]"
};

static int state, page;
static uint16_t instr_pc;
#define BYTES_BUF_SIZE 5
static int bytes_count;
static uint8_t bytes_buf[BYTES_BUF_SIZE];
static int irq_vector;

static const char *mnemonic;
static char operand_text[19];

static void trace_print_short(void);

static void reset_state(void) {
	state = WANT_INSTRUCTION;
	page = PAGE0;
	bytes_count = 0;
	mnemonic = "*";
	operand_text[0] = '*';
	operand_text[1] = '\0';
}

void hd6309_trace_reset(void) {
	reset_state();
	hd6309_trace_irq(NULL, 0xfffe);
}

#define STACK_PRINT(r) do { \
		if (not_first) { strcat(operand_text, "," r); } \
		else { strcat(operand_text, r); not_first = 1; } \
	} while (0)

#define sex5(v) ((int)(((v) & 0x0f) - ((v) & 0x10)))

void hd6309_trace_byte(uint8_t byte, uint16_t pc) {
	static int ins_type = PAGE0;
	static uint8_t byte_val = 0;
	static uint16_t word_val = 0;
	static int indexed_mode = 0;
	static char indexed_fmt[20] = "";
	static int indexed_flags;
	static uint8_t membit_postbyte = 0;
	static _Bool inmem_instr = 0;

	if (bytes_count == 0) {
		instr_pc = pc;
	}

	if (bytes_count < BYTES_BUF_SIZE && state != WANT_PRINT) {
		bytes_buf[bytes_count++] = byte;
	}

	switch (state) {
		default:
		case WANT_INSTRUCTION:
			mnemonic = instructions[page][byte].mnemonic;
			ins_type = instructions[page][byte].type;
			inmem_instr = 0;
			indexed_fmt[0] = 0;
			switch (ins_type) {
				case PAGE2: case PAGE3:
					page = ins_type;
					break;
				default: case ILLEGAL: case INHERENT:
					state = WANT_PRINT;
					break;
				case IMMEDIATE: case DIRECT: case RELATIVE:
				case STACKS: case STACKU: case REGISTER:
				case INDEXED: case MEMBIT:
					state = WANT_BYTE;
					break;
				case WORD_IMMEDIATE: case EXTENDED:
				case LONG_RELATIVE:
					state = WANT_WORD1;
					break;
				case INMEM_DIRECT: case INMEM_INDEXED:
				case INMEM_EXTENDED:
					state = WANT_BYTE;
					break;
			}
			break;
		case WANT_IRQVEC1:
			mnemonic = irq_names[irq_vector];
			ins_type = IRQVECTOR;
			state = WANT_IRQVEC2;
			break;
		case WANT_IRQVEC2:
			state = WANT_PRINT;
			break;
		case WANT_BYTE:
			if (ins_type == INDEXED && indexed_mode == 0) {
				indexed_mode = byte;
				if ((indexed_mode & 0x80) == 0) {
					state = WANT_PRINT;
					break;
				}
				if (byte == 0x8f || byte == 0x90) {
					strcat(indexed_fmt, "%s,W%s");
					indexed_flags = 0;
				} else if (byte == 0xaf || byte == 0xb0) {
					strcat(indexed_fmt, "%s%s,W%s");
					indexed_flags = INDEXED_WANT_WORD;
				} else if (byte == 0xcf || byte == 0xd0) {
					strcat(indexed_fmt, "%s,W++%s");
					indexed_flags = 0;
				} else if (byte == 0xef || byte == 0xf0) {
					strcat(indexed_fmt, "%s,--W%s");
					indexed_flags = 0;
				} else {
					strcat(indexed_fmt, indexed_modes[byte & 0x0f].fmt);
					indexed_flags = indexed_modes[byte & 0x0f].flags;
				}
				if (indexed_flags & INDEXED_WANT_WORD) {
					state = WANT_WORD1;
				} else if (indexed_flags & INDEXED_WANT_BYTE) {
					state = WANT_BYTE;
				} else {
					state = WANT_PRINT;
				}
			} else switch (ins_type) {
			case MEMBIT:
				membit_postbyte = byte;
				state = WANT_BYTE2;
				break;
			case INMEM_DIRECT:
				snprintf(indexed_fmt, sizeof(indexed_fmt), "%02x;", byte_val);
				inmem_instr = DIRECT;
				state = WANT_BYTE;
				break;
			case INMEM_INDEXED:
				snprintf(indexed_fmt, sizeof(indexed_fmt), "%02x;", byte_val);
				inmem_instr = INDEXED;
				state = WANT_BYTE;
				break;
			case INMEM_EXTENDED:
				snprintf(indexed_fmt, sizeof(indexed_fmt), "%02x;", byte_val);
				inmem_instr = EXTENDED;
				state = WANT_WORD1;
				break;
			default:
				byte_val = byte;
				state = WANT_PRINT;
				break;
			}
			break;
		case WANT_BYTE2:
			byte_val = byte;
			state = WANT_PRINT;
			break;
		case WANT_WORD1:
			word_val = byte;
			state = WANT_WORD2;
			break;
		case WANT_WORD2:
			word_val = (word_val << 8) | byte;
			state = WANT_PRINT;
			break;
		case WANT_PRINT:
			/* Now waiting for a call to hd6309_trace_print() */
			return;
	}

	/* If state got set to WANT_PRINT, create the operand text */
	if (state != WANT_PRINT)
		return;

	operand_text[0] = '\0';
	switch (ins_type) {
		case ILLEGAL: case INHERENT:
			break;

		case IMMEDIATE:
			snprintf(operand_text, sizeof(operand_text), "#$%02x", byte_val);
			break;

		case DIRECT:
			snprintf(operand_text, sizeof(operand_text), "<$%02x", byte_val);
			break;

		case WORD_IMMEDIATE:
			snprintf(operand_text, sizeof(operand_text), "#$%04x", word_val);
			break;

		case EXTENDED:
			snprintf(operand_text, sizeof(operand_text), "$%04x", word_val);
			break;

		case STACKS: {
			_Bool not_first = 0;
			if (byte_val & 0x01) { STACK_PRINT("CC"); }
			if (byte_val & 0x02) { STACK_PRINT("A"); }
			if (byte_val & 0x04) { STACK_PRINT("B"); }
			if (byte_val & 0x08) { STACK_PRINT("DP"); }
			if (byte_val & 0x10) { STACK_PRINT("X"); }
			if (byte_val & 0x20) { STACK_PRINT("Y"); }
			if (byte_val & 0x40) { STACK_PRINT("U"); }
			if (byte_val & 0x80) { STACK_PRINT("PC"); }
		} break;

		case STACKU: {
			_Bool not_first = 0;
			if (byte_val & 0x01) { STACK_PRINT("CC"); }
			if (byte_val & 0x02) { STACK_PRINT("A"); }
			if (byte_val & 0x04) { STACK_PRINT("B"); }
			if (byte_val & 0x08) { STACK_PRINT("DP"); }
			if (byte_val & 0x10) { STACK_PRINT("X"); }
			if (byte_val & 0x20) { STACK_PRINT("Y"); }
			if (byte_val & 0x40) { STACK_PRINT("S"); }
			if (byte_val & 0x80) { STACK_PRINT("PC"); }
		} break;

		case REGISTER:
			snprintf(operand_text, sizeof(operand_text), "%s,%s",
					tfr_regs[(byte_val&0xf0)>>4],
					tfr_regs[byte_val&0x0f]);
			break;

		case MEMBIT:
			snprintf(operand_text, sizeof(operand_text), "%s,%d,%d,<$%02x",
					membit_regs[(membit_postbyte&0xc0)>>6],
					(membit_postbyte&0x38)>>3,
					(membit_postbyte&0x07), byte_val);
			break;

		case INDEXED: {
			const char *reg = indexed_regs[(indexed_mode>>5)&3];
			const char *pre = "";
			const char *post = "";
			int value = word_val;
			if (indexed_flags & INDEXED_WANT_BYTE) value = byte_val;
			if ((indexed_mode & 0x80) == 0) {
				value = sex5(indexed_mode & 0x1f);
				snprintf(operand_text, sizeof(operand_text), "%d,%s", value, reg);
				break;
			}
			if (indexed_mode & 0x10) {
				pre = "[";
				post = "]";
			}
			if (indexed_flags & (INDEXED_WANT_WORD | INDEXED_WANT_BYTE)) {
				if (indexed_flags & INDEXED_WANT_REG) {
					snprintf(operand_text, sizeof(operand_text), indexed_fmt, pre, value, reg, post);
				} else {
					snprintf(operand_text, sizeof(operand_text), indexed_fmt, pre, value, post);
				}
			} else {
				if (indexed_flags & INDEXED_WANT_REG) {
					snprintf(operand_text, sizeof(operand_text), indexed_fmt, pre, reg, post);
				} else {
					snprintf(operand_text, sizeof(operand_text), indexed_fmt, pre, post);
				}
			}
			} break;

		case RELATIVE:
			pc += (byte_val & 0x80) ? 0xff00 : 0;
			pc += 1 + byte_val;
			pc &= 0xffff;
			snprintf(operand_text, sizeof(operand_text), "$%04x", pc);
			break;

		case LONG_RELATIVE:
			pc += 1 + word_val;
			pc &= 0xffff;
			snprintf(operand_text, sizeof(operand_text), "$%04x", pc);
			break;

		case IRQVECTOR:
			trace_print_short();
			break;

		default:
			break;
	}
	indexed_mode = 0;
	byte_val = word_val = 0;
}

void hd6309_trace_irq(struct MC6809 *cpu, uint16_t vector) {
	(void)cpu;
	reset_state();
	state = WANT_IRQVEC1;
	irq_vector = (vector & 15) >> 1;
}

static void trace_print_short(void) {
	char bytes_string[(BYTES_BUF_SIZE*2)+1];
	int i;

	if (bytes_count == 0) return;
	if (state != WANT_PRINT) return;

	bytes_string[0] = '\0';
	for (i = 0; i < bytes_count; i++) {
		snprintf(bytes_string + i*2, 3, "%02x", bytes_buf[i]);
	}

	printf("%04x| %-12s%-8s%-20s\n", instr_pc, bytes_string, mnemonic, operand_text);

	reset_state();
}

void hd6309_trace_print(struct MC6809 *cpu) {
	struct HD6309 *hcpu = (struct HD6309 *)cpu;
	char bytes_string[(BYTES_BUF_SIZE*2)+1];
	int i;

	if (bytes_count == 0) return;
	if (state != WANT_PRINT) return;

	bytes_string[0] = '\0';
	for (i = 0; i < bytes_count; i++) {
		snprintf(bytes_string + i*2, 3, "%02x", bytes_buf[i]);
	}

	printf("%04x| %-12s%-8s%-20s", instr_pc, bytes_string, mnemonic, operand_text);
	printf("cc=%02x a=%02x b=%02x e=%02x f=%02x dp=%02x x=%04x y=%04x u=%04x s=%04x v=%04x\n", cpu->reg_cc, MC6809_REG_A(cpu), MC6809_REG_B(cpu), HD6309_REG_E(hcpu), HD6309_REG_F(hcpu), cpu->reg_dp, cpu->reg_x, cpu->reg_y, cpu->reg_u, cpu->reg_s, hcpu->reg_v);
	fflush(stdout);

	reset_state();
}
