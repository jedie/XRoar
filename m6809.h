/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2005  Ciaran Anscomb
 *
 *  See COPYING for redistribution conditions. */

#ifndef __M6809_H__
#define __M6809_H__

#include "machine.h"
#include "types.h"

#define reg_a reg_accum.byte_values.upper
#define reg_b reg_accum.byte_values.lower
#define reg_d reg_accum.word_value

typedef union {
	//uint32_t word_value;
	uint16_t word_value;
	struct {
#ifdef WRONG_ENDIAN
		uint8_t lower;
		uint8_t upper;
		//uint16_t dummy;
#else
		//uint16_t dummy;
		uint8_t upper;
		uint8_t lower;
#endif
	} byte_values;
} accumulator_t;

extern uint_fast8_t nmi, firq, irq;

void m6809_init(void);
void m6809_reset(void);
void m6809_cycle(Cycle until);
void m6809_get_registers(uint8_t *regs);
void m6809_set_registers(uint8_t *regs);
void m6809_jump(uint_fast16_t pc);

#endif  /* __M6809_H__ */
