/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2009  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef __M6809_H__
#define __M6809_H__

#include "types.h"

#define M6809_COMPAT_STATE_NORMAL (0)
#define M6809_COMPAT_STATE_SYNC   (1)
#define M6809_COMPAT_STATE_CWAI   (2)

typedef struct {
	uint8_t reg_cc, reg_a, reg_b, reg_dp;
	uint16_t reg_x, reg_y, reg_u, reg_s, reg_pc;
	uint8_t halt, nmi, firq, irq;
	uint8_t cpu_state;
	uint8_t nmi_armed;
} M6809State;

extern unsigned int halt, nmi, firq, irq;

void m6809_init(void);
void m6809_reset(void);
void m6809_run(int cycles);
void m6809_get_state(M6809State *state);
void m6809_set_state(M6809State *state);
void m6809_jump(unsigned int pc);

/*** Private ***/

/* Returns result of a byte read cycle */
extern unsigned int (*m6809_read_cycle)(unsigned int addr);

/* Performs a byte write cycle */
extern void (*m6809_write_cycle)(unsigned int addr, unsigned int value);

/* Non valid memory access ("busy") cycles */
extern void (*m6809_nvma_cycles)(int cycles);

/* Ensure all outside events are complete up to current cycle */
extern void (*m6809_sync)(void);

#endif  /* __M6809_H__ */
