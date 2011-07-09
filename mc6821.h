/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2011  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_MC6821_H_
#define XROAR_MC6821_H_

#include "types.h"

/* 
 * Two "sides" per PIA (A & B), each basically the same except side A
 * initialised with "internal pull-up resistors" (effectively port_input all
 * high).
 *
 * Pointers to pre-read and post-write functions can be set for data & control
 * registers.
 *
 * Not implemented: Cx2/IRQx2 behaviour.
 */

struct MC6821_PIA_side {
	uint8_t control_register;
	uint8_t direction_register;
	uint8_t output_register;
	uint8_t port_output;
	uint8_t port_input;
	uint8_t tied_low;
	int interrupt_received;
	int irq;
	void (*control_preread)(void);
	void (*control_postwrite)(void);
	void (*data_preread)(void);
	void (*data_postwrite)(void);
};

typedef struct {
	struct MC6821_PIA_side a, b;
} MC6821_PIA;

#define PIA_SET_Cx1(s) mc6821_set_cx1(&s)
#define PIA_RESET_Cx1(s) mc6821_reset_cx1(&s)

MC6821_PIA *mc6821_new(void);
void mc6821_init(MC6821_PIA *pia);
void mc6821_destroy(MC6821_PIA *pia);

void mc6821_reset(MC6821_PIA *pia);
void mc6821_set_cx1(struct MC6821_PIA_side *side);
void mc6821_reset_cx1(struct MC6821_PIA_side *side);
void mc6821_update_state(MC6821_PIA *pia);
uint8_t mc6821_read(MC6821_PIA *pia, unsigned int addr);
void mc6821_write(MC6821_PIA *pia, unsigned int addr, uint8_t val);

#endif  /* XROAR_MC6821_H_ */
