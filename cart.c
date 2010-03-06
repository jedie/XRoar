/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2010  Ciaran Anscomb
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

#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "cart.h"
#include "events.h"
#include "machine.h"
#include "mc6821.h"
#include "xroar.h"

static void detach(void);
static struct cart cart;

static event_t *firq_event;
static void do_firq(void);

struct cart *cart_rom_new(const char *filename, int autorun) {
	cart.type = CART_ROM;
	memset(cart.mem_data, 0x7e, sizeof(cart.mem_data));
	machine_load_rom(filename, cart.mem_data, sizeof(cart.mem_data));
	cart.mem_writable = 0;
	cart.io_read = NULL;
	cart.io_write = NULL;
	cart.reset = NULL;
	cart.detach = detach;
	if (autorun) {
		firq_event = event_new();
		firq_event->dispatch = do_firq;
		firq_event->at_cycle = current_cycle + (OSCILLATOR_RATE/10);
		event_queue(&MACHINE_EVENT_LIST, firq_event);
	}
	return &cart;
}

struct cart *cart_ram_new(void) {
	cart.type = CART_RAM;
	memset(cart.mem_data, 0, sizeof(cart.mem_data));
	cart.mem_writable = 1;
	cart.io_read = NULL;
	cart.io_write = NULL;
	cart.reset = NULL;
	cart.detach = detach;
	return &cart;
}

static void detach(void) {
	if (firq_event) {
		event_free(firq_event);
		firq_event = NULL;
	}
}

static void do_firq(void) {
	PIA_SET_Cx1(PIA1.b);
	firq_event->at_cycle = current_cycle + (OSCILLATOR_RATE/10);
	event_queue(&MACHINE_EVENT_LIST, firq_event);
}
