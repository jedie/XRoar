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
 * Code to set/reset Cx1 is in the form of macros for speed, as these generate
 * interrupts and are likely to happen frequently.
 *
 * Not implemented: Cx2/IRQx2 behaviour.
 */

typedef struct {
	struct {
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
	} a, b;
} MC6821_PIA;

#define PIA_INTERRUPT_ENABLED(p) (p.control_register & 0x01)
#define PIA_ACTIVE_TRANSITION(p) (p.control_register & 0x02)
#define PIA_DDR_SELECTED(p)      (!(p.control_register & 0x04))
#define PIA_PDR_SELECTED(p)      (p.control_register & 0x04)

#define PIA_SET_Cx1(p) do { \
		if (PIA_ACTIVE_TRANSITION(p)) { \
			p.interrupt_received = 0x80; \
			if (PIA_INTERRUPT_ENABLED(p)) { \
				p.irq = 1; \
			} else { \
				p.irq = 0; \
			} \
		} \
	} while (0)

#define PIA_RESET_Cx1(p) do { \
		if (!PIA_ACTIVE_TRANSITION(p)) { \
			p.interrupt_received = 0x80; \
			if (PIA_INTERRUPT_ENABLED(p)) { \
				p.irq = 1; \
			} else { \
				p.irq = 0; \
			} \
		} \
	} while (0)

MC6821_PIA *mc6821_new(void);
void mc6821_init(MC6821_PIA *pia);
void mc6821_destroy(MC6821_PIA *pia);

void mc6821_reset(MC6821_PIA *pia);
void mc6821_update_state(MC6821_PIA *pia);
uint8_t mc6821_read(MC6821_PIA *pia, unsigned int addr);
void mc6821_write(MC6821_PIA *pia, unsigned int addr, uint8_t val);

#endif  /* XROAR_MC6821_H_ */
