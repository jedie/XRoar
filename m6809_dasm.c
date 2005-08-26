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

/* Only worth building this in if TRACE is defined */
#ifdef TRACE

#include "types.h"
#include "logging.h"

enum {
	PAGE0 = 0, PAGE2 = 1, PAGE3 = 2, ILLEGAL,
	INHERENT, WORD_IMMEDIATE, IMMEDIATE, EXTENDED,
	DIRECT, INDEXED, RELATIVE, LONG_RELATIVE,
	STACKS, STACKU, REGISTER
} instruction_type;

typedef struct {
	const char *mnemonic;
	int type; 
} instruction;

static instruction instructions[3][256] = { {
	/* 0x00 - 0x0F */
	{ "NEG",	DIRECT },
	{ "NEG *",	DIRECT },
	{ "NEGCOM*",	DIRECT },
	{ "COM",	DIRECT },
	{ "LSR",	DIRECT },
	{ "LSR *",	DIRECT },
	{ "ROR",	DIRECT },
	{ "ASR",	DIRECT },
	{ "LSL",	DIRECT },
	{ "ROL",	DIRECT },
	{ "DEC",	DIRECT },
	{ "DEC *",	DIRECT },
	{ "INC",	DIRECT },
	{ "TST",	DIRECT },
	{ "JMP",	DIRECT },
	{ "CLR",	DIRECT },
	/* 0x10 - 0x1F */
	{ "",		PAGE2 },
	{ "",		PAGE3 },
	{ "NOP",	INHERENT },
	{ "SYNC",	INHERENT },
	{ "*"	,	ILLEGAL },
	{ "*",		ILLEGAL },
	{ "LBRA",	LONG_RELATIVE },
	{ "LBSR",	LONG_RELATIVE },
	{ "*",		ILLEGAL },
	{ "DAA",	INHERENT },
	{ "ORCC",	IMMEDIATE },
	{ "*",		ILLEGAL },
	{ "ANDCC",	IMMEDIATE },
	{ "SEX",	INHERENT },
	{ "EXG",	REGISTER },
	{ "TFR",	REGISTER },
	/* 0x20 - 0x2F */
	{ "BRA",	RELATIVE },
	{ "BRN",	RELATIVE },
	{ "BHI",	RELATIVE },
	{ "BLS",	RELATIVE },
	{ "BCC",	RELATIVE },
	{ "BCS",	RELATIVE },
	{ "BNE",	RELATIVE },
	{ "BEQ",	RELATIVE },
	{ "BVC",	RELATIVE },
	{ "BVS",	RELATIVE },
	{ "BPL",	RELATIVE },
	{ "BMI",	RELATIVE },
	{ "BGE",	RELATIVE },
	{ "BLT",	RELATIVE },
	{ "BGT",	RELATIVE },
	{ "BLE",	RELATIVE },
	/* 0x30 - 0x3F */
	{ "LEAX",	INDEXED },
	{ "LEAY",	INDEXED },
	{ "LEAS",	INDEXED },
	{ "LEAU",	INDEXED },
	{ "PSHS",	STACKS },
	{ "PULS",	STACKS },
	{ "PSHU",	STACKU },
	{ "PULU",	STACKU },
	{ "*",		ILLEGAL },
	{ "RTS",	INHERENT },
	{ "ABX",	INHERENT },
	{ "RTI",	INHERENT },
	{ "CWAI",	IMMEDIATE },
	{ "MUL",	INHERENT },
	{ "*",		ILLEGAL },
	{ "SWI",	INHERENT },
	/* 0x40 - 0x4F */
	{ "NEGA",	INHERENT },
	{ "NEGA *",	INHERENT },
	{ "COMA *",	INHERENT },
	{ "COMA",	INHERENT },
	{ "LSRA",	INHERENT },
	{ "LSRA *",	INHERENT },
	{ "RORA",	INHERENT },
	{ "ASRA",	INHERENT },
	{ "LSLA",	INHERENT },
	{ "ROLA",	INHERENT },
	{ "DECA",	INHERENT },
	{ "DECA *",	INHERENT },
	{ "INCA",	INHERENT },
	{ "TSTA",	INHERENT },
	{ "*",		ILLEGAL },
	{ "CLRA",	INHERENT },
	/* 0x50 - 0x5F */
	{ "NEGB",	INHERENT },
	{ "NEGB *",	INHERENT },
	{ "COMB *",	INHERENT },
	{ "COMB",	INHERENT },
	{ "LSRB",	INHERENT },
	{ "LSRB *",	INHERENT },
	{ "RORB",	INHERENT },
	{ "ASRB",	INHERENT },
	{ "LSLB",	INHERENT },
	{ "ROLB",	INHERENT },
	{ "DECB",	INHERENT },
	{ "DECB *",	INHERENT },
	{ "INCB",	INHERENT },
	{ "TSTB",	INHERENT },
	{ "*",		ILLEGAL },
	{ "CLRB",	INHERENT },
	/* 0x60 - 0x6F */
	{ "NEG",	INDEXED },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "COM",	INDEXED },
	{ "LSR",	INDEXED },
	{ "*",		ILLEGAL },
	{ "ROR",	INDEXED },
	{ "ASR",	INDEXED },
	{ "LSL",	INDEXED },
	{ "ROL",	INDEXED },
	{ "DEC",	INDEXED },
	{ "*",		ILLEGAL },
	{ "INC",	INDEXED },
	{ "TST",	INDEXED },
	{ "JMP",	INDEXED },
	{ "CLR",	INDEXED },
	/* 0x70 - 0x7F */
	{ "NEG",	EXTENDED },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "COM",	EXTENDED },
	{ "LSR",	EXTENDED },
	{ "*",		ILLEGAL },
	{ "ROR",	EXTENDED },
	{ "ASR",	EXTENDED },
	{ "LSL",	EXTENDED },
	{ "ROL",	EXTENDED },
	{ "DEC",	EXTENDED },
	{ "*",		ILLEGAL },
	{ "INC",	EXTENDED },
	{ "TST",	EXTENDED },
	{ "JMP",	EXTENDED },
	{ "CLR",	EXTENDED },
	/* 0x80 - 0x8F */
	{ "SUBA",	IMMEDIATE },
	{ "CMPA",	IMMEDIATE },
	{ "SBCA",	IMMEDIATE },
	{ "SUBD",	WORD_IMMEDIATE },
	{ "ANDA",	IMMEDIATE },
	{ "BITA",	IMMEDIATE },
	{ "LDA",	IMMEDIATE },
	{ "*",		ILLEGAL },
	{ "EORA",	IMMEDIATE },
	{ "ADCA",	IMMEDIATE },
	{ "ORA",	IMMEDIATE },
	{ "ADDA",	IMMEDIATE },
	{ "CMPX",	WORD_IMMEDIATE },
	{ "BSR",	RELATIVE },
	{ "LDX",	WORD_IMMEDIATE },
	{ "*",		ILLEGAL },
	/* 0x90 - 0x9F */
	{ "SUBA",	DIRECT },
	{ "CMPA",	DIRECT },
	{ "SBCA",	DIRECT },
	{ "SUBD",	DIRECT },
	{ "ANDA",	DIRECT },
	{ "BITA",	DIRECT },
	{ "LDA",	DIRECT },
	{ "STA",	DIRECT },
	{ "EORA",	DIRECT },
	{ "ADCA",	DIRECT },
	{ "ORA",	DIRECT },
	{ "ADDA",	DIRECT },
	{ "CMPX",	DIRECT },
	{ "JSR",	DIRECT },
	{ "LDX",	DIRECT },
	{ "STX",	DIRECT },
	/* 0xA0 - 0xAF */
	{ "SUBA",	INDEXED },
	{ "CMPA",	INDEXED },
	{ "SBCA",	INDEXED },
	{ "SUBD",	INDEXED },
	{ "ANDA",	INDEXED },
	{ "BITA",	INDEXED },
	{ "LDA",	INDEXED },
	{ "STA",	INDEXED },
	{ "EORA",	INDEXED },
	{ "ADCA",	INDEXED },
	{ "ORA",	INDEXED },
	{ "ADDA",	INDEXED },
	{ "CMPX",	INDEXED },
	{ "JSR",	INDEXED },
	{ "LDX",	INDEXED },
	{ "STX",	INDEXED },
	/* 0xB0 - 0xBF */
	{ "SUBA",	EXTENDED },
	{ "CMPA",	EXTENDED },
	{ "SBCA",	EXTENDED },
	{ "SUBD",	EXTENDED },
	{ "ANDA",	EXTENDED },
	{ "BITA",	EXTENDED },
	{ "LDA",	EXTENDED },
	{ "STA",	EXTENDED },
	{ "EORA",	EXTENDED },
	{ "ADCA",	EXTENDED },
	{ "ORA",	EXTENDED },
	{ "ADDA",	EXTENDED },
	{ "CMPX",	EXTENDED },
	{ "JSR",	EXTENDED },
	{ "LDX",	EXTENDED },
	{ "STX",	EXTENDED },
	/* 0xC0 - 0xCF */
	{ "SUBB",	IMMEDIATE },
	{ "CMPB",	IMMEDIATE },
	{ "SBCB",	IMMEDIATE },
	{ "ADDD",	WORD_IMMEDIATE },
	{ "ANDB",	IMMEDIATE },
	{ "BITB",	IMMEDIATE },
	{ "LDB",	IMMEDIATE },
	{ "*",		ILLEGAL },
	{ "EORB",	IMMEDIATE },
	{ "ADCB",	IMMEDIATE },
	{ "ORB",	IMMEDIATE },
	{ "ADDB",	IMMEDIATE },
	{ "LDD",	WORD_IMMEDIATE },
	{ "*",		ILLEGAL },
	{ "LDU",	WORD_IMMEDIATE },
	{ "*",		ILLEGAL },
	/* 0xD0 - 0xDF */
	{ "SUBB",	DIRECT },
	{ "CMPB",	DIRECT },
	{ "SBCB",	DIRECT },
	{ "ADDD",	DIRECT },
	{ "ANDB",	DIRECT },
	{ "BITB",	DIRECT },
	{ "LDB",	DIRECT },
	{ "STB",	DIRECT },
	{ "EORB",	DIRECT },
	{ "ADCB",	DIRECT },
	{ "ORB",	DIRECT },
	{ "ADDB",	DIRECT },
	{ "LDD",	DIRECT },
	{ "STD",	DIRECT },
	{ "LDU",	DIRECT },
	{ "STU",	DIRECT },
	/* 0xE0 - 0xEF */
	{ "SUBB",	INDEXED },
	{ "CMPB",	INDEXED },
	{ "SBCB",	INDEXED },
	{ "ADDD",	INDEXED },
	{ "ANDB",	INDEXED },
	{ "BITB",	INDEXED },
	{ "LDB",	INDEXED },
	{ "STB",	INDEXED },
	{ "EORB",	INDEXED },
	{ "ADCB",	INDEXED },
	{ "ORB",	INDEXED },
	{ "ADDB",	INDEXED },
	{ "LDD",	INDEXED },
	{ "STD",	INDEXED },
	{ "LDU",	INDEXED },
	{ "STU",	INDEXED },
	/* 0xF0 - 0xFF */
	{ "SUBB",	EXTENDED },
	{ "CMPB",	EXTENDED },
	{ "SBCB",	EXTENDED },
	{ "ADDD",	EXTENDED },
	{ "ANDB",	EXTENDED },
	{ "BITB",	EXTENDED },
	{ "LDB",	EXTENDED },
	{ "STB",	EXTENDED },
	{ "EORB",	EXTENDED },
	{ "ADCB",	EXTENDED },
	{ "ORB",	EXTENDED },
	{ "ADDB",	EXTENDED },
	{ "LDD",	EXTENDED },
	{ "STD",	EXTENDED },
	{ "LDU",	EXTENDED },
	{ "STU",	EXTENDED }
}, {
	/* 0x1000 - 0x100F */
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	/* 0x1010 - 0x101F */
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	/* 0x1020 - 0x102F */
	{ "*",		ILLEGAL },
	{ "LBRN",	LONG_RELATIVE },
	{ "LBHI",	LONG_RELATIVE },
	{ "LBLS",	LONG_RELATIVE },
	{ "LBCC",	LONG_RELATIVE },
	{ "LBCS",	LONG_RELATIVE },
	{ "LBNE",	LONG_RELATIVE },
	{ "LBEQ",	LONG_RELATIVE },
	{ "LBVC",	LONG_RELATIVE },
	{ "LBVS",	LONG_RELATIVE },
	{ "LBPL",	LONG_RELATIVE },
	{ "LBMI",	LONG_RELATIVE },
	{ "LBGE",	LONG_RELATIVE },
	{ "LBLT",	LONG_RELATIVE },
	{ "LBGT",	LONG_RELATIVE },
	{ "LBLE",	LONG_RELATIVE },
	/* 0x1030 - 0x103F */
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "SWI2",	INHERENT },
	/* 0x1040 - 0x104F */
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	/* 0x1050 - 0x105F */
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	/* 0x1060 - 0x106F */
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	/* 0x1070 - 0x107F */
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	/* 0x1080 - 0x108F */
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "CMPD",	WORD_IMMEDIATE },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "CMPY",	WORD_IMMEDIATE },
	{ "*",		ILLEGAL },
	{ "LDY",	WORD_IMMEDIATE },
	{ "*",		ILLEGAL },
	/* 0x1090 - 0x109F */
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "CMPD",	DIRECT },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "CMPY",	DIRECT },
	{ "*",		ILLEGAL },
	{ "LDY",	DIRECT },
	{ "STY",	DIRECT },
	/* 0x10A0 - 0x10AF */
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "CMPD",	INDEXED },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "CMPY",	INDEXED },
	{ "*",		ILLEGAL },
	{ "LDY",	INDEXED },
	{ "STY",	INDEXED },
	/* 0x10B0 - 0x10BF */
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "CMPD",	EXTENDED },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "CMPY",	EXTENDED },
	{ "*",		ILLEGAL },
	{ "LDY",	EXTENDED },
	{ "STY",	EXTENDED },
	/* 0x10C0 - 0x10CF */
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "LDS",	WORD_IMMEDIATE },
	{ "*",		ILLEGAL },
	/* 0x10D0 - 0x10DF */
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "LDS",	DIRECT },
	{ "STS",	DIRECT },
	/* 0x10E0 - 0x10EF */
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "LDS",	INDEXED },
	{ "STS",	INDEXED },
	/* 0x10F0 - 0x10FF */
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "LDS",	EXTENDED },
	{ "STS",	EXTENDED }
}, {
	/* 0x1100 - 0x110F */
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	/* 0x1110 - 0x111F */
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	/* 0x1120 - 0x112F */
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	/* 0x1130 - 0x113F */
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "SWI3",	INHERENT },
	/* 0x1140 - 0x114F */
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	/* 0x1150 - 0x115F */
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	/* 0x1160 - 0x116F */
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	/* 0x1170 - 0x117F */
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	/* 0x1180 - 0x118F */
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "CMPU",	WORD_IMMEDIATE },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "CMPS",	WORD_IMMEDIATE },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	/* 0x1190 - 0x119F */
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "CMPU",	DIRECT },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "CMPS",	DIRECT },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	/* 0x11A0 - 0x11AF */
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "CMPU",	INDEXED },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "CMPS",	INDEXED },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	/* 0x11B0 - 0x11BF */
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "CMPU",	EXTENDED },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "CMPS",	EXTENDED },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	/* 0x11C0 - 0x11CF */
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	/* 0x11D0 - 0x11DF */
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	/* 0x11E0 - 0x11EF */
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	/* 0x11F0 - 0x11FF */
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL },
	{ "*",		ILLEGAL }
} };

enum {
	WANT_INSTRUCTION, WANT_BYTE, WANT_WORD1, WANT_WORD2
} state_enum;

static const char *tfr_regs[16] = {
	"D", "X", "Y", "U", "S", "PC", "*", "*",
	"A", "B", "CC", "DP", "*", "*", "*", "*"
};
static const char *indexed_regs[4] = { "X", "Y", "U", "S" };

static int state, page, type;
static unsigned int ins_val, byte_val, word_val, index_mode;

void m6809_dasm_reset(void) {
	state = WANT_INSTRUCTION;
	page = PAGE0;
	type = 0;
	ins_val = byte_val = word_val = 0;
}

#define STACK_PRINT(r) do { \
		if (not_first) { LOG_DEBUG(0, "," r); } \
		else { LOG_DEBUG(0, r); not_first = 1; } \
	} while (0)

#define sex5(v) ((int)(((v) & 0x0f) - ((v) & 0x10)))

void m6809_dasm_byte(unsigned int byte, unsigned int pc) {
	int display = 0;
	const char *mnemonic;
	byte &= 0xff;
	pc &= 0xffff;
	switch (state) {
		default:
		case WANT_INSTRUCTION:
			ins_val = byte;
			type = instructions[page][ins_val].type;
			switch (type) {
				case PAGE2: case PAGE3:
					page = type;
					break;
				default: case ILLEGAL: case INHERENT:
					display = 1;
					break;
				case IMMEDIATE: case DIRECT: case RELATIVE:
				case STACKS: case STACKU: case REGISTER:
				case INDEXED:
					state = WANT_BYTE;
					break;
				case WORD_IMMEDIATE: case EXTENDED:
				case LONG_RELATIVE:
					state = WANT_WORD1;
					break;
			}
			break;
		case WANT_BYTE:
			if (type == INDEXED && index_mode == 0) {
				index_mode = byte;
				if ((index_mode & 0x80) == 0) {
					display = 1;
					break;
				}
				switch (index_mode & 0x0f) {
					case 0: case 1: case 2: case 3:
					case 4: case 5: case 6: case 11:
						display = 1;
						break;
					case 8: case 12:
						state = WANT_BYTE;
						break;
					case 9: case 13: case 15:
						state = WANT_WORD1;
						break;
					default:
						display = 1; 
						break;
				}
			} else {
				byte_val = byte;
				display = 1;
			}
			break;
		case WANT_WORD1:
			word_val = byte;
			state = WANT_WORD2;
			break;
		case WANT_WORD2:
			word_val = (word_val << 8) | byte;
			display = 1;
			break;
	}
	if (!display)
	 	return;
	mnemonic = instructions[page][ins_val].mnemonic;
	switch (type) {
		case ILLEGAL: case INHERENT:
			LOG_DEBUG(0, "%-16s", mnemonic);
			break;
		case IMMEDIATE:
			LOG_DEBUG(0, "%-8s#$%02x", mnemonic, byte_val);
			break;
		case DIRECT:
			LOG_DEBUG(0, "%-8s<$%02x", mnemonic, byte_val);
			break;
		case WORD_IMMEDIATE:
			LOG_DEBUG(0, "%-8s#$%04x", mnemonic, word_val);
			break;
		case EXTENDED:
			LOG_DEBUG(0, "%-8s$%04x", mnemonic, word_val);
			break;
		case STACKS: {
			int not_first = 0;
			LOG_DEBUG(0, "%-8s", mnemonic);
			if (byte_val & 0x01) STACK_PRINT("CC");
			if (byte_val & 0x02) STACK_PRINT("A");
			if (byte_val & 0x04) STACK_PRINT("B");
			if (byte_val & 0x08) STACK_PRINT("DP");
			if (byte_val & 0x10) STACK_PRINT("X");
			if (byte_val & 0x20) STACK_PRINT("Y");
			if (byte_val & 0x40) STACK_PRINT("U");
			if (byte_val & 0x80) STACK_PRINT("PC");
		} break;
		case STACKU: {
			int not_first = 0;
			LOG_DEBUG(0, "%-8s", mnemonic);
			if (byte_val & 0x01) STACK_PRINT("CC");
			if (byte_val & 0x02) STACK_PRINT("A");
			if (byte_val & 0x04) STACK_PRINT("B");
			if (byte_val & 0x08) STACK_PRINT("DP");
			if (byte_val & 0x10) STACK_PRINT("X");
			if (byte_val & 0x20) STACK_PRINT("Y");
			if (byte_val & 0x40) STACK_PRINT("S");
			if (byte_val & 0x80) STACK_PRINT("PC");
		} break;
		case REGISTER:
			LOG_DEBUG(0, "%-8s%s,%s", mnemonic,
					tfr_regs[(byte_val&0xf0)>>4],
					tfr_regs[byte_val&0x0f]);
			break;
		case INDEXED: {
			const char *reg = indexed_regs[(index_mode>>5)&3];
			if ((index_mode & 0x80) == 0) {
				LOG_DEBUG(0, "%-8s%d,%s", mnemonic, sex5(index_mode & 0x1f), reg);
				break;
			}
			if ((index_mode & 0x10) == 0) {

				switch (index_mode & 0x0f) {
					case 0:
						LOG_DEBUG(0, "%-8s,%s+", mnemonic, reg);
						break;
					case 1:
						LOG_DEBUG(0, "%-8s,%s++", mnemonic, reg);
						break;
					case 2:
						LOG_DEBUG(0, "%-8s,-%s", mnemonic, reg);
						break;
					case 3:
						LOG_DEBUG(0, "%-8s,--%s", mnemonic, reg);
						break;
					case 4:
						LOG_DEBUG(0, "%-8s,%s", mnemonic, reg);
						break;
					case 5:
						LOG_DEBUG(0, "%-8sB,%s", mnemonic, reg);
						break;
					case 6:
						LOG_DEBUG(0, "%-8sA,%s", mnemonic, reg);
						break;
					case 8:
						LOG_DEBUG(0, "%-8s$%02x,%s", mnemonic, byte_val, reg);
						break;
					case 9:
						LOG_DEBUG(0, "%-8s$%04x,%s", mnemonic, word_val, reg);
						break;
					case 11:
						LOG_DEBUG(0, "%-8sD,%s", mnemonic, reg);
						break;
					case 12:
						LOG_DEBUG(0, "%-8s$%02x,PCR", mnemonic, byte_val);
						break;
					case 13:
						LOG_DEBUG(0, "%-8s$%04x,PCR", mnemonic, word_val);
						break;
					case 15:
						LOG_DEBUG(0, "%-8s$%04x *", mnemonic, word_val);
						break;
					default:
						LOG_DEBUG(0, "%-8s$%02x *", mnemonic, index_mode);
						break;
				}

			} else {

				switch (index_mode & 0x0f) {
					case 0:
						LOG_DEBUG(0, "%-8s[,%s+ *]", mnemonic, reg);
						break;
					case 1:
						LOG_DEBUG(0, "%-8s[,%s++]", mnemonic, reg);
						break;
					case 2:
						LOG_DEBUG(0, "%-8s[,-%s *]", mnemonic, reg);
						break;
					case 3:
						LOG_DEBUG(0, "%-8s[,--%s]", mnemonic, reg);
						break;
					case 4:
						LOG_DEBUG(0, "%-8s[,%s]", mnemonic, reg);
						break;
					case 5:
						LOG_DEBUG(0, "%-8s[B,%s]", mnemonic, reg);
						break;
					case 6:
						LOG_DEBUG(0, "%-8s[A,%s]", mnemonic, reg);
						break;
					case 8:
						LOG_DEBUG(0, "%-8s[$%02x,%s]", mnemonic, byte_val, reg);
						break;
					case 9:
						LOG_DEBUG(0, "%-8s[$%04x,%s]", mnemonic, word_val, reg);
						break;
					case 11:
						LOG_DEBUG(0, "%-8s[D,%s]", mnemonic, reg);
						break;
					case 12:
						LOG_DEBUG(0, "%-8s[$%02x,PCR]", mnemonic, byte_val);
						break;
					case 13:
						LOG_DEBUG(0, "%-8s[$%04x,PCR]", mnemonic, word_val);
						break;
					case 15:
						LOG_DEBUG(0, "%-8s[$%04x]", mnemonic, word_val);
						break;
					default:
						LOG_DEBUG(0, "%-8sillegal: $%02x *", mnemonic, index_mode);
						break;
				}

			}
			} break;
		case RELATIVE:
			pc += (byte_val & 0x80) ? 0xff00 : 0;
			pc += 1 + byte_val;
			pc &= 0xffff;
			LOG_DEBUG(0, "%-8s$%04x", mnemonic, pc);
			break;
		case LONG_RELATIVE:
			pc += 1 + word_val;
			pc &= 0xffff;
			LOG_DEBUG(0, "%-8s$%04x", mnemonic, pc);
			break;
		default:
			LOG_DEBUG(0, "%-8s", mnemonic);
			break;
	}
	index_mode = 0;
	state = WANT_INSTRUCTION;
	page = PAGE0;
}

#endif /* TRACE */
