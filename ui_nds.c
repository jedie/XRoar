/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2006  Ciaran Anscomb
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <nds.h>
#include <fat.h>

#include "types.h"
#include "logging.h"
#include "module.h"
#include "nds/ndsgfx.h"

static int init(int argc, char **argv);
static void shutdown(void);

extern VideoModule video_nds_module;
static VideoModule *nds_video_module_list[] = {
	&video_nds_module,
	NULL
};

extern KeyboardModule keyboard_nds_module;
static KeyboardModule *nds_keyboard_module_list[] = {
	&keyboard_nds_module,
	NULL
};

UIModule ui_nds_module = {
	{ "nds", "SDL user-interface",
	  init, 0, shutdown, NULL },
	NULL,  /* use default filereq module list */
	nds_video_module_list,
	NULL,  /* use default sound module list */
	nds_keyboard_module_list,
	NULL  /* use default joystick module list */
};

extern Sprite kbd_bin[2];

static int init(int argc, char **argv) {
	(void)argc;
	(void)argv;
	powerSET(POWER_ALL_2D|POWER_SWAP_LCDS);
	// Use the main screen for output
	/*
	videoSetModeSub(MODE_0_2D | DISPLAY_BG0_ACTIVE);
	vramSetBankC(VRAM_C_SUB_BG);
	SUB_BG0_CR = BG_MAP_BASE(31);
	// Set the colour of the font to White.
	BG_PALETTE_SUB[255] = RGB15(31,31,31);
	consoleInitDefault((uint16_t *)SCREEN_BASE_BLOCK_SUB(31), (uint16_t *)CHAR_BASE_BLOCK_SUB(0), 16);
	*/
	if (!fatInitDefault()) {
		LOG_ERROR("fatInitDefault() failed.\n");
	}
	ndsgfx_init();
	ndsgfx_fillrect(0, 0, 256, 192, 0x00000000);
	ndsgfx_blit(13, 110, &kbd_bin[0]);
	return 0;
}

static void shutdown(void) {
}
