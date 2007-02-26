/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2007  Ciaran Anscomb
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
#include <nds.h>
#include "nds/ndsgfx.h"

#include "types.h"
#include "logging.h"
#include "events.h"
#include "joystick.h"
#include "keyboard.h"
#include "module.h"
#include "pia.h"
#include "snapshot.h"
#include "xroar.h"

static int init(int argc, char **argv);
static void shutdown(void);

KeyboardModule keyboard_nds_module = {
	{ "nds", "NDS virtual keyboard driver",
	  init, 0, shutdown, NULL }
};

static event_t *poll_event;
static void do_poll(void);

#define KEY_UPDATE(t,s) if (t) { KEYBOARD_PRESS(s); } else { KEYBOARD_RELEASE(s); }
#define KEYBOARD_X (13)
#define KEYBOARD_Y (110)


extern Sprite kbd_bin[2];

/* This struct and array define where keys are displayed, and what keysym they
 * correspond to. */

typedef struct {
	int xoffset, width;
	int sym;
} keydata;

static keydata keys[5][14] = {
	{ {5,7,49}, {13,7,50}, {21,7,51}, {29,7,52}, {37,7,53}, {45,7,54}, {53,7,55}, {61,7,56}, {69,7,57}, {77,7,48}, {85,7,58}, {93,7,45}, {101,7,27}, {255,0,0} },
	{ {1,7,94}, {9,7,113}, {17,7,119}, {25,7,101}, {33,7,114}, {41,7,116}, {49,7,121}, {57,7,117}, {65,7,105}, {73,7,111}, {81,7,112}, {89,7,64}, {97,7,8}, {105,7,9} },
	{ {3,7,10}, {11,7,97}, {19,7,115}, {27,7,100}, {35,7,102}, {43,7,103}, {51,7,104}, {59,7,106}, {67,7,107}, {75,7,108}, {83,7,59}, {91,15,13}, {107,7,12}, {255,0,0} },
	{ {3,11,0}, {15,7,122}, {23,7,120}, {31,7,99}, {39,7,118}, {47,7,98}, {55,7,110}, {63,7,109}, {71,7,44}, {79,7,46}, {87,7,47}, {95,11,0}, {255,0,0}, {255,0,0} },
	{ {23,63,32}, {255,0,0}, {255,0,0}, {255,0,0}, {255,0,0}, {255,0,0}, {255,0,0}, {255,0,0}, {255,0,0}, {255,0,0}, {255,0,0}, {255,0,0}, {255,0,0}, {255,0,0} }
};

static int keyy;
static keydata *current;
static unsigned int displayed_keyboard;

static void highlight_key(void);

static int init(int argc, char **argv) {
	(void)argc;
	(void)argv;
	ndsgfx_blit(KEYBOARD_X, KEYBOARD_Y, &kbd_bin[0]);
	keyy = -1;
	current = NULL;
	displayed_keyboard = 0;
	poll_event = event_new();
	poll_event->dispatch = do_poll;
	poll_event->at_cycle = current_cycle + (OSCILLATOR_RATE / 100);
	event_queue(poll_event);
	return 0;
}

static void shutdown(void) {
	event_free(poll_event);
}

static void highlight_key(void) {
	int x = KEYBOARD_X + (current->xoffset * 2);
	int y = KEYBOARD_Y + (keyy * 16) + 2;
	int w = (current->width * 2) + 1;
	uint16_t *p = (uint16_t *)BG_GFX_SUB + (y*256) + x;
	int skip = 256 - w;
	int i, j;
	for (j = 15; j; j--) {
		for (i = w; i; i--) {
			*(p++) ^= 0x7fff;
		}
		p += skip;
	}
}

static void do_poll(void) {
	uint16_t px, py;
	while (IPC->mailBusy);
	px = IPC->touchXpx;
	py = IPC->touchYpx;
	if (px == 0 || py == 0) {
		if (current) {
			if (current->sym != 0) {
				highlight_key();
				KEYBOARD_RELEASE(current->sym);
			}
			current = NULL;
		}
	} else {
		if (current == NULL && py >= (KEYBOARD_Y+2) && py <= (KEYBOARD_Y+81)) {
			int i;
			keyy = (py - (KEYBOARD_Y+2)) >> 4;
			for (i = 0; i < 14; i++) {
				if (keys[keyy][i].width > 0 && px >= (KEYBOARD_X + (keys[keyy][i].xoffset*2)) && px <= ((KEYBOARD_X + (keys[keyy][i].xoffset*2) + (keys[keyy][i].width*2)))) {
					current = &keys[keyy][i];
				}
			}
			if (current) {
				if (current->sym == 0) {
					displayed_keyboard ^= 1;
					ndsgfx_blit(KEYBOARD_X, KEYBOARD_Y, &kbd_bin[displayed_keyboard]);
					KEY_UPDATE(displayed_keyboard, 0);
				} else {
					highlight_key();
					KEYBOARD_PRESS(current->sym);
				}
			}
		}
	}
	poll_event->at_cycle += OSCILLATOR_RATE / 100;
	event_queue(poll_event);
}
