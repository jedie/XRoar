/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2010  Ciaran Anscomb
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

#include <string.h>

#include "types.h"
#include "events.h"
#include "keyboard.h"
#include "logging.h"
#include "machine.h"
#include "mc6821.h"
#include "xroar.h"

/* These map virtual scancodes to keyboard matrix points */
static Keymap dragon_keymap = {
	{7,6}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, /* 0 - 7 */
	{5,5}, {6,5}, {4,5}, {8,8}, {1,6}, {0,6}, {8,8}, {8,8}, /* 8 - 15 */
	{8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, /* 16 - 23 */
	{8,8}, {8,8}, {8,8}, {2,6}, {8,8}, {8,8}, {8,8}, {8,8}, /* 24 - 31 */
	{7,5}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, /* 32 - 39 */
	{8,8}, {8,8}, {8,8}, {8,8}, {4,1}, {5,1}, {6,1}, {7,1}, /* 40 - 47 */
	{0,0}, {1,0}, {2,0}, {3,0}, {4,0}, {5,0}, {6,0}, {7,0}, /* 48 - 55 */
	{0,1}, {1,1}, {2,1}, {3,1}, {8,8}, {8,8}, {8,8}, {8,8}, /* 56 - 63 */
	{0,2}, {1,2}, {2,2}, {3,2}, {4,2}, {5,2}, {6,2}, {7,2}, /* 64 - 71 */
	{0,3}, {1,3}, {2,3}, {3,3}, {4,3}, {5,3}, {6,3}, {7,3}, /* 72 - 79 */
	{0,4}, {1,4}, {2,4}, {3,4}, {4,4}, {5,4}, {6,4}, {7,4}, /* 80 - 87 */
	{0,5}, {1,5}, {2,5}, {8,8}, {8,8}, {8,8}, {3,5}, {8,8}, /* 88 - 95 */
	{1,6}, {1,2}, {2,2}, {3,2}, {4,2}, {5,2}, {6,2}, {7,2}, /* 96 - 103 */
	{0,3}, {1,3}, {2,3}, {3,3}, {4,3}, {5,3}, {6,3}, {7,3}, /* 104 - 111 */
	{0,4}, {1,4}, {2,4}, {3,4}, {4,4}, {5,4}, {6,4}, {7,4}, /* 112 - 119 */
	{0,5}, {1,5}, {2,5}, {8,8}, {8,8}, {8,8}, {8,8}, {5,5}, /* 120 - 127 */
};
static Keymap coco_keymap = {
	{7,6}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, /* 0 - 7 */
	{5,3}, {6,3}, {4,3}, {8,8}, {1,6}, {0,6}, {8,8}, {8,8}, /* 8 - 15 */
	{8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, /* 16 - 23 */
	{8,8}, {8,8}, {8,8}, {2,6}, {8,8}, {8,8}, {8,8}, {8,8}, /* 24 - 31 */
	{7,3}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, /* 32 - 39 */
	{8,8}, {8,8}, {8,8}, {8,8}, {4,5}, {5,5}, {6,5}, {7,5}, /* 40 - 47 */
	{0,4}, {1,4}, {2,4}, {3,4}, {4,4}, {5,4}, {6,4}, {7,4}, /* 48 - 55 */
	{0,5}, {1,5}, {2,5}, {3,5}, {8,8}, {5,5}, {8,8}, {8,8}, /* 56 - 63 */
	{0,0}, {1,0}, {2,0}, {3,0}, {4,0}, {5,0}, {6,0}, {7,0}, /* 64 - 71 */
	{0,1}, {1,1}, {2,1}, {3,1}, {4,1}, {5,1}, {6,1}, {7,1}, /* 72 - 79 */
	{0,2}, {1,2}, {2,2}, {3,2}, {4,2}, {5,2}, {6,2}, {7,2}, /* 80 - 87 */
	{0,3}, {1,3}, {2,3}, {0,0}, {8,8}, {8,8}, {3,3}, {8,8}, /* 88 - 95 */
	{1,6}, {1,0}, {2,0}, {3,0}, {4,0}, {5,0}, {6,0}, {7,0}, /* 96 - 103 */
	{0,1}, {1,1}, {2,1}, {3,1}, {4,1}, {5,1}, {6,1}, {7,1}, /* 104 - 111 */
	{0,2}, {1,2}, {2,2}, {3,2}, {4,2}, {5,2}, {6,2}, {7,2}, /* 112 - 119 */
	{0,3}, {1,3}, {2,3}, {8,8}, {8,8}, {8,8}, {8,8}, {5,3}, /* 120 - 127 */
};
Keymap keymap;  /* active keymap */

static unsigned int unicode_to_dragon[128] = {
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
unsigned int keyboard_column[9];
unsigned int keyboard_row[9];

unsigned int keyboard_buffer[256];
int keyboard_buffer_next, keyboard_buffer_cursor;

static void keyboard_press_queued(void);
static void keyboard_release_queued(void);
static event_t queue_event;

void keyboard_init(void) {
	unsigned int i;
	event_init(&queue_event);
	keyboard_buffer_next = keyboard_buffer_cursor = 0;
	for (i = 0; i < 8; i++) {
		keyboard_column[i] = ~0;
		keyboard_row[i] = ~0;
	}
}

void keyboard_set_keymap(int map) {
	map %= NUM_KEYMAPS;
	running_config.keymap = map;
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

void keyboard_column_update(void) {
	unsigned int mask = PIA0.b.port_output;
	unsigned int i, row = 0x7f;
	for (i = 0; i < 8; i++) {
		if (!(mask & (1 << i))) {
			row &= keyboard_column[i];
		}
	}
	PIA0.a.port_input = (PIA0.a.port_input & 0x80) | row;
}

void keyboard_row_update(void) {
	unsigned int mask = PIA0.a.port_output;
	unsigned int i, col = 0xff;
	for (i = 0; i < 7; i++) {
		if (!(mask & (1 << i))) {
			col &= keyboard_row[i];
		}
	}
	PIA0.b.port_input = col;
}

void keyboard_queue_string(const char *s) {
	unsigned int c;
	while ( (c = *(s++)) ) {
		KEYBOARD_QUEUE(c);
	}
	if (!queue_event.queued) {
		queue_event.dispatch = keyboard_press_queued;
		queue_event.at_cycle = current_cycle + OSCILLATOR_RATE / 20;
		event_queue(&MACHINE_EVENT_LIST, &queue_event);
	}
}

void keyboard_queue(unsigned int c) {
	KEYBOARD_QUEUE(c);
	if (!queue_event.queued) {
		queue_event.dispatch = keyboard_press_queued;
		queue_event.at_cycle = current_cycle + OSCILLATOR_RATE / 20;
		event_queue(&MACHINE_EVENT_LIST, &queue_event);
	}
}

void keyboard_unicode_press(unsigned int unicode) {
	if (unicode == '\\') {
		/* CoCo and Dragon 64 in 64K mode have a different way
		 * of scanning for '\' */
		if (IS_COCO_KEYMAP || (IS_DRAGON64 && !(PIA1.b.port_output & 0x04))) {
			KEYBOARD_PRESS(0);
			KEYBOARD_PRESS(12);
		} else {
			KEYBOARD_PRESS(0);
			KEYBOARD_PRESS(12);
			KEYBOARD_PRESS('/');
		}
	} else if (unicode == 163) {
		/* Pound sign */
		KEYBOARD_PRESS(0);
		KEYBOARD_PRESS('3');
	} else if (unicode < 128) {
		unsigned int code = unicode_to_dragon[unicode];
		if (code & 128)
			KEYBOARD_PRESS(0);
		else
			KEYBOARD_RELEASE(0);
		KEYBOARD_PRESS(code & 0x7f);
	}
	keyboard_column_update();
	keyboard_row_update();
}

void keyboard_unicode_release(unsigned int unicode) {
	if (unicode == '\\') {
		/* CoCo and Dragon 64 in 64K mode have a different way
		 * of scanning for '\' */
		if (IS_COCO_KEYMAP || (IS_DRAGON64 && !(PIA1.b.port_output & 0x04))) {
			KEYBOARD_RELEASE(0);
			KEYBOARD_RELEASE(12);
		} else {
			KEYBOARD_RELEASE(0);
			KEYBOARD_RELEASE(12);
			KEYBOARD_RELEASE('/');
		}
	} else if (unicode == 163) {
		/* Pound sign */
		KEYBOARD_RELEASE(0);
		KEYBOARD_RELEASE('3');
	} else if (unicode < 128) {
		unsigned int code = unicode_to_dragon[unicode];
		if (code & 128)
			KEYBOARD_RELEASE(0);
		KEYBOARD_RELEASE(code & 0x7f);
	}
	keyboard_column_update();
	keyboard_row_update();
}

static unsigned int key_pressed;

static void keyboard_press_queued(void) {
	if (KEYBOARD_HASQUEUE) {
		key_pressed = keyboard_buffer[keyboard_buffer_cursor];
		if (key_pressed) {
			keyboard_unicode_press(key_pressed);
			/* Schedule key release event */
			queue_event.dispatch = keyboard_release_queued;
			queue_event.at_cycle += OSCILLATOR_RATE / 20;
			event_queue(&MACHINE_EVENT_LIST, &queue_event);
		}
	}
}

static void keyboard_release_queued(void) {
	keyboard_unicode_release(key_pressed);
	KEYBOARD_DEQUEUE();
	/* Schedule another key press event only if queue not empty */
	if (KEYBOARD_HASQUEUE) {
		queue_event.dispatch = keyboard_press_queued;
		queue_event.at_cycle += OSCILLATOR_RATE / 20;
		event_queue(&MACHINE_EVENT_LIST, &queue_event);
	}
}
