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

#include <string.h>
#include <gpdef.h>
#include <gpstdlib.h>
#include <gpgraphic.h>
#include <gpstdio.h>
#include <gpfont.h>
#include "gp32/gp32.h"
#include "gp32/gpgfx.h"
#include "gp32/gpkeypad.h"
#include "gp32/gpchatboard.h"

#include "types.h"
#include "logging.h"
#include "events.h"
#include "input.h"
#include "joystick.h"
#include "keyboard.h"
#include "machine.h"
#include "mc6821.h"
#include "module.h"
#include "snapshot.h"
#include "xroar.h"

void gp32_menu(void);

static int init(void);
static void shutdown(void);

KeyboardModule keyboard_gp32_module = {
	{ "gp32", "GP32 virtual keyboard driver",
	  init, 0, shutdown }
};

static event_t poll_event;
static void do_poll(void);

#define KEY_UPDATE(t,s) if (t) { KEYBOARD_PRESS(s); } else { KEYBOARD_RELEASE(s); }

#define KEYBOARD_OFFSET (64*240)

extern Sprite kbd_bin[2];
extern Sprite cmode_bin[4];

/* This struct and array define where keys are displayed, and what keysym they
 * correspond to. */

typedef struct {
	unsigned int xoffset, width;
	unsigned int sym;
} keydata;

static keydata keys[5][14] = {
	{ {0,0,0}, {0,0,0}, {23,7,32}, {31,7,32}, {39,7,32}, {47,7,32}, {55,7,32}, {63,7,32}, {71,7,32}, {79,7,32}, {255,0,0}, {255,0,0}, {255,0,0}, {255,0,0} },
	{ {3,11,0}, {15,7,122}, {23,7,120}, {31,7,99}, {39,7,118}, {47,7,98}, {55,7,110}, {63,7,109}, {71,7,44}, {79,7,46}, {87,7,47}, {95,11,0}, {255,0,0}, {255,0,0} },
	{ {3,7,10}, {11,7,97}, {19,7,115}, {27,7,100}, {35,7,102}, {43,7,103}, {51,7,104}, {59,7,106}, {67,7,107}, {75,7,108}, {83,7,59}, {91,15,13}, {107,7,12}, {255,0,0} },
	{ {1,7,94}, {9,7,113}, {17,7,119}, {25,7,101}, {33,7,114}, {41,7,116}, {49,7,121}, {57,7,117}, {65,7,105}, {73,7,111}, {81,7,112}, {89,7,64}, {97,7,8}, {105,7,9} },
	{ {0,0,0}, {5,7,49}, {13,7,50}, {21,7,51}, {29,7,52}, {37,7,53}, {45,7,54}, {53,7,55}, {61,7,56}, {69,7,57}, {77,7,48}, {85,7,58}, {93,7,45}, {101,7,27} }
};

static unsigned int keyx = 1, keyy = 4;
static keydata *current;
static unsigned int keyboard_mode;
static unsigned int displayed_keyboard;

static void highlight_key(void);

#define REPEAT_MASK (GPC_VK_UP | GPC_VK_DOWN | GPC_VK_LEFT | GPC_VK_RIGHT)

static int init(void) {
	gpgfx_blit(64, 200, &kbd_bin[0]);
	keyboard_mode = 0;
	gpgfx_blit(8, 200, &cmode_bin[keyboard_mode]);
	displayed_keyboard = 0;
	keyx = 1; keyy = 4;
	current = &keys[keyy][keyx];
	highlight_key();
	rPBCON = 0x0;
	gpkeypad_init();
	gpkeypad_set_repeat(REPEAT_MASK, 225);
	gpchatboard_init();
	event_init(&poll_event);
	poll_event.dispatch = do_poll;
	poll_event.at_cycle = current_cycle + (OSCILLATOR_RATE / 100);
	event_queue(&UI_EVENT_LIST, &poll_event);
	return 0;
}

static void shutdown(void) {
	gpchatboard_shutdown();
}

static void highlight_key(void) {
	uint_least16_t i;
	uint8_t *p = (uint8_t *)gp_screen.ptbuffer + KEYBOARD_OFFSET
		+ current->xoffset*240 + keyy * 8;
	for (i = 0; i < current->width; i++) {
		*(p++) ^= ~0; *(p++) ^= ~0; *(p++) ^= ~0; *(p++) ^= ~0;
		*(p++) ^= ~0; *(p++) ^= ~0; *(p++) ^= ~0;
		p += 233;
	}
}

static void do_poll(void) {
	unsigned int chatboard_key;
	unsigned int newkeyx = keyx, newkeyy = keyy;
	int key, newkey, relkey;

	poll_event.at_cycle += OSCILLATOR_RATE / 100;
	event_queue(&UI_EVENT_LIST, &poll_event);

	/* Poll chatboard - doesn't matter if it's not actually there */
	chatboard_key = gpchatboard_scan();
	if (chatboard_key) {
		keyboard_queue(chatboard_key);
	}
	/* While there's stuff in the queue, don't do anything else */
	if (KEYBOARD_HASQUEUE)
		return;

	gpkeypad_poll(&key, &newkey, &relkey);
	if (newkey & GPC_VK_FL) {
		keyboard_mode++;
		if (keyboard_mode > 3) {
			keyboard_mode = 0;
			gpkeypad_set_repeat(REPEAT_MASK, 225);
		} else {
			gpkeypad_set_repeat(0, 225);
		}
		gpgfx_blit(8, 200, &cmode_bin[keyboard_mode]);
	}
	if ((key & (GPC_VK_FR|GPC_VK_FL)) == (GPC_VK_FR|GPC_VK_FL))
		machine_reset(RESET_HARD);  /* hard reset machine */
	if (newkey & GPC_VK_START)
		gp32_menu();
	switch (keyboard_mode) {
		case 3:
			if (newkey & GPC_VK_LEFT) input_control_press(INPUT_JOY_LEFT_X, 0);
			if (relkey & GPC_VK_LEFT) input_control_release(INPUT_JOY_LEFT_X, 0);
			if (newkey & GPC_VK_RIGHT) input_control_press(INPUT_JOY_LEFT_X, 255);
			if (relkey & GPC_VK_RIGHT) input_control_release(INPUT_JOY_LEFT_X, 255);
			if (newkey & GPC_VK_UP) input_control_press(INPUT_JOY_LEFT_Y, 0);
			if (relkey & GPC_VK_UP) input_control_release(INPUT_JOY_LEFT_Y, 0);
			if (newkey & GPC_VK_DOWN) input_control_press(INPUT_JOY_LEFT_Y, 255);
			if (relkey & GPC_VK_DOWN) input_control_release(INPUT_JOY_LEFT_Y, 255);
			if (newkey & GPC_VK_FB) input_control_press(INPUT_JOY_LEFT_FIRE, 0);
			if (relkey & GPC_VK_FB) input_control_release(INPUT_JOY_LEFT_FIRE, 0);
			KEY_UPDATE(key & GPC_VK_FR, 13);
			KEY_UPDATE(key & GPC_VK_FA, 32);
			KEY_UPDATE(key & GPC_VK_SELECT, 112);
			break;
		case 2:
			if (newkey & GPC_VK_LEFT) input_control_press(INPUT_JOY_RIGHT_X, 0);
			if (relkey & GPC_VK_LEFT) input_control_release(INPUT_JOY_RIGHT_X, 0);
			if (newkey & GPC_VK_RIGHT) input_control_press(INPUT_JOY_RIGHT_X, 255);
			if (relkey & GPC_VK_RIGHT) input_control_release(INPUT_JOY_RIGHT_X, 255);
			if (newkey & GPC_VK_UP) input_control_press(INPUT_JOY_RIGHT_Y, 0);
			if (relkey & GPC_VK_UP) input_control_release(INPUT_JOY_RIGHT_Y, 0);
			if (newkey & GPC_VK_DOWN) input_control_press(INPUT_JOY_RIGHT_Y, 255);
			if (relkey & GPC_VK_DOWN) input_control_release(INPUT_JOY_RIGHT_Y, 255);
			if (newkey & GPC_VK_FB) input_control_press(INPUT_JOY_RIGHT_FIRE, 0);
			if (relkey & GPC_VK_FB) input_control_release(INPUT_JOY_RIGHT_FIRE, 0);
			KEY_UPDATE(key & GPC_VK_FR, 13);
			KEY_UPDATE(key & GPC_VK_FA, 32);
			KEY_UPDATE(key & GPC_VK_SELECT, 112);
			break;
		case 1:
			KEY_UPDATE(key & GPC_VK_FR, 13);
			KEY_UPDATE(key & GPC_VK_FB, 0);
			KEY_UPDATE(key & GPC_VK_UP, 94);
			KEY_UPDATE(key & GPC_VK_DOWN, 10);
			KEY_UPDATE(key & GPC_VK_LEFT, 8);
			KEY_UPDATE(key & GPC_VK_RIGHT, 9);
			KEY_UPDATE(key & GPC_VK_FA, 32);
			KEY_UPDATE(key & GPC_VK_SELECT, 100);
			break;
		default:
		case 0:
			KEY_UPDATE(key & GPC_VK_FR, 0);
			if (((key&GPC_VK_FR)!=0)^displayed_keyboard) {
				displayed_keyboard ^= 1;
				gpgfx_blit(64, 200, &kbd_bin[displayed_keyboard]);
				highlight_key();
			}
			if (newkey & GPC_VK_UP && keyy < 4)
				newkeyy = keyy + 1;
			if (newkey & GPC_VK_DOWN && keyy > 0)
				newkeyy = keyy - 1;
			if (newkey & GPC_VK_LEFT && keyx > 0)
				newkeyx = keyx - 1;
			if (newkey & GPC_VK_RIGHT && keyx < 13)
				newkeyx = keyx + 1;
			while (keys[newkeyy][newkeyx].xoffset == 0)
				newkeyx++;
			while (keys[newkeyy][newkeyx].xoffset == 255)
				newkeyx--;
			if (newkeyx != keyx || newkeyy != keyy) {
				highlight_key();
				KEYBOARD_RELEASE(current->sym);
				keyx = newkeyx; keyy = newkeyy;
				current = &keys[keyy][keyx];
				highlight_key();
			}
			KEY_UPDATE(key & GPC_VK_FB, current->sym);
			break;
	}
}
