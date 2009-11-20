/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2009  Ciaran Anscomb
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
#include "events.h"
#include "fs.h"
#include "logging.h"
#include "machine.h"
#include "mc6821.h"
#include "xroar.h"
#include "cart.h"

uint8_t cart_data[0x4000];
int cart_data_writable;

static event_t cart_event;
static char *cart_filename;
static int cart_autostart;

static void cart_configure(const char *filename, int autostart);
static int cart_load(void);
static void cart_interrupt(void);

void cart_init(void) {
	event_init(&cart_event);
	cart_event.dispatch = cart_interrupt;
	memset(cart_data, 0x7e, sizeof(cart_data));
	cart_data_writable = 0;
}

void cart_reset(void) {
	cart_load();
}

int cart_insert(const char *filename, int autostart) {
	cart_configure(filename, autostart);
	return cart_load();
}

void cart_remove(void) {
	if (cart_filename) {
		free(cart_filename);
		cart_filename = NULL;
	}
}

static void cart_configure(const char *filename, int autostart) {
	if (cart_filename)
		cart_remove();
	cart_filename = malloc(strlen(filename)+1);
	if (cart_filename == NULL)
		return;
	strcpy(cart_filename, filename);
	cart_autostart = autostart;
}

static int cart_load(void) {
	int fd;
	if (cart_filename == NULL)
		return -1;
	if ((fd = fs_open(cart_filename, FS_READ)) == -1) {
		cart_remove();
		return -1;
	}
	LOG_DEBUG(3, "Loading cartridge: %s\n", cart_filename);
	fs_read(fd, cart_data, sizeof(cart_data));
	fs_close(fd);
	if (cart_autostart) {
		cart_event.at_cycle = current_cycle + (OSCILLATOR_RATE/10);
		event_queue(&MACHINE_EVENT_LIST, &cart_event);
	} else {
		event_dequeue(&cart_event);
	}
	cart_data_writable = 0;
	return 0;
}

static void cart_interrupt(void) {
	if (cart_filename) {
		PIA_SET_Cx1(PIA1.b);
		cart_event.at_cycle = current_cycle + (OSCILLATOR_RATE/10);
		event_queue(&MACHINE_EVENT_LIST, &cart_event);
	}
}
