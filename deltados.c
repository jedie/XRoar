/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2007  Ciaran Anscomb
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

/* Sources:
 *     Delta cartridge detail:
 *         Partly inferred from disassembly of Delta ROM,
 *         Partly from information provided by Phill Harvey-Smith on
 *         www.dragon-archive.co.uk.
 */

#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "deltados.h"
#include "logging.h"
#include "vdrive.h"
#include "wd279x.h"
#include "xroar.h"

/* Latch that's part of the Delta cart: */
static unsigned int ic1_drive_select;
static unsigned int ic1_side_select;
static unsigned int ic1_density;

void deltados_init(void) {
}

void deltados_reset(void) {
	wd279x_type = WD2791;
	wd279x_set_drq_handler     = NULL;
	wd279x_reset_drq_handler   = NULL;
	wd279x_set_intrq_handler   = NULL;
	wd279x_reset_intrq_handler = NULL;
	wd279x_reset();
	ic1_drive_select = 0xff;
	ic1_side_select  = 0xff;
	ic1_density      = 0xff;
	deltados_ff44_write(0);
}

/* Delta cartridge circuitry */
void deltados_ff44_write(unsigned int octet) {
	LOG_DEBUG(4, "Delta: Write to FF44: ");
	if ((octet & 0x03) != ic1_drive_select) {
		LOG_DEBUG(4, "DRIVE SELECT %01d, ", octet & 0x03);
	}
	if ((octet & 0x04) != ic1_side_select) {
		LOG_DEBUG(4, "SIDE %s, ", (octet & 0x04)?"1":"0");
	}
	octet ^= 0x08;
	if ((octet & 0x08) != ic1_density) {
		LOG_DEBUG(4, "DENSITY %s, ", (octet & 0x08)?"SINGLE":"DOUBLE");
	}
	LOG_DEBUG(4, "\n");
	ic1_drive_select = octet & 0x03;
	vdrive_set_drive(ic1_drive_select);
	ic1_side_select = octet & 0x04;
	vdrive_set_side(ic1_side_select ? 1 : 0);
	ic1_density = octet & 0x08;
	wd279x_set_density(ic1_density);
}
