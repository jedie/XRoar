/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2006  Ciaran Anscomb
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "cart.h"
#include "events.h"
#include "fs.h"
#include "joystick.h"
#include "keyboard.h"
#include "logging.h"
#include "m6809.h"
#include "machine.h"
#include "pia.h"
#include "sam.h"
#include "snapshot.h"
#include "sound.h"
#include "ui.h"
#include "video.h"
#include "wd2797.h"
#include "xroar.h"

Cycle current_cycle;

#ifdef TRACE
int trace = 0;
#endif

static char *snapshot_load = NULL;

static void xroar_helptext(void) {
	printf("  -snap FILENAME        load snapshot after initialising\n");
	printf("  -h                    display this help and exit\n");
}

void xroar_getargs(int argc, char **argv) {
	int i;
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-snap")) {
			i++;
			if (i >= argc) break;
			snapshot_load = argv[i];
		} else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")
				|| !strcmp(argv[i], "-help")) {
			printf("Usage: xroar [OPTION]...\n\n");
			machine_helptext();
			cart_helptext();
			video_helptext();
			sound_helptext();
			ui_helptext();
			xroar_helptext();
			exit(0);
		}
	}
	/* Let some subsystems scan for options relevant to them */
	video_getargs(argc, argv);
	sound_getargs(argc, argv);
	ui_getargs(argc, argv);
	keyboard_getargs(argc, argv);
	machine_getargs(argc, argv);
	cart_getargs(argc, argv);
}

void xroar_init(void) {
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
	machine_init();
	/* Reset everything */
	joystick_reset();
	keyboard_reset();
	machine_reset(RESET_HARD);
	if (snapshot_load)
		read_snapshot(snapshot_load);
}

void xroar_shutdown(void) {
	joystick_shutdown();
	ui_shutdown();
	sound_shutdown();
	video_shutdown();
}

void xroar_mainloop(void) {
	while (1) {
		while (EVENT_PENDING)
			DISPATCH_NEXT_EVENT;
		m6809_cycle(event_list->at_cycle);
	}
}
