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

#include "config.h"

#ifdef HAVE_GP32

#include <string.h>
#include <gpdef.h>
#include <gpstdlib.h>
#include <gpgraphic.h>
#include <gpstdio.h>
#include <gpfont.h>
#include "gp32/gp32.h"
#include "gp32/gpkeypad.h"
#include "gp32/gpchatboard.h"

#include "types.h"
#include "xroar.h"
#include "pia.h"
#include "keyboard.h"
#include "snapshot.h"
#include "logging.h"
#include "video.h"
#include "joystick.h"
#include "ui.h"

static int init(void);
static void shutdown(void);
static void poll(void);

KeyboardModule keyboard_gp32_module = {
	NULL,
	"gp32",
	"GP32 virtual keyboard driver",
	init, shutdown,
	poll
};

#define KEY_PRESSED(s) { \
		keyboard_column[keymap[s].col] &= ~(1<<keymap[s].row); \
		keyboard_row[keymap[s].row] &= ~(1<<keymap[s].col); \
	}
#define KEY_RELEASED(s) { \
		keyboard_column[keymap[s].col] |= 1<<keymap[s].row; \
		keyboard_row[keymap[s].row] |= 1<<keymap[s].col; \
	}
#define KEY_UPDATE(t,s) if (t) { KEY_PRESSED(s); } else { KEY_RELEASED(s); }

#define KEYBOARD_OFFSET (64*240)

extern GPDRAWSURFACE screen;
extern Sprite kbd_bin[2];
extern Sprite cmode_bin[4];

/* This struct and array define where keys are displayed, and what keysym they
 * correspond to. */

typedef struct {
	uint_fast16_t xoffset, width;
	uint_fast8_t sym;
} keydata;

static keydata keys[5][14] = {
	{ {0,0,0}, {0,0,0}, {23,7,32}, {31,7,32}, {39,7,32}, {47,7,32}, {55,7,32}, {63,7,32}, {71,7,32}, {79,7,32}, {255,0,0}, {255,0,0}, {255,0,0}, {255,0,0} },
	{ {3,11,0}, {15,7,122}, {23,7,120}, {31,7,99}, {39,7,118}, {47,7,98}, {55,7,110}, {63,7,109}, {71,7,44}, {79,7,46}, {87,7,47}, {95,11,0}, {255,0,0}, {255,0,0} },
	{ {3,7,10}, {11,7,97}, {19,7,115}, {27,7,100}, {35,7,102}, {43,7,103}, {51,7,104}, {59,7,106}, {67,7,107}, {75,7,108}, {83,7,59}, {91,15,13}, {107,7,12}, {255,0,0} },
	{ {1,7,11}, {9,7,113}, {17,7,119}, {25,7,101}, {33,7,114}, {41,7,116}, {49,7,121}, {57,7,117}, {65,7,105}, {73,7,111}, {81,7,112}, {89,7,91}, {97,7,8}, {105,7,9} },
	{ {0,0,0}, {5,7,49}, {13,7,50}, {21,7,51}, {29,7,52}, {37,7,53}, {45,7,54}, {53,7,55}, {61,7,56}, {69,7,57}, {77,7,48}, {85,7,45}, {93,7,61}, {101,7,27} }
};

static uint_fast8_t keyx = 1, keyy = 4;
static keydata *current;
//static uint32_t oldkey = ~0;
static uint_fast8_t keyboard_mode;
static uint_fast8_t displayed_keyboard;

static void highlight_key(void);

static int init(void) {
	video_module->blit(64, 200, &kbd_bin[0]);
	keyboard_mode = 0;
	video_module->blit(8, 200, &cmode_bin[keyboard_mode]);
	displayed_keyboard = 0;
	keyx = 1; keyy = 4;
	current = &keys[keyy][keyx];
	highlight_key();
	rPBCON = 0x0;
	gpkeypad_init();
	gpkeypad_repeat_rate(225);
	gpchatboard_init();
	return 0;
}

static void shutdown(void) {
	gpchatboard_shutdown();
}

static void highlight_key(void) {
	uint_fast16_t i;
	uint8_t *p = (uint8_t *)screen.ptbuffer + KEYBOARD_OFFSET
		+ current->xoffset*240 + keyy * 8;
	for (i = 0; i < current->width; i++) {
		*(p++) ^= ~0; *(p++) ^= ~0; *(p++) ^= ~0; *(p++) ^= ~0;
		*(p++) ^= ~0; *(p++) ^= ~0; *(p++) ^= ~0;
		p += 233;
	}
}

static void poll(void) {
	uint_fast8_t newkeyx = keyx, newkeyy = keyy;
	int key, newkey, rkey;
	gpkeypad_poll(&key, &newkey, &rkey);
	if (newkey & GPC_VK_FL) {
		keyboard_mode++;
		if (keyboard_mode > 3)
			keyboard_mode = 0;
		video_module->blit(8, 200, &cmode_bin[keyboard_mode]);
	}
	if ((key & (GPC_VK_FR|GPC_VK_FL)) == (GPC_VK_FR|GPC_VK_FL))
		xroar_reset(RESET_HARD);  /* hard reset machine */
	if (newkey & GPC_VK_START)
		ui_module->menu();
	switch (keyboard_mode) {
		case 3:
			joystick_rightx = joystick_righty = 255;
			joystick_leftx = (joystick_leftx < 128) ? 127 : 128;
			joystick_lefty = (joystick_lefty < 128) ? 127 : 128;
			if (key & GPC_VK_LEFT) joystick_leftx = 0;
			if (key & GPC_VK_RIGHT) joystick_leftx = 255;
			if (key & GPC_VK_UP) joystick_lefty = 0;
			if (key & GPC_VK_DOWN) joystick_lefty = 255;
			if (key & GPC_VK_FB)
				PIA_0A.tied_low &= 0xfd;
			else
				PIA_0A.tied_low |= 0x02;
			KEY_UPDATE(key & GPC_VK_FR, 13);
			KEY_UPDATE(key & GPC_VK_FA, 32);
			KEY_UPDATE(key & GPC_VK_SELECT, 112);
			break;
		case 2:
			joystick_leftx = joystick_lefty = 255;
			joystick_rightx = (joystick_rightx < 128) ? 127 : 128;
			joystick_righty = (joystick_righty < 128) ? 127 : 128;
			if (key & GPC_VK_LEFT) joystick_rightx = 0;
			if (key & GPC_VK_RIGHT) joystick_rightx = 255;
			if (key & GPC_VK_UP) joystick_righty = 0;
			if (key & GPC_VK_DOWN) joystick_righty = 255;
			if (key & GPC_VK_FB)
				PIA_0A.tied_low &= 0xfe;
			else
				PIA_0A.tied_low |= 0x01;
			KEY_UPDATE(key & GPC_VK_FR, 13);
			KEY_UPDATE(key & GPC_VK_FA, 32);
			KEY_UPDATE(key & GPC_VK_SELECT, 112);
			break;
		case 1:
			KEY_UPDATE(key & GPC_VK_FR, 13);
			KEY_UPDATE(key & GPC_VK_FB, 0);
			KEY_UPDATE(key & GPC_VK_UP, 11);
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
				video_module->blit(64, 200, &kbd_bin[displayed_keyboard]);
				highlight_key();
			}
			if (rkey & GPC_VK_UP && keyy < 4)
				newkeyy = keyy + 1;
			if (rkey & GPC_VK_DOWN && keyy > 0)
				newkeyy = keyy - 1;
			if (rkey & GPC_VK_LEFT && keyx > 0)
				newkeyx = keyx - 1;
			if (rkey & GPC_VK_RIGHT && keyx < 13)
				newkeyx = keyx + 1;
			while (keys[newkeyy][newkeyx].xoffset == 0)
				newkeyx++;
			while (keys[newkeyy][newkeyx].xoffset == 255)
				newkeyx--;
			if (newkeyx != keyx || newkeyy != keyy) {
				highlight_key();
				KEY_RELEASED(current->sym);
				keyx = newkeyx; keyy = newkeyy;
				current = &keys[keyy][keyx];
				highlight_key();
			}
			KEY_UPDATE(key & GPC_VK_FB, current->sym);
			break;
	}
	/* Poll chatboard - doesn't matter if it's not actually there */
	{
		unsigned char chatboard_key = gpchatboard_scan();
		if (chatboard_key) {
			KEYBOARD_QUEUE(chatboard_key);
		}
	}
}

#endif  /* HAVE_GP32 */
