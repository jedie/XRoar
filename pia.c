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

#include <string.h>

#include "types.h"
#include "logging.h"
#include "m6809.h"
#include "machine.h"
#include "xroar.h"
#include "pia.h"

pia_port PIA_0A, PIA_0B, PIA_1A, PIA_1B;

void pia_init(void) {
}

void pia_reset(void) {
	/* Side A */
	PIA_0A.control_register		= PIA_1A.control_register	= 0;
	PIA_0A.direction_register	= PIA_1A.direction_register	= 0;
	PIA_0A.output_register		= PIA_1A.output_register	= 0;
	PIA_0A.port_input		= PIA_1A.port_input		= 0xff;
	PIA_0A.tied_low			= PIA_1A.tied_low		= 0xff;
	PIA_UPDATE_OUTPUT(PIA_0A);
	PIA_UPDATE_OUTPUT(PIA_1A);
	/* Side B */
	PIA_0B.control_register		= PIA_1B.control_register	= 0;
	PIA_0B.direction_register	= PIA_1B.direction_register	= 0;
	PIA_0B.output_register		= PIA_1B.output_register	= 0;
	PIA_0B.port_input		= PIA_1B.port_input		= 0;
	PIA_0B.tied_low			= PIA_1B.tied_low		= 0xff;
	if (IS_DRAGON64) {
		PIA_1B.port_input |= (1<<2);
	}
	PIA_UPDATE_OUTPUT(PIA_0B);
	PIA_UPDATE_OUTPUT(PIA_1B);
	/* Clear interrupt lines */
	PIA_0A.interrupt_received = PIA_1A.interrupt_received = 0;
	PIA_0B.interrupt_received = PIA_1B.interrupt_received = 0;
	irq = firq = 0;
}
