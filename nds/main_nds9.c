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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <nds.h>
#include <fat.h>

#include "xroar.h"
#include "events.h"
#include "logging.h"
#include "m6809.h"
#include "machine.h"

int main(int argc, char **argv) {
	if (!fatInitDefault()) {
		LOG_ERROR("fatInitDefault() failed.\n");
	}
	irqInit();
	xroar_init(argc, argv);
	irqEnable(IRQ_VBLANK | IRQ_VCOUNT);
	while (1) {
		m6809_run(456);
		while (EVENT_PENDING(UI_EVENT_LIST))
			DISPATCH_NEXT_EVENT(UI_EVENT_LIST);
	}
	return 0;
}
