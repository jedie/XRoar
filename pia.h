/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2005  Ciaran Anscomb
 *
 *  See COPYING for redistribution conditions. */

#ifndef __PIA_H__
#define __PIA_H__

#include "keyboard.h"
#include "joystick.h"
#include "sound.h"
#include "vdg.h"
#include "types.h"
#include "m6809.h"

#define MAX_WRFUNCS 4

typedef struct {
	unsigned int control_register;
	unsigned int direction_register;
	unsigned int output_register;
	unsigned int port_output;
	unsigned int port_input;
	unsigned int tied_low;
	/* Convenience flags split out from control_register above */
	unsigned int interrupt_enable;
	unsigned int interrupt_transition;
	unsigned int register_select;  /* 0 = DDR, SET = OR */
	/* ignore Cx2 stuff for now */
} pia_port;

extern pia_port PIA_0A, PIA_0B, PIA_1A, PIA_1B;

#define PIA_SET_P0CA1 do { PIA_0A.control_register |= 0x80; if (PIA_0A.interrupt_enable) irq = 1; } while (0)
#define PIA_SET_P0CB1 do { PIA_0B.control_register |= 0x80; if (PIA_0B.interrupt_enable) irq = 1; } while (0)
#define PIA_SET_P1CA1 do { PIA_1A.control_register |= 0x80; if (PIA_1A.interrupt_enable) firq = 1; } while (0)
#define PIA_SET_P1CB1 do { PIA_1B.control_register |= 0x80; if (PIA_1B.interrupt_enable) firq = 1; } while (0)

#define PIA_CONTROL_READ(p) (p.control_register)

#define PIA_CONTROL_WRITE(p,v) do { \
		p.control_register = v; \
		p.interrupt_enable = v & 0x01; \
		p.interrupt_transition = v & 0x02; \
		p.register_select = v & 0x04; \
	} while (0)

#define PIA_READ(p) (p.register_select ? \
	(p.control_register &= 0x3f, ((p.port_input & p.tied_low) & ~p.direction_register) | (p.output_register & p.direction_register)) \
	: p.direction_register)

#define PIA_WRITE(p,v) do { \
		if (!p.register_select) { \
			p.direction_register = v; \
			v &= p.output_register; \
		} else { \
			p.output_register = v; \
			v &= p.direction_register; \
		} \
		p.port_output = (v | (p.port_input & ~(p.direction_register))) & p.tied_low; \
	} while (0)

#define PIA_UPDATE_OUTPUT(p) do { \
		p.port_output = ((p.output_register & p.direction_register) | (p.port_input & ~(p.direction_register))) & p.tied_low; \
	} while (0)

#define PIA_WRITE_P0DA(v) do { PIA_WRITE(PIA_0A, v); keyboard_row_update(); } while (0)
#define PIA_WRITE_P0DB(v) do { PIA_WRITE(PIA_0B, v); keyboard_column_update(); } while (0)
#define PIA_WRITE_P1DA(v) do { PIA_WRITE(PIA_1A, v); sound_module->update(); joystick_update(); } while (0)
#define PIA_WRITE_P1DB(v) do { PIA_WRITE(PIA_1B, v); sound_module->update(); vdg_set_mode(); } while (0)

void pia_init(void);
void pia_reset(void);

#endif  /* __PIA_H__ */
