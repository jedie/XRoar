/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2007  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef __MC6821_H__
#define __MC6821_H__

/* 
 * Two "sides" per PIA (A & B), each basically the same except side A
 * initialised with "internal pull-up resistors" (effectively port_input all
 * high).
 *
 * The bits specified in irq_or are set/reset in *irq_line when interrupts are
 * set or cleared - both sides may be pointed at the same unsigned int as the
 * default for irq_or is different per-side.
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
		unsigned int control_register;
		unsigned int direction_register;
		unsigned int output_register;
		unsigned int port_output;
		unsigned int port_input;
		unsigned int tied_low;
		unsigned int interrupt_received;
		unsigned int *irq_line;
		unsigned int irq_or;
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
				if (p.irq_line) *(p.irq_line) |= (p.irq_or); \
			} else { \
				if (p.irq_line) *(p.irq_line) &= ~(p.irq_or); \
			} \
		} \
	} while (0)

#define PIA_RESET_Cx1(p) do { \
		if (!PIA_ACTIVE_TRANSITION(p)) { \
			p.interrupt_received = 0x80; \
			if (PIA_INTERRUPT_ENABLED(p)) { \
				if (p.irq_line) *(p.irq_line) |= (p.irq_or); \
			} else { \
				if (p.irq_line) *(p.irq_line) &= ~(p.irq_or); \
			} \
		} \
	} while (0)

MC6821_PIA *mc6821_new(void);
void mc6821_init(MC6821_PIA *pia);
void mc6821_destroy(MC6821_PIA *pia);

void mc6821_reset(MC6821_PIA *pia);
void mc6821_update_state(MC6821_PIA *pia);
unsigned int mc6821_read(MC6821_PIA *pia, unsigned int addr);
void mc6821_write(MC6821_PIA *pia, unsigned int addr, unsigned int val);

#endif  /* __MC6821_H__ */
