/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2004  Ciaran Anscomb
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

#include "xroar.h"
#include "pia.h"
#include "logging.h"
#include "types.h"

pia_port PIA_0A, PIA_0B, PIA_1A, PIA_1B;

void pia_init(void) {
	memset(&PIA_0A, 0, sizeof(pia_port));
	memset(&PIA_0B, 0, sizeof(pia_port));
	memset(&PIA_1A, 0, sizeof(pia_port));
	memset(&PIA_1B, 0, sizeof(pia_port));
	PIA_0A.tied_low = PIA_0B.tied_low = PIA_1A.tied_low =
		PIA_1B.tied_low = 0xff;
}

void pia_reset(void) {
	PIA_0A.control_register = PIA_0A.direction_register =
		PIA_0A.output_register = 0;
	PIA_0B.control_register = PIA_0B.direction_register =
		PIA_0B.output_register = 0;
	PIA_1A.control_register = PIA_1A.direction_register =
		PIA_1A.output_register = 0;
	PIA_1B.control_register = PIA_1B.direction_register =
		PIA_1B.output_register = 0;
	PIA_0A.port_output = PIA_0B.port_output = PIA_1A.port_output =
		PIA_1B.port_output = 0;
	PIA_0A.port_input = PIA_0B.port_input = PIA_1A.port_input =
		PIA_1B.port_input = 0;
	PIA_0A.tied_low = PIA_0B.tied_low = PIA_1A.tied_low =
		PIA_1B.tied_low = 0xff;
	PIA_0A.interrupt_enable = PIA_0A.interrupt_transition =
		PIA_1A.interrupt_enable = PIA_1A.interrupt_transition =
		PIA_0B.interrupt_enable = PIA_0B.interrupt_transition =
		PIA_1B.interrupt_enable = PIA_1B.interrupt_transition = 0;
	PIA_0A.interrupt_transition = PIA_0A.register_select =
		PIA_1A.interrupt_transition = PIA_1A.register_select =
		PIA_0B.interrupt_transition = PIA_0B.register_select =
		PIA_1B.interrupt_transition = PIA_1B.register_select = 0;
}
