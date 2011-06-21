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

/* Sources:
 *     Delta cartridge detail:
 *         Partly inferred from disassembly of Delta ROM,
 *         Partly from information provided by Phill Harvey-Smith on
 *         www.dragon-archive.co.uk.
 */

#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "cart.h"
#include "deltados.h"
#include "logging.h"
#include "machine.h"
#include "vdrive.h"
#include "wd279x.h"
#include "xroar.h"

static int io_read(int addr);
static void io_write(int addr, int val);
static void reset(void);

/* Latch that's part of the Delta cart: */
static int ic1_old;
static int ic1_drive_select;
static int ic1_side_select;
static int ic1_density;

static void ff44_write(int octet);

void deltados_configure(struct cart *c, struct cart_config *cc) {
	(void)cc;
	c->io_read = io_read;
	c->io_write = io_write;
	c->reset = reset;
	c->detach = NULL;
	wd279x_type = WD2791;
	wd279x_set_drq_handler     = NULL;
	wd279x_reset_drq_handler   = NULL;
	wd279x_set_intrq_handler   = NULL;
	wd279x_reset_intrq_handler = NULL;
}

static void reset(void) {
	wd279x_reset();
	ic1_old = 0xff;
	ic1_drive_select = 0xff;
	ic1_side_select  = 0xff;
	ic1_density      = 0xff;
	ff44_write(0);
}

static int io_read(int addr) {
	if ((addr & 7) == 0) return wd279x_status_read();
	if ((addr & 7) == 1) return wd279x_track_register_read();
	if ((addr & 7) == 2) return wd279x_sector_register_read();
	if ((addr & 7) == 3) return wd279x_data_register_read();
	return 0x7e;
}

static void io_write(int addr, int val) {
	if ((addr & 7) == 0) wd279x_command_write(val);
	if ((addr & 7) == 1) wd279x_track_register_write(val);
	if ((addr & 7) == 2) wd279x_sector_register_write(val);
	if ((addr & 7) == 3) wd279x_data_register_write(val);
	if (addr & 4) ff44_write(val);
}

/* Delta cartridge circuitry */
static void ff44_write(int octet) {
	if (octet != ic1_old) {
		LOG_DEBUG(4, "Delta: Write to FF44: ");
		if ((octet ^ ic1_old) & 0x03) {
			LOG_DEBUG(4, "DRIVE SELECT %01d, ", octet & 0x03);
		}
		if ((octet ^ ic1_old) & 0x04) {
			LOG_DEBUG(4, "SIDE %s, ", (octet & 0x04)?"1":"0");
		}
		if ((octet ^ ic1_old) & 0x08) {
			LOG_DEBUG(4, "DENSITY %s, ", (octet & 0x08)?"DOUBLE":"SINGLE");
		}
		LOG_DEBUG(4, "\n");
		ic1_old = octet;
	}
	ic1_drive_select = octet & 0x03;
	vdrive_set_drive(ic1_drive_select);
	ic1_side_select = octet & 0x04;
	vdrive_set_side(ic1_side_select ? 1 : 0);
	ic1_density = !(octet & 0x08);
	wd279x_set_density(ic1_density);
}
