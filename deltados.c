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
 *     Delta cartridge detail:
 *         Partly inferred from disassembly of Delta ROM,
 *         Partly from information provided by Phill Harvey-Smith on
 *         www.dragon-archive.co.uk.
 */

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "cart.h"
#include "deltados.h"
#include "logging.h"
#include "machine.h"
#include "vdrive.h"
#include "wd279x.h"
#include "xroar.h"

static uint8_t io_read(uint16_t A);
static void io_write(uint16_t A, uint8_t D);
static void reset(void);
static void detach(void);

/* Latch that's part of the Delta cart: */
static int ic1_old;
static int ic1_drive_select;
static _Bool ic1_side_select;
static _Bool ic1_density;

static WD279X *fdc;

static void ff44_write(uint8_t octet);

void deltados_configure(struct cart *c, struct cart_config *cc) {
	(void)cc;
	c->io_read = io_read;
	c->io_write = io_write;
	c->reset = reset;
	c->detach = detach;
	fdc = wd279x_new(WD2791);
}

static void reset(void) {
	wd279x_reset(fdc);
	ic1_old = -1;
	ic1_drive_select = -1;
	ff44_write(0);
}

static void detach(void) {
	wd279x_free(fdc);
	fdc = NULL;
}

static uint8_t io_read(uint16_t A) {
	if ((A & 4) == 0) return wd279x_read(fdc, A);
	return 0x7e;
}

static void io_write(uint16_t A, uint8_t D) {
	if ((A & 4) == 0) wd279x_write(fdc, A, D);
	if (A & 4) ff44_write(D);
}

/* Delta cartridge circuitry */
static void ff44_write(uint8_t octet) {
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
	wd279x_set_dden(fdc, !ic1_density);
}
