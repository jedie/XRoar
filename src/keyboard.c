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

/* Compute which rows & columns are to act as sinks based on inputs to the
 * matrix and the current state of depressed keys. */

void keyboard_read_matrix(int row_in, int col_in, int *row_sink, int *col_sink) {
	/* Pull low any directly connected rows & columns */
	for (int i = 0; i < 8; i++) {
		if (!(col_in & (1 << i))) {
			*row_sink &= keyboard_column[i];
		}
	}
	for (int i = 0; i < 7; i++) {
		if (!(row_in & (1 << i))) {
			*col_sink &= keyboard_row[i];
		}
	}
	/* Ghosting: pull low column inputs that share any pulled low rows, and
	 * merge that column's direct row connections.  Repeat until no change
	 * in the row mask. */
	int old;
	do {
		old = *row_sink;
		for (int i = 0; i < 8; i++) {
			if (~*row_sink & ~keyboard_column[i]) {
				*col_sink &= ~(1 << i);
				*row_sink &= keyboard_column[i];
			}
		}
	} while (old != *row_sink);
	/* Likewise the other way around. */
	do {
		old = *col_sink;
		for (int i = 0; i < 7; i++) {
			if (~*col_sink & ~keyboard_row[i]) {
				*row_sink &= ~(1 << i);
				*col_sink &= keyboard_row[i];
			}
		}
	} while (old != *col_sink);
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
static const char *basic_command = NULL;

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
		MC6809_REG_A(cpu) = chr;
		cpu->reg_cc &= ~4;
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

void keyboard_queue_basic(const char *s) {
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
