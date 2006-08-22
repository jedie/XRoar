/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2006  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef __PIA_H__
#define __PIA_H__

#include "types.h"
#include "joystick.h"
#include "keyboard.h"
#include "m6809.h"
#include "module.h"
#include "tape.h"
#include "vdg.h"

#ifdef HAVE_GP32
#define tape_update_input()
#endif

#define MAX_WRFUNCS 4

typedef struct {
	unsigned int control_register;
	unsigned int direction_register;
	unsigned int output_register;
	unsigned int port_output;
	unsigned int port_input;
	unsigned int tied_low;
	unsigned int interrupt_received;
} pia_port;

#define PIA_SELECT_DDR(p) do { p.control_register &= 0xfb; } while (0)
#define PIA_SELECT_PDR(p) do { p.control_register |= 0x04; } while (0)

#define PIA_INTERRUPT_ENABLED(p) (p.control_register & 0x01)
#define PIA_ACTIVE_TRANSITION(p) (p.control_register & 0x02)
#define PIA_DDR_SELECTED(p)      (!(p.control_register & 0x04))
#define PIA_PDR_SELECTED(p)      (p.control_register & 0x04)

extern pia_port PIA_0A, PIA_0B, PIA_1A, PIA_1B;

#define PIA_SET_Cx1(p,i,bv) do { \
		if (PIA_ACTIVE_TRANSITION(p)) { \
			p.interrupt_received = 0x80; \
			if (PIA_INTERRUPT_ENABLED(p)) { \
				i |= (bv); \
			} else { \
				i &= ~(bv); \
			} \
		} \
	} while (0)

#define PIA_SET_P0CA1 do { \
		PIA_SET_Cx1(PIA_0A, irq, 1); \
	} while (0)
#define PIA_SET_P0CB1 do { \
		PIA_SET_Cx1(PIA_0B, irq, 2); \
	} while (0)
#define PIA_SET_P1CA1 do { \
		PIA_SET_Cx1(PIA_1A, firq, 1); \
	} while (0)
#define PIA_SET_P1CB1 do { \
		PIA_SET_Cx1(PIA_1B, firq, 2); \
	} while (0)

#define PIA_RESET_Cx1(p,i,bv) do { \
		if (!PIA_ACTIVE_TRANSITION(p)) { \
			p.interrupt_received = 0x80; \
			if (PIA_INTERRUPT_ENABLED(p)) { \
				i |= (bv); \
			} else { \
				i &= ~(bv); \
			} \
		} \
	} while (0)

#define PIA_RESET_P0CA1 do { \
			PIA_RESET_Cx1(PIA_0A, irq, 1); \
		} while (0)
#define PIA_RESET_P0CB1 do { \
			PIA_RESET_Cx1(PIA_0B, irq, 2); \
		} while (0)
#define PIA_RESET_P1CA1 do { \
			PIA_RESET_Cx1(PIA_1A, firq, 1); \
		} while (0)
#define PIA_RESET_P1CB1 do { \
			PIA_RESET_Cx1(PIA_1B, firq, 2); \
		} while (0)

#define PIA_CONTROL_READ(p) (p.control_register | p.interrupt_received)

#define PIA_READ_P0CA PIA_CONTROL_READ(PIA_0A)
#define PIA_READ_P0CB PIA_CONTROL_READ(PIA_0B)
#define PIA_READ_P1CA PIA_CONTROL_READ(PIA_1A)
#define PIA_READ_P1CB PIA_CONTROL_READ(PIA_1B)

#define PIA_CONTROL_WRITE(p,v,i,bv) do { \
		p.control_register = v & 0x3f; \
		if (PIA_INTERRUPT_ENABLED(p)) { \
			if (p.interrupt_received) \
				i |= (bv); \
		} else { \
			i &= ~(bv); \
		} \
	} while (0)

#define PIA_WRITE_P0CA(v) PIA_CONTROL_WRITE(PIA_0A,v,irq,1)
#define PIA_WRITE_P0CB(v) PIA_CONTROL_WRITE(PIA_0B,v,irq,2)
#define PIA_WRITE_P1CA(v) do { PIA_CONTROL_WRITE(PIA_1A,v,firq,1); tape_update_motor(); } while (0)
#define PIA_WRITE_P1CB(v) PIA_CONTROL_WRITE(PIA_1B,v,firq,2)

#define PIA_READ(p,i,bv,r) do { \
		if (PIA_PDR_SELECTED(p)) { \
			p.interrupt_received = 0; \
			i &= ~(bv); \
			r = ((p.port_input & p.tied_low) & ~p.direction_register) | (p.output_register & p.direction_register); \
		} else { \
			r = p.direction_register; \
		} \
	} while (0)

#define PIA_READ_P0DA(r) PIA_READ(PIA_0A, irq, 1, r)
#define PIA_READ_P0DB(r) PIA_READ(PIA_0B, irq, 2, r)
#define PIA_READ_P1DA(r) do { tape_update_input(); PIA_READ(PIA_1A, firq, 1, r); } while (0)
#define PIA_READ_P1DB(r) PIA_READ(PIA_1B, firq, 2, r)

#define PIA_WRITE(p,v) do { \
		if (PIA_PDR_SELECTED(p)) { \
			p.output_register = v; \
			v &= p.direction_register; \
		} else { \
			p.direction_register = v; \
			v &= p.output_register; \
		} \
		p.port_output = (v | (p.port_input & ~(p.direction_register))) & p.tied_low; \
	} while (0)

#define PIA_UPDATE_OUTPUT(p) do { \
		p.port_output = ((p.output_register & p.direction_register) | (p.port_input & ~(p.direction_register))) & p.tied_low; \
	} while (0)

#define PIA_WRITE_P0DA(v) do { PIA_WRITE(PIA_0A, v); keyboard_row_update(); } while (0)
#define PIA_WRITE_P0DB(v) do { PIA_WRITE(PIA_0B, v); keyboard_column_update(); } while (0)
#define PIA_WRITE_P1DA(v) do { PIA_WRITE(PIA_1A, v); sound_module->update(); joystick_update(); tape_update_output(); } while (0)
#define PIA_WRITE_P1DB(v) do { PIA_WRITE(PIA_1B, v); sound_module->update(); vdg_set_mode(); } while (0)

void pia_init(void);
void pia_reset(void);

#endif  /* __PIA_H__ */
