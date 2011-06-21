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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curses.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#include "types.h"
#include "logging.h"
#include "events.h"
#include "input.h"
#include "keyboard.h"
#include "machine.h"
#include "module.h"
#include "xroar.h"

static int init(void);
static void shutdown(void);

KeyboardModule keyboard_curses_module = {
	.common = { .name = "curses", .description = "Curses keyboard driver",
	            .init = init, .shutdown = shutdown }
};

static event_t *poll_event;
static void do_poll(void);

static int init(void) {
	poll_event = event_new();
	poll_event->dispatch = do_poll;
	poll_event->at_cycle = current_cycle + (OSCILLATOR_RATE / 100);
	event_queue(&UI_EVENT_LIST, poll_event);
	return 0;
}

static void shutdown(void) {
	event_free(poll_event);
}

static void do_poll(void) {
	//char keyval[24];
	int key;
	while ((key = getch()) != ERR) {
		//snprintf(keyval, sizeof(keyval), "%d   ", key);
		//mvaddstr(0,34, keyval);
		switch (key) {
			case 3:   /* Ctrl+C */
			case 17:  /* Ctrl+Q */
				endwin();
				xroar_quit();
				break;
			case 5:   /* Ctrl+E */
				xroar_set_dos(XROAR_TOGGLE);
				break;
			case 11:  /* Ctrl+K */
				xroar_set_keymap(XROAR_CYCLE);
				break;
			case 12:  /* Ctrl+L */
			case 2:   /* Ctrl+B */
			case 20:  /* Ctrl+T */
				endwin();
				xroar_run_file(NULL);
				break;
			case 18:  /* Ctrl+R */
				machine_reset(RESET_SOFT);
				break;
			case 19:  /* Ctrl+S */
				endwin();
				xroar_save_snapshot();
				break;
			case 23:  /* Ctrl+W */
				endwin();
				xroar_select_tape_output();
				break;
			case 26:  /* Ctrl+Z */
				endwin();
				kill(getpid(), SIGSTOP);
				break;
			case 258:  /* Down */
				keyboard_queue(10);
				break;
			case 259:  /* Up */
				keyboard_queue(94);
				break;
			case 260:  /* Left */
				keyboard_queue(8);
				break;
			case 261:  /* Right */
				keyboard_queue(9);
				break;
			case 263:  /* Delete? */
				keyboard_queue(8);
				break;
			default:
				keyboard_queue(key);
				break;
		}
	}
	poll_event->at_cycle += OSCILLATOR_RATE / 100;
	event_queue(&UI_EVENT_LIST, poll_event);
}
