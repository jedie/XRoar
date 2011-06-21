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
 *     CoCo DOS cartridge detail:
 *         http://www.coco3.com/unravalled/disk-basic-unravelled.pdf
 */

#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "cart.h"
#include "logging.h"
#include "m6809.h"
#include "machine.h"
#include "rsdos.h"
#include "vdrive.h"
#include "wd279x.h"
#include "xroar.h"

static int io_read(int addr);
static void io_write(int addr, int val);
static void reset(void);
static void detach(void);

/* Handle signals from WD2793 */
static void set_drq_handler(void);
static void reset_drq_handler(void);
static void set_intrq_handler(void);
static void reset_intrq_handler(void);

/* Latch that's part of the RSDOS cart: */
static int ic1_old;
static int ic1_drive_select;
static int ic1_density;
static int drq_flag;
static int intrq_flag;
static int halt_enable;

static void ff40_write(int octet);

void rsdos_configure(struct cart *c, struct cart_config *cc) {
	(void)cc;
	c->io_read = io_read;
	c->io_write = io_write;
	c->reset = reset;
	c->detach = detach;
	wd279x_type = WD2793;
	wd279x_set_drq_handler     = set_drq_handler;
	wd279x_reset_drq_handler   = reset_drq_handler;
	wd279x_set_intrq_handler   = set_intrq_handler;
	wd279x_reset_intrq_handler = reset_intrq_handler;
}

static void reset(void) {
	wd279x_reset();
	ic1_old = 0xff;
	ic1_drive_select = 0xff;
	ic1_density = 0xff;
	halt_enable = 0xff;
	drq_flag = intrq_flag = 0;
	ff40_write(0);
}

static void detach(void) {
}

static int io_read(int addr) {
	if ((addr & 15) == 8) return wd279x_status_read();
	if ((addr & 15) == 9) return wd279x_track_register_read();
	if ((addr & 15) == 10) return wd279x_sector_register_read();
	if ((addr & 15) == 11) return wd279x_data_register_read();
	return 0x7e;
}

static void io_write(int addr, int val) {
	if ((addr & 15) == 8) wd279x_command_write(val);
	if ((addr & 15) == 9) wd279x_track_register_write(val);
	if ((addr & 15) == 10) wd279x_sector_register_write(val);
	if ((addr & 15) == 11) wd279x_data_register_write(val);
	if (!(addr & 8)) ff40_write(val);
}

/* RSDOS cartridge circuitry */
static void ff40_write(int octet) {
	int new_drive_select = 0;
	octet ^= 0x20;
	if (octet & 0x01) {
		new_drive_select = 0;
	} else if (octet & 0x02) {
		new_drive_select = 1;
	} else if (octet & 0x04) {
		new_drive_select = 2;
	}
	vdrive_set_side(octet & 0x40 ? 1 : 0);
	if (octet != ic1_old) {
		LOG_DEBUG(4, "RSDOS: Write to FF40: ");
		if (new_drive_select != ic1_drive_select) {
			LOG_DEBUG(4, "DRIVE SELECT %d, ", new_drive_select);
		}
		if ((octet ^ ic1_old) & 0x08) {
			LOG_DEBUG(4, "MOTOR %s, ", (octet & 0x08)?"ON":"OFF");
		}
		if ((octet ^ ic1_old) & 0x20) {
			LOG_DEBUG(4, "DENSITY %s, ", (octet & 0x20)?"SINGLE":"DOUBLE");
		}
		if ((octet ^ ic1_old) & 0x10) {
			LOG_DEBUG(4, "PRECOMP %s, ", (octet & 0x10)?"ON":"OFF");
		}
		if ((octet ^ ic1_old) & 0x40) {
			LOG_DEBUG(4, "SIDE %d, ", (octet & 0x40) >> 6);
		}
		if ((octet ^ ic1_old) & 0x80) {
			LOG_DEBUG(4, "HALT %s, ", (octet & 0x80)?"ENABLED":"DISABLED");
		}
		LOG_DEBUG(4, "\n");
		ic1_old = octet;
	}
	ic1_drive_select = new_drive_select;
	vdrive_set_drive(ic1_drive_select);
	ic1_density = octet & 0x20;
	wd279x_set_density(ic1_density);
	if (ic1_density && intrq_flag) {
		m6809_nmi_set();
	}
	halt_enable = octet & 0x80;
	if (intrq_flag) halt_enable = 0;
	if (halt_enable && !drq_flag) {
		m6809_halt_set();
	} else {
		m6809_halt_clear();
	}
}

static void set_drq_handler(void) {
	drq_flag = 1;
	m6809_halt_clear();
}

static void reset_drq_handler(void) {
	drq_flag = 0;
	if (halt_enable) {
		m6809_halt_set();
	}
}

static void set_intrq_handler(void) {
	intrq_flag = 1;
	halt_enable = 0;
	m6809_halt_clear();
	if (!ic1_density && intrq_flag) {
		m6809_nmi_set();
	}
}

static void reset_intrq_handler(void) {
	intrq_flag = 0;
	m6809_nmi_clear();
}
