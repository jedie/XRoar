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

#include "types.h"
#include "keyboard.h"
#include "pia.h"
#include "logging.h"

#ifdef HAVE_SDL_KEYBOARD
extern KeyboardModule keyboard_sdl_module;
#endif
#ifdef HAVE_GP32
extern KeyboardModule keyboard_gp32_module;
#endif

KeyboardModule *keyboard_module;

/* These contain masks to be applied when the corresponding row/column is
 * held low.  eg, if row 1 is outputting a 0 , keyboard_column[1] will
 * be applied on column reads */
uint_fast8_t keyboard_column[9];
uint_fast8_t keyboard_row[9];

uint_fast8_t keyboard_buffer[256];
uint_fast8_t *keyboard_bufcur, *keyboard_buflast;

int keyboard_init(void) {
	/* A video module should have set keyboard_module by this point */
	if (keyboard_module == NULL)
		return 1;
	keyboard_bufcur = keyboard_buflast = keyboard_buffer;
	return keyboard_module->init();
}

void keyboard_shutdown(void) {
	if (keyboard_module)
		keyboard_module->shutdown();
}

void keyboard_reset(void) {
	uint_fast8_t i;
	for (i = 0; i < 8; i++) {
		keyboard_column[i] = ~0;
		keyboard_row[i] = ~0;
	}
}

void keyboard_column_update(void) {
	uint_fast8_t mask = PIA_0B.port_output;
	uint_fast8_t i, row = 0x7f;
	for (i = 0; i < 8; i++) {
		if (!(mask & (1 << i))) {
			row &= keyboard_column[i];
		}
	}
	PIA_0A.port_input = (PIA_0A.port_input & 0x80) | row;
}

void keyboard_row_update(void) {
	uint_fast8_t mask = PIA_0A.port_output;
	uint_fast8_t i, col = 0xff;
	for (i = 0; i < 7; i++) {
		if (!(mask & (1 << i))) {
			col &= keyboard_row[i];
		}
	}
	PIA_0B.port_input = col;
}

void keyboard_queue_string(char *s) {
	for (; *s; s++) {
		KEYBOARD_QUEUE(*s);
	}
}
