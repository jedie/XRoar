/*  Copyright 2003-2013 Ciaran Anscomb
 *
 *  This file is part of XRoar.
 *
 *  XRoar is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  XRoar is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XRoar.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "pl_glib.h"

#include "breakpoint.h"
#include "dkbd.h"
#include "events.h"
#include "keyboard.h"
#include "logging.h"
#include "machine.h"
#include "mc6809.h"
#include "xroar.h"

/* Map of virtual scancodes to keyboard matrix points: */
struct dkbd_map keymap_new;

/* Current chording mode - only affects how backslash is typed: */
static enum keyboard_chord_mode chord_mode = keyboard_chord_mode_dragon_32k_basic;

/* These contain masks to be applied when the corresponding row/column is held
 * low.  e.g., if row 1 is outputting a 0, keyboard_column[1] will be applied
 * on column reads: */

unsigned keyboard_column[9];
unsigned keyboard_row[9];

void keyboard_init(void) {
	int i;
	for (i = 0; i < 8; i++) {
		keyboard_column[i] = ~0;
		keyboard_row[i] = ~0;
	}
}

void keyboard_set_keymap(int map) {
	map %= NUM_KEYMAPS;
	xroar_machine_config->keymap = map;
	dkbd_map_init(&keymap_new, map);
}

void keyboard_set_chord_mode(enum keyboard_chord_mode mode) {
	chord_mode = mode;
	if (keymap_new.layout == dkbd_layout_dragon) {
		if (mode == keyboard_chord_mode_dragon_32k_basic) {
			keymap_new.unicode_to_dkey['\\'].dk_key = DSCAN_COMMA;
		} else {
			keymap_new.unicode_to_dkey['\\'].dk_key = DSCAN_INVALID;
		}
	}
}

/* Compute sources & sinks based on inputs to the matrix and the current state
 * of depressed keys. */

void keyboard_read_matrix(struct keyboard_state *state) {
	/* Ghosting: combine columns that share any pressed rows.  Repeat until
	 * no change in the row mask. */
	int old;
	do {
		old = state->row_sink;
		for (int i = 0; i < 8; i++) {
			if (~state->row_sink & ~keyboard_column[i]) {
				state->col_sink &= ~(1 << i);
				state->row_sink &= keyboard_column[i];
			}
		}
	} while (old != state->row_sink);
	/* Likewise combining rows. */
	do {
		old = state->col_sink;
		for (int i = 0; i < 7; i++) {
			if (~state->col_sink & ~keyboard_row[i]) {
				state->row_sink &= ~(1 << i);
				state->col_sink &= keyboard_row[i];
			}
		}
	} while (old != state->col_sink);

	/* Sink & source any directly connected rows & columns */
	for (int i = 0; i < 8; i++) {
		if (!(state->col_sink & (1 << i))) {
			state->row_sink &= keyboard_column[i];
		}
		if (state->col_source & (1 << i)) {
			state->row_source |= ~keyboard_column[i];
		}
	}
	for (int i = 0; i < 7; i++) {
		if (!(state->row_sink & (1 << i))) {
			state->col_sink &= keyboard_row[i];
		}
		if (state->row_source & (1 << i)) {
			state->col_source |= ~keyboard_row[i];
		}
	}
}

void keyboard_unicode_press(unsigned unicode) {
	if (unicode >= DKBD_U_TABLE_SIZE)
		return;
	if (keymap_new.unicode_to_dkey[unicode].dk_mod & DK_MOD_SHIFT)
		KEYBOARD_PRESS_SHIFT;
	if (keymap_new.unicode_to_dkey[unicode].dk_mod & DK_MOD_UNSHIFT)
		KEYBOARD_RELEASE_SHIFT;
	if (keymap_new.unicode_to_dkey[unicode].dk_mod & DK_MOD_CLEAR)
		KEYBOARD_PRESS_CLEAR;
	keyboard_press(keymap_new.unicode_to_dkey[unicode].dk_key);
	return;
}

void keyboard_unicode_release(unsigned unicode) {
	if (unicode >= DKBD_U_TABLE_SIZE)
		return;
	if (keymap_new.unicode_to_dkey[unicode].dk_mod & DK_MOD_SHIFT)
		KEYBOARD_RELEASE_SHIFT;
	if (keymap_new.unicode_to_dkey[unicode].dk_mod & DK_MOD_UNSHIFT)
		KEYBOARD_PRESS_SHIFT;
	if (keymap_new.unicode_to_dkey[unicode].dk_mod & DK_MOD_CLEAR)
		KEYBOARD_RELEASE_CLEAR;
	keyboard_release(keymap_new.unicode_to_dkey[unicode].dk_key);
	return;
}

static GSList *basic_command_list = NULL;
static const uint8_t *basic_command = NULL;

static void type_command(struct MC6809 *cpu);

static struct breakpoint basic_command_breakpoint[] = {
	BP_DRAGON_ROM(.address = 0xbbe5, .handler = (bp_handler)type_command),
	BP_COCO_BAS10_ROM(.address = 0xa1c1, .handler = (bp_handler)type_command),
	BP_COCO_BAS11_ROM(.address = 0xa1c1, .handler = (bp_handler)type_command),
	BP_COCO_BAS12_ROM(.address = 0xa1cb, .handler = (bp_handler)type_command),
	BP_COCO_BAS13_ROM(.address = 0xa1cb, .handler = (bp_handler)type_command),
	BP_MX1600_BAS_ROM(.address = 0xa1cb, .handler = (bp_handler)type_command),
};

static void type_command(struct MC6809 *cpu) {
	if (basic_command) {
		int chr = *(basic_command++);
		if (chr == '\\') {
			chr = *(basic_command++);
			switch (chr) {
				case '0': chr = '\0'; break;
				case 'e': chr = '\003'; break;
				case 'f': chr = '\f'; break;
				case 'n':
				case 'r': chr = '\r'; break;
				default: break;
			}
		}
		if (keymap_new.layout == dkbd_layout_dragon200e) {
			switch (chr) {
			case '[': chr = 0x00; break;
			case ']': chr = 0x01; break;
			case '\\': chr = 0x0b; break;
			case 0xc2:
				chr = *(basic_command++);
				switch (chr) {
				case 0xa1: chr = 0x5b; break; // ¡
				case 0xa7: chr = 0x13; break; // §
				case 0xba: chr = 0x14; break; // º
				case 0xbf: chr = 0x5d; break; // ¿
				default: chr = -1; break;
				}
				break;
			case 0xc3:
				chr = *(basic_command++);
				switch (chr) {
				case 0x80: case 0xa0: chr = 0x1b; break; // à
				case 0x81: case 0xa1: chr = 0x16; break; // á
				case 0x82: case 0xa2: chr = 0x0e; break; // â
				case 0x83: case 0xa3: chr = 0x0a; break; // ã
				case 0x84: case 0xa4: chr = 0x05; break; // ä
				case 0x87: case 0xa7: chr = 0x7d; break; // ç
				case 0x88: case 0xa8: chr = 0x1c; break; // è
				case 0x89: case 0xa9: chr = 0x17; break; // é
				case 0x8a: case 0xaa: chr = 0x0f; break; // ê
				case 0x8b: case 0xab: chr = 0x06; break; // ë
				case 0x8c: case 0xac: chr = 0x1d; break; // ì
				case 0x8d: case 0xad: chr = 0x18; break; // í
				case 0x8e: case 0xae: chr = 0x10; break; // î
				case 0x8f: case 0xaf: chr = 0x09; break; // ï
				case 0x91: chr = 0x5c; break; // Ñ
				case 0x92: case 0xb2: chr = 0x1e; break; // ò
				case 0x93: case 0xb3: chr = 0x19; break; // ó
				case 0x94: case 0xb4: chr = 0x11; break; // ô
				case 0x96: case 0xb6: chr = 0x07; break; // ö
				case 0x99: case 0xb9: chr = 0x1f; break; // ù
				case 0x9a: case 0xba: chr = 0x1a; break; // ú
				case 0x9b: case 0xbb: chr = 0x12; break; // û
				case 0x9c: chr = 0x7f; break; // Ü
				case 0x9f: chr = 0x02; break; // ß (also β)
				case 0xb1: chr = 0x7c; break; // ñ
				case 0xbc: chr = 0x7b; break; // ü
				default: chr = -1; break;
				}
				break;
			case 0xce:
				chr = *(basic_command++);
				switch (chr) {
				case 0xb1: case 0x91: chr = 0x04; break; // α
				case 0xb2: case 0x92: chr = 0x02; break; // β (also ß)
				default: chr = -1; break;
				}
				break;
			}
		}
		if (chr >= 0) {
			MC6809_REG_A(cpu) = chr;
			cpu->reg_cc &= ~4;
		}
		if (*basic_command == 0)
			basic_command = NULL;
	}
	if (!basic_command) {
		if (basic_command_list) {
			void *data = basic_command_list->data;
			basic_command_list = g_slist_remove(basic_command_list, data);
			g_free(data);
		}
		if (basic_command_list) {
			basic_command = basic_command_list->data;
		} else {
			bp_remove_list(basic_command_breakpoint);
		}
	}
	/* Use CPU read routine to pull return address back off stack */
	machine_op_rts(cpu);
}

void keyboard_queue_basic(const uint8_t *s) {
	char *data = NULL;
	bp_remove_list(basic_command_breakpoint);
	if (s) {
		data = g_strdup(s);
		basic_command_list = g_slist_append(basic_command_list, data);
	}
	if (!basic_command) {
		basic_command = data;
	}
	if (basic_command) {
		bp_add_list(basic_command_breakpoint);
	}
}
