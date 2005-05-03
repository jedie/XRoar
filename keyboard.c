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
unsigned int keyboard_column[9];
unsigned int keyboard_row[9];

unsigned int keyboard_buffer[256];
unsigned int *keyboard_bufcur, *keyboard_buflast;

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
	unsigned int i;
	for (i = 0; i < 8; i++) {
		keyboard_column[i] = ~0;
		keyboard_row[i] = ~0;
	}
}

void keyboard_column_update(void) {
	unsigned int mask = PIA_0B.port_output;
	unsigned int i, row = 0x7f;
	for (i = 0; i < 8; i++) {
		if (!(mask & (1 << i))) {
			row &= keyboard_column[i];
		}
	}
	PIA_0A.port_input = (PIA_0A.port_input & 0x80) | row;
}

void keyboard_row_update(void) {
	unsigned int mask = PIA_0A.port_output;
	unsigned int i, col = 0xff;
	for (i = 0; i < 7; i++) {
		if (!(mask & (1 << i))) {
			col &= keyboard_row[i];
		}
	}
	PIA_0B.port_input = col;
}

void keyboard_queue_string(const char *s) {
	uint_fast16_t c;
	while ( (c = *(s++)) ) {
		*(keyboard_buflast++) = (~c)&0x80; /* shift/unshift */
		*(keyboard_buflast++) = c & 0x7f;
		*(keyboard_buflast++) = (c & 0x7f) | 0x80;
	}
	*(keyboard_buflast++) = 0x80; /* unshift */
}

void keyboard_queue(uint_least16_t c) {
	int shift_state = keyboard_column[keymap[0].col] & (1<<keymap[0].row);
	switch (c>>8) {
		case 1:
			*(keyboard_buflast++) = 0;  /* shift */
			break;
		case 2:
			*(keyboard_buflast++) = 0x80;  /* unshift */
			break;
		case 3:
			*(keyboard_buflast++) = 0;     /* shift */
			*(keyboard_buflast++) = 0x0c;  /* clear */
			break;
		default:
			break;
	}
	*(keyboard_buflast++) = c & 0x7f;
	*(keyboard_buflast++) = (c & 0x7f) | 0x80;
	if ((c>>8) == 3) *(keyboard_buflast++) = 0x8c;  /* unclear */
	*(keyboard_buflast++) = shift_state ? 0x80 : 0; /* last shift state */
}
