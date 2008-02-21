/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2007  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef __M6809_H__
#define __M6809_H__

#include "types.h"
#include "sam.h"
#include "machine.h"

typedef struct {
	uint8_t reg_cc, reg_a, reg_b, reg_dp;
	uint16_t reg_x, reg_y, reg_u, reg_s, reg_pc;
	uint8_t halt, nmi, firq, irq;
	uint8_t halted;
	uint8_t wait_for_interrupt;
	uint8_t skip_register_push;
	uint8_t nmi_armed;
} M6809State;

extern int halt, nmi, firq, irq;

void m6809_init(void);
void m6809_reset(void);
void m6809_cycle(Cycle until);
void m6809_get_state(M6809State *state);
void m6809_set_state(M6809State *state);
void m6809_jump(unsigned int pc);

/*** Old ***/

void m6809_set_registers(uint8_t *regs);

/*** Private ***/

/* Current cycle */
#define m6809_current_cycle current_cycle

/* Returns result of a byte read cycle */
#define m6809_read_cycle(a) sam_read_byte(a)

/* For where the CPU will discard the result of a read */
#define m6809_discard_read_cycle(a) sam_peek_byte(a)

/* Performs a byte write cycle */
#define m6809_write_cycle(a,v) do { sam_store_byte((a),(v)); } while (0)

/* Non valid memory access ("busy") cycles */
#define m6809_nvma_cycles(c) current_cycle += ((c) * sam_topaddr_cycles)

#endif  /* __M6809_H__ */
