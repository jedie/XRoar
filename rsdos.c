/*  Copyright 2003-2013 Ciaran Anscomb
 *
 *  This file is part of XRoar.
 *
 *  XRoar is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  XRoar is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XRoar.  If not, see <http://www.gnu.org/licenses/>.
 */

/* Sources:
 *     CoCo DOS cartridge detail:
 *         http://www.coco3.com/unravalled/disk-basic-unravelled.pdf
 */

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "becker.h"
#include "cart.h"
#include "logging.h"
#include "machine.h"
#include "mc6809.h"
#include "rsdos.h"
#include "vdrive.h"
#include "wd279x.h"
#include "xroar.h"

static uint8_t io_read(uint16_t A);
static void io_write(uint16_t A, uint8_t D);
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
static _Bool ic1_density;
static _Bool drq_flag;
static _Bool intrq_flag;
static _Bool halt_enable;

/* Optional becker port */
static _Bool have_becker = 0;

static WD279X *fdc;

static void ff40_write(int octet);

void rsdos_configure(struct cart *c, struct cart_config *cc) {
	have_becker = (cc->becker_port && becker_open());
	c->io_read = io_read;
	c->io_write = io_write;
	c->reset = reset;
	c->detach = detach;
	fdc = wd279x_new(WD2793);
	fdc->set_drq_handler     = set_drq_handler;
	fdc->reset_drq_handler   = reset_drq_handler;
	fdc->set_intrq_handler   = set_intrq_handler;
	fdc->reset_intrq_handler = reset_intrq_handler;
}

static void reset(void) {
	wd279x_reset(fdc);
	ic1_old = -1;
	ic1_drive_select = -1;
	drq_flag = intrq_flag = 0;
	ff40_write(0);
}

static void detach(void) {
	wd279x_free(fdc);
	fdc = NULL;
	if (have_becker)
		becker_close();
}

static uint8_t io_read(uint16_t A) {
	if (A & 0x8)
		return wd279x_read(fdc, A);
	if (have_becker) {
		switch (A & 3) {
		case 0x1:
			return becker_read_status();
		case 0x2:
			return becker_read_data();
		default:
			break;
		}
	}
	return 0x7e;
}

static void io_write(uint16_t A, uint8_t D) {
	if (A & 0x8) {
		wd279x_write(fdc, A, D);
		return;
	}
	if (have_becker) {
		/* XXX not exactly sure in what way anyone has tightened up the
		 * address decoding for the becker port */
		switch (A & 3) {
		case 0x0:
			ff40_write(D);
			break;
		case 0x2:
			becker_write_data(D);
			break;
		default:
			break;
		}
	} else {
		if (!(A & 8))
			ff40_write(D);
	}
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
	wd279x_set_dden(fdc, !ic1_density);
	if (ic1_density && intrq_flag) {
		MC6809_NMI_SET(CPU0, 1);
	}
	halt_enable = octet & 0x80;
	if (intrq_flag) halt_enable = 0;
	if (halt_enable && !drq_flag) {
		MC6809_HALT_SET(CPU0, 1);
	} else {
		MC6809_HALT_SET(CPU0, 0);
	}
}

static void set_drq_handler(void) {
	drq_flag = 1;
	MC6809_HALT_SET(CPU0, 0);
}

static void reset_drq_handler(void) {
	drq_flag = 0;
	if (halt_enable) {
		MC6809_HALT_SET(CPU0, 1);
	}
}

static void set_intrq_handler(void) {
	intrq_flag = 1;
	halt_enable = 0;
	MC6809_HALT_SET(CPU0, 0);
	if (!ic1_density && intrq_flag) {
		MC6809_NMI_SET(CPU0, 1);
	}
}

static void reset_intrq_handler(void) {
	intrq_flag = 0;
	MC6809_NMI_SET(CPU0, 0);
}
