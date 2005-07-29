/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2005  Ciaran Anscomb
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

#include "types.h"
#include "events.h"
#include "xroar.h"
#include "video.h"
#include "sound.h"
#include "keyboard.h"
#include "joystick.h"
#include "ui.h"
#include "fs.h"
#include "machine.h"
#include "m6809.h"
#include "sam.h"
#include "pia.h"
#include "wd2797.h"
#include "snapshot.h"
#include "logging.h"

Cycle current_cycle;
int enable_disk_interrupt;

#ifdef TRACE
int trace = 0;
#endif

void xroar_init(int argc, char **argv) {
	/* Let some subsystems scan for options relevant to them */
	video_getargs(argc, argv);
	sound_getargs(argc, argv);
	ui_getargs(argc, argv);
	keyboard_getargs(argc, argv);
	/* Initialise everything */
	current_cycle = 0;
	if (video_init()) {
		LOG_ERROR("No video module initialised.\n");
		exit(1);
	}
	if (sound_init()) {
		LOG_ERROR("No sound module initialised.\n");
		exit(1);
	}
	if (ui_init()) {
		LOG_ERROR("No user-interface module initialised.\n");
		exit(1);
	}
	if (keyboard_init()) {
		LOG_ERROR("No keyboard module initialised.\n");
		exit(1);
	}
	if (joystick_init()) {
		LOG_WARN("No joystick module initialised.\n");
	}
	fs_init();
	machine_init();
	xroar_reset(RESET_HARD);
}

void xroar_shutdown(void) {
	joystick_shutdown();
	ui_shutdown();
	video_shutdown();
	sound_shutdown();
}

void xroar_reset(int hard) {
	enable_disk_interrupt = 0;
	/* Reset everything */
	sound_module->reset();
	joystick_reset();
	keyboard_reset();
	machine_reset(hard);
}

void xroar_mainloop(void) {
	while (1) {
		while (EVENT_PENDING)
			DISPATCH_NEXT_EVENT;
		m6809_cycle(event_list->at_cycle);
	}
}
