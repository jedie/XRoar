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
static Cycle next_hsync, next_keyboard_poll;
int enable_disk_interrupt;
static int_least16_t vdg_call;

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
	event_init();
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
	vdg_call = 0;
	enable_disk_interrupt = 0;
	next_hsync = current_cycle + CYCLES_PER_SCANLINE;
	next_keyboard_poll = current_cycle + 141050;
	/* Reset everything */
	sound_module->reset();
	joystick_reset();
	keyboard_reset();
	machine_reset(hard);
}

void xroar_mainloop(void) {
	while (1) {
		Cycle until;
		if ((int)(current_cycle - next_hsync) >= 0) {
			next_hsync += CYCLES_PER_SCANLINE;
			if (vdg_call >= TOP_BORDER_OFFSET) {
				if (vdg_call < (24 + TOP_BORDER_OFFSET)) {
#ifndef HAVE_GP32
					video_module->render_border();
#endif
				} else {
					if (vdg_call < (216 + TOP_BORDER_OFFSET)) {
#ifdef HAVE_GP32
						if ((vdg_call & 3) == 0)
#endif
							vdg_render_scanline();
					} else {
						if (vdg_call < (240 + TOP_BORDER_OFFSET)) {
#ifndef HAVE_GP32
							video_module->render_border();
#endif
						}
					}
				}
			}
			PIA_SET_P0CA1;
			vdg_call++;
			if (vdg_call == ACTIVE_SCANLINES_PER_FRAME) {
				//vdg_vsync();
				sam_vdg_fsync();
				PIA_SET_P0CB1;
			}
			if (vdg_call >= TOTAL_SCANLINES_PER_FRAME) {
				video_module->vdg_vsync();  // XXX
				vdg_call = 0;
			}
		}
		if ((int)(current_cycle - next_keyboard_poll) >= 0) {
			next_keyboard_poll += 141050;
			if (KEYBOARD_HASQUEUE) {
				unsigned int k;
				next_keyboard_poll += 141050;
				KEYBOARD_DEQUEUE(k);
				if (k & 0x80) {
					KEYBOARD_RELEASE(k & 0x7f);
				} else {
					KEYBOARD_PRESS(k);
				}
			} else {
				keyboard_module->poll();
			}
#ifndef HAVE_GP32
			joystick_module->poll();
#endif
		}
		while (EVENT_PENDING)
			DISPATCH_NEXT_EVENT;
		if (EVENT_EXISTS)
			until = event_list->at_cycle;
		else
			until = next_hsync;
		if (next_hsync < until) until = next_hsync;
		if (next_keyboard_poll < until) until = next_keyboard_poll;
		m6809_cycle(until);
	}
}
