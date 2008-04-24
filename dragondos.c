/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2008  Ciaran Anscomb
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
 *     DragonDOS cartridge detail:
 *         http://www.dragon-archive.co.uk/
 */

#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "dragondos.h"
#include "events.h"
#include "logging.h"
#include "m6809.h"
#include "machine.h"
#include "mc6821.h"
#include "vdrive.h"
#include "wd279x.h"
#include "xroar.h"

#define QUEUE_NMI() do { \
		nmi_event.at_cycle = current_cycle + 1; \
		event_queue(&MACHINE_EVENT_LIST, &nmi_event); \
	} while (0)

/* Handle signals from WD2797 */
static void set_drq_handler(void);
static void reset_drq_handler(void);
static void set_intrq_handler(void);
static void reset_intrq_handler(void);

/* NMIs queued to allow CPU to run next instruction */
static event_t nmi_event;
static void do_nmi(void);

/* Latch that's part of the DragonDOS cart: */
static unsigned int ic1_drive_select;
static unsigned int ic1_motor_enable;
static unsigned int ic1_precomp_enable;
static unsigned int ic1_density;
static unsigned int ic1_nmi_enable;

static void ff48_write(unsigned int octet);

void dragondos_init(void) {
	event_init(&nmi_event);
	nmi_event.dispatch = do_nmi;
}

void dragondos_reset(void) {
	wd279x_type = WD2797;
	wd279x_set_drq_handler     = set_drq_handler;
	wd279x_reset_drq_handler   = reset_drq_handler;
	wd279x_set_intrq_handler   = set_intrq_handler;
	wd279x_reset_intrq_handler = reset_intrq_handler;
	wd279x_reset();
	ic1_drive_select = 0xff;
	ic1_motor_enable = 0xff;
	ic1_precomp_enable = 0xff;
	ic1_density = 0xff;
	ic1_nmi_enable = 0xff;
	ff48_write(0);
}

unsigned int dragondos_read(unsigned int addr) {
	if ((addr & 15) == 0) return wd279x_status_read();
	if ((addr & 15) == 1) return wd279x_track_register_read();
	if ((addr & 15) == 2) return wd279x_sector_register_read();
	if ((addr & 15) == 3) return wd279x_data_register_read();
	return 0x7e;
}

void dragondos_write(unsigned int addr, unsigned int val) {
	if ((addr & 15) == 0) wd279x_command_write(val);
	if ((addr & 15) == 1) wd279x_track_register_write(val);
	if ((addr & 15) == 2) wd279x_sector_register_write(val);
	if ((addr & 15) == 3) wd279x_data_register_write(val);
	if (addr & 8) ff48_write(val);
}

/* DragonDOS cartridge circuitry */
static void ff48_write(unsigned int octet) {
	LOG_DEBUG(4, "DragonDOS: Write to FF48: ");
	if ((octet & 0x03) != ic1_drive_select) {
		LOG_DEBUG(4, "DRIVE SELECT %01d, ", octet & 0x03);
	}
	if ((octet & 0x04) != ic1_motor_enable) {
		LOG_DEBUG(4, "MOTOR %s, ", (octet & 0x04)?"ON":"OFF");
	}
	if ((octet & 0x08) != ic1_density) {
		LOG_DEBUG(4, "DENSITY %s, ", (octet & 0x08)?"SINGLE":"DOUBLE");
	}
	if ((octet & 0x10) != ic1_precomp_enable) {
		LOG_DEBUG(4, "PRECOMP %s, ", (octet & 0x10)?"ON":"OFF");
	}
	if ((octet & 0x20) != ic1_nmi_enable) {
		LOG_DEBUG(4, "NMI %s, ", (octet & 0x20)?"ENABLED":"DISABLED");
	}
	LOG_DEBUG(4, "\n");
	ic1_drive_select = octet & 0x03;
	vdrive_set_drive(ic1_drive_select);
	ic1_motor_enable = octet & 0x04;
	ic1_density = octet & 0x08;
	wd279x_set_density(ic1_density);
	ic1_precomp_enable = octet & 0x10;
	ic1_nmi_enable = octet & 0x20;
}

static void set_drq_handler(void) {
	PIA_SET_Cx1(PIA1.b);
}

static void reset_drq_handler(void) {
	PIA_RESET_Cx1(PIA1.b);
}

static void set_intrq_handler(void) {
	QUEUE_NMI();
}

static void reset_intrq_handler(void) {
	nmi = 0;
}

static void do_nmi(void) {
	if (ic1_nmi_enable) {
		nmi = 1;
	}
}
