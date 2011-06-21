/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2011  Ciaran Anscomb
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdlib.h>
#include <string.h>

#include "mc6821.h"
#include "misc.h"

MC6821_PIA *mc6821_new(void) {
	MC6821_PIA *new = xmalloc(sizeof(MC6821_PIA));
	mc6821_init(new);
	return new;
}

void mc6821_init(MC6821_PIA *pia) {
	memset(pia, 0, sizeof(MC6821_PIA));
	pia->a.tied_low = 0xff;
	pia->b.tied_low = 0xff;
	pia->a.port_input = 0xff;
}

void mc6821_destroy(MC6821_PIA *pia) {
	if (pia == NULL) return;
	free(pia);
}

#define INTERRUPT_ENABLED(p) (p.control_register & 0x01)
#define ACTIVE_TRANSITION(p) (p.control_register & 0x02)
#define DDR_SELECTED(p)      (!(p.control_register & 0x04))
#define PDR_SELECTED(p)      (p.control_register & 0x04)

void mc6821_reset(MC6821_PIA *pia) {
	if (pia == NULL) return;
	pia->a.control_register = 0;
	pia->a.direction_register = 0;
	pia->a.output_register = 0;
	pia->a.interrupt_received = 0;
	pia->a.irq = 0;
	pia->b.control_register = 0;
	pia->b.direction_register = 0;
	pia->b.output_register = 0;
	pia->b.interrupt_received = 0;
	pia->b.irq = 0;
	mc6821_update_state(pia);
}

#define UPDATE_OUTPUT(p) do { \
		p.port_output = ((p.output_register & p.direction_register) | (p.port_input & ~(p.direction_register))) & p.tied_low; \
		if (p.data_postwrite) p.data_postwrite(); \
	} while (0)

void mc6821_update_state(MC6821_PIA *pia) {
	UPDATE_OUTPUT(pia->a);
	UPDATE_OUTPUT(pia->b);
}

#define READ_DR(p) do { \
		if (PDR_SELECTED(p)) { \
			if (p.data_preread) p.data_preread(); \
			p.interrupt_received = 0; \
			p.irq = 0; \
			return ((p.port_input & p.tied_low) & ~p.direction_register) | (p.output_register & p.direction_register); \
		} else { \
			return p.direction_register; \
		} \
	} while (0)

#define READ_CR(p) do { \
		if (p.control_preread) p.control_preread(); \
		return (p.control_register | p.interrupt_received); \
	} while (0)

unsigned int mc6821_read(MC6821_PIA *pia, unsigned int addr) {
	switch (addr & 3) {
		default:
		case 0:
			READ_DR(pia->a);
			break;
		case 1:
			READ_CR(pia->a);
			break;
		case 2:
			READ_DR(pia->b);
			break;
		case 3:
			READ_CR(pia->b);
			break;
	}
}

#define WRITE_DR(p,v) do { \
		if (PDR_SELECTED(p)) { \
			p.output_register = v; \
			v &= p.direction_register; \
		} else { \
			p.direction_register = v; \
			v &= p.output_register; \
		} \
		p.port_output = (v | (p.port_input & ~(p.direction_register))) & p.tied_low; \
		if (p.data_postwrite) p.data_postwrite(); \
	} while (0)

#define WRITE_CR(p,v) do { \
		p.control_register = v & 0x3f; \
		if (INTERRUPT_ENABLED(p)) { \
			if (p.interrupt_received) \
				p.irq = 1; \
		} else { \
			p.irq = 0; \
		} \
		if (p.control_postwrite) p.control_postwrite(); \
	} while (0)

void mc6821_write(MC6821_PIA *pia, unsigned int addr, unsigned int val) {
	switch (addr & 3) {
		default:
		case 0:
			WRITE_DR(pia->a, val);
			break;
		case 1:
			WRITE_CR(pia->a, val);
			break;
		case 2:
			WRITE_DR(pia->b, val);
			break;
		case 3:
			WRITE_CR(pia->b, val);
			break;
	}
}
