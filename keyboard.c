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

#include <string.h>
#include <stdlib.h>
#include "portalib/glib.h"

#include "breakpoint.h"
#include "events.h"
#include "keyboard.h"
#include "logging.h"
#include "machine.h"
#include "mc6821.h"
#include "xroar.h"

/* These map virtual scancodes to keyboard matrix points */
static Keymap dragon_keymap = {
	{7,6}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, /* 0x00 - 0x07 */
	{5,5}, {6,5}, {4,5}, {8,8}, {1,6}, {0,6}, {8,8}, {8,8}, /* 0x08 - 0x0f */
	{8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, /* 0x10 - 0x17 */
	{8,8}, {8,8}, {8,8}, {2,6}, {8,8}, {8,8}, {8,8}, {8,8}, /* 0x18 - 0x1f */
	{7,5}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, /* 0x20 - 0x27 */
	{8,8}, {8,8}, {8,8}, {8,8}, {4,1}, {5,1}, {6,1}, {7,1}, /* 0x28 - 0x2f */
	{0,0}, {1,0}, {2,0}, {3,0}, {4,0}, {5,0}, {6,0}, {7,0}, /* 0x30 - 0x37 */
	{0,1}, {1,1}, {2,1}, {3,1}, {8,8}, {8,8}, {8,8}, {8,8}, /* 0x38 - 0x3f */
	{0,2}, {1,2}, {2,2}, {3,2}, {4,2}, {5,2}, {6,2}, {7,2}, /* 0x40 - 0x47 */
	{0,3}, {1,3}, {2,3}, {3,3}, {4,3}, {5,3}, {6,3}, {7,3}, /* 0x48 - 0x4f */
	{0,4}, {1,4}, {2,4}, {3,4}, {4,4}, {5,4}, {6,4}, {7,4}, /* 0x50 - 0x57 */
	{0,5}, {1,5}, {2,5}, {8,8}, {8,8}, {8,8}, {3,5}, {8,8}, /* 0x58 - 0x5f */
	{1,6}, {1,2}, {2,2}, {3,2}, {4,2}, {5,2}, {6,2}, {7,2}, /* 0x60 - 0x67 */
	{0,3}, {1,3}, {2,3}, {3,3}, {4,3}, {5,3}, {6,3}, {7,3}, /* 0x68 - 0x6f */
	{0,4}, {1,4}, {2,4}, {3,4}, {4,4}, {5,4}, {6,4}, {7,4}, /* 0x70 - 0x77 */
	{0,5}, {1,5}, {2,5}, {8,8}, {8,8}, {8,8}, {8,8}, {5,5}, /* 0x78 - 0x7f */
};
/* CoCo keymap populated in keyboard_init() */
static Keymap coco_keymap;
/* Active keymap */
Keymap keymap;

static unsigned unicode_to_dragon[128] = {
	0,       0,       0,       0,       0,       0,       0,       0,
	8,       9,       10,      0,       12,      13,      0,       0,
	0,       0,       0,       0,       0,       128+8,   0,       0,
	0,       0,       0,       27,      0,       0,       0,       0,
	' ',     128+'1', 128+'2', 128+'3', 128+'4', 128+'5', 128+'6', 128+'7',
	128+'8', 128+'9', 128+':', 128+';', ',',     '-',     '.',     '/',
	'0',     '1',     '2',     '3',     '4',     '5',     '6',     '7',
	'8',     '9',     ':',     ';',     128+',', 128+'-', 128+'.', 128+'/',
	'@',     128+'a', 128+'b', 128+'c', 128+'d', 128+'e', 128+'f', 128+'g',
	128+'h', 128+'i', 128+'j', 128+'k', 128+'l', 128+'m', 128+'n', 128+'o',
	128+'p', 128+'q', 128+'r', 128+'s', 128+'t', 128+'u', 128+'v', 128+'w',
	128+'x', 128+'y', 128+'z', 128+10,  128+12,  128+9,   '^',     128+'^',
	12,      'a',     'b',     'c',     'd',     'e',     'f',     'g',
	'h',     'i',     'j',     'k',     'l',     'm',     'n',     'o',
	'p',     'q',     'r',     's',     't',     'u',     'v',     'w',
	'x',     'y',     'z',     0,       0,       0,       128+'@', 8
};

/* These contain masks to be applied when the corresponding row/column is
 * held low.  eg, if row 1 is outputting a 0 , keyboard_column[1] will
 * be applied on column reads */
unsigned keyboard_column[9];
unsigned keyboard_row[9];

void keyboard_init(void) {
	int i;
	for (i = 0; i < 8; i++) {
		keyboard_column[i] = ~0;
		keyboard_row[i] = ~0;
	}
	for (i = 0; i < 128; i++) {
		coco_keymap[i].col = dragon_keymap[i].col;
		if (dragon_keymap[i].row == 6) {
			coco_keymap[i].row = 6;
		} else {
			coco_keymap[i].row = (dragon_keymap[i].row + 4) % 6;
		}
	}
}

void keyboard_set_keymap(int map) {
	map %= NUM_KEYMAPS;
	xroar_machine_config->keymap = map;
	switch (map) {
		default:
		case KEYMAP_DRAGON:
			memcpy(keymap, dragon_keymap, sizeof(keymap));
			break;
		case KEYMAP_COCO:
			memcpy(keymap, coco_keymap, sizeof(keymap));
			break;
	}
}

void keyboard_update(void) {
	unsigned row_out = PIA0.a.out_sink;
	unsigned col_out = PIA0.b.out_source & PIA0.b.out_sink;
	unsigned row_in = ~0, col_in = ~0;
	unsigned old, i;

	/* Pull low any directly connected rows & columns */
	for (i = 0; i < 8; i++) {
		if (!(col_out & (1 << i))) {
			row_in &= keyboard_column[i];
		}
	}
	for (i = 0; i < 7; i++) {
		if (!(row_out & (1 << i))) {
			col_in &= keyboard_row[i];
		}
	}
	/* Ghosting: pull low column inputs that share any pulled low rows, and
	 * merge that column's direct row connections.  Repeat until no change
	 * in the row mask. */
	do {
		old = row_in;
		for (i = 0; i < 8; i++) {
			if (~row_in & ~keyboard_column[i]) {
				col_in &= ~(1 << i);
				row_in &= keyboard_column[i];
			}
		}
	} while (old != row_in);
	/* Likewise the other way around. */
	do {
		old = col_in;
		for (i = 0; i < 7; i++) {
			if (~col_in & ~keyboard_row[i]) {
				row_in &= ~(1 << i);
				col_in &= keyboard_row[i];
			}
		}
	} while (old != col_in);

	/* Update inputs */
	PIA0.a.in_sink = (PIA0.a.in_sink & 0x80) | (row_in & 0x7f);
	PIA0.b.in_sink = col_in;
}

void keyboard_unicode_press(unsigned unicode) {
	if (unicode == '\\') {
		/* CoCo and Dragon 64 in 64K mode have a different way
		 * of scanning for '\' */
		if (IS_COCO_KEYMAP || (IS_DRAGON64 && !(PIA_VALUE_B(PIA1) & 0x04))) {
			KEYBOARD_PRESS(0);
			KEYBOARD_PRESS(12);
		} else {
			KEYBOARD_PRESS(0);
			KEYBOARD_PRESS(12);
			KEYBOARD_PRESS(',');
		}
	} else if (unicode == 163) {
		/* Pound sign */
		KEYBOARD_PRESS(0);
		KEYBOARD_PRESS('3');
	} else if (unicode < 128) {
		unsigned code = unicode_to_dragon[unicode];
		if (code & 128)
			KEYBOARD_PRESS(0);
		else
			KEYBOARD_RELEASE(0);
		KEYBOARD_PRESS(code & 0x7f);
	}
	keyboard_update();
}

void keyboard_unicode_release(unsigned unicode) {
	if (unicode == '\\') {
		/* CoCo and Dragon 64 in 64K mode have a different way
		 * of scanning for '\' */
		if (IS_COCO_KEYMAP || (IS_DRAGON64 && !(PIA_VALUE_B(PIA1) & 0x04))) {
			KEYBOARD_RELEASE(0);
			KEYBOARD_RELEASE(12);
		} else {
			KEYBOARD_RELEASE(0);
			KEYBOARD_RELEASE(12);
			KEYBOARD_RELEASE(',');
		}
	} else if (unicode == 163) {
		/* Pound sign */
		KEYBOARD_RELEASE(0);
		KEYBOARD_RELEASE('3');
	} else if (unicode < 128) {
		unsigned code = unicode_to_dragon[unicode];
		if (code & 128)
			KEYBOARD_RELEASE(0);
		KEYBOARD_RELEASE(code & 0x7f);
	}
	keyboard_update();
}

static GSList *basic_command_list = NULL;
static const char *basic_command = NULL;

static void type_command(struct MC6809 *cpu);

static struct breakpoint basic_command_breakpoint[] = {
	BP_DRAGON_ROM(.address = 0xbbe5, .handler = type_command),
	BP_COCO_BAS10_ROM(.address = 0xa1c1, .handler = type_command),
	BP_COCO_BAS11_ROM(.address = 0xa1c1, .handler = type_command),
	BP_COCO_BAS12_ROM(.address = 0xa1cb, .handler = type_command),
	BP_COCO_BAS13_ROM(.address = 0xa1cb, .handler = type_command),
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
