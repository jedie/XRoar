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

#include <string.h>

#include "machine.h"
#include "sam.h"
#include "pia.h"
#include "wd2797.h"
#include "m6809.h"
#include "vdg.h"
#include "tape.h"
#include "keyboard.h"
#include "logging.h"
#include "fs.h"
#include "types.h"

/* machine_romtype indicates which ROM to try and load (Dragon or Coco),
 * and which cassette breakpoints to set (at the moment, this is also used
 * to choose between some hardware configs).  machine_keymap tracks which
 * keymap is in use (good to be able to set it separately). */
int machine_romtype, machine_keymap;
int dragondos_enabled;
int_fast32_t brk_csrdon, brk_bitin;
Keymap keymap;
uint8_t ram0[0x8000];
uint8_t ram1[0x8000];
uint8_t rom0[0x8000];
uint8_t rom1[0x8000];

static Keymap dragon_keymap = {
	{7,6}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, /* 0 - 7 */
	{5,5}, {6,5}, {4,5}, {3,5}, {1,6}, {0,6}, {8,8}, {8,8}, /* 8 - 15 */
	{8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, /* 16 - 23 */
	{8,8}, {8,8}, {8,8}, {2,6}, {8,8}, {8,8}, {8,8}, {8,8}, /* 24 - 31 */
	{7,5}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, /* 32 - 39 */
	{8,8}, {8,8}, {8,8}, {8,8}, {4,1}, {5,1}, {6,1}, {7,1}, /* 40 - 47 */
	{0,0}, {1,0}, {2,0}, {3,0}, {4,0}, {5,0}, {6,0}, {7,0}, /* 48 - 55 */
	{0,1}, {1,1}, {2,1}, {3,1}, {8,8}, {8,8}, {8,8}, {8,8}, /* 56 - 63 */
	{0,2}, {1,2}, {2,2}, {3,2}, {4,2}, {5,2}, {6,2}, {7,2}, /* 64 - 71 */
	{0,3}, {1,3}, {2,3}, {3,3}, {4,3}, {5,3}, {6,3}, {7,3}, /* 72 - 79 */
	{0,4}, {1,4}, {2,4}, {3,4}, {4,4}, {5,4}, {6,4}, {7,4}, /* 80 - 87 */
	{0,5}, {1,5}, {2,5}, {8,8}, {2,6}, {8,8}, {3,5}, {8,8}, /* 88 - 95 */
	{1,6}, {1,2}, {2,2}, {3,2}, {4,2}, {5,2}, {6,2}, {7,2}, /* 96 - 103 */
	{0,3}, {1,3}, {2,3}, {3,3}, {4,3}, {5,3}, {6,3}, {7,3}, /* 104 - 111 */
	{0,4}, {1,4}, {2,4}, {3,4}, {4,4}, {5,4}, {6,4}, {7,4}, /* 112 - 119 */
	{0,5}, {1,5}, {2,5}, {8,8}, {8,8}, {8,8}, {8,8}, {1,6}, /* 120 - 127 */
};
static Keymap coco_keymap = {
	{7,6}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, /* 0 - 7 */
	{5,3}, {6,3}, {4,3}, {3,3}, {1,6}, {0,6}, {8,8}, {8,8}, /* 8 - 15 */
	{8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, /* 16 - 23 */
	{8,8}, {8,8}, {8,8}, {2,6}, {8,8}, {8,8}, {8,8}, {8,8}, /* 24 - 31 */
	{7,3}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, /* 32 - 39 */
	{8,8}, {8,8}, {8,8}, {8,8}, {4,5}, {2,5}, {6,5}, {7,5}, /* 40 - 47 */
	{0,4}, {1,4}, {2,4}, {3,4}, {4,4}, {5,4}, {6,4}, {7,4}, /* 48 - 55 */
	{0,5}, {1,5}, {8,8}, {3,5}, {8,8}, {5,5}, {8,8}, {8,8}, /* 56 - 63 */
	//{8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, /* 64 - 71 */
	//{8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, /* 72 - 79 */
	//{8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, {8,8}, /* 80 - 87 */
	//{8,8}, {8,8}, {8,8}, {0,0}, {2,6}, {8,8}, {5,3}, {8,8}, /* 88 - 95 */
	{0,0}, {1,0}, {2,0}, {3,0}, {4,0}, {5,0}, {6,0}, {7,0}, /* 64 - 71 */
	{0,1}, {1,1}, {2,1}, {3,1}, {4,1}, {5,1}, {6,1}, {7,1}, /* 72 - 79 */
	{0,2}, {1,2}, {2,2}, {3,2}, {4,2}, {5,2}, {6,2}, {7,2}, /* 80 - 87 */
	{0,3}, {1,3}, {2,3}, {0,0}, {2,6}, {8,8}, {3,3}, {8,8}, /* 88 - 95 */
	{1,6}, {1,0}, {2,0}, {3,0}, {4,0}, {5,0}, {6,0}, {7,0}, /* 96 - 103 */
	{0,1}, {1,1}, {2,1}, {3,1}, {4,1}, {5,1}, {6,1}, {7,1}, /* 104 - 111 */
	{0,2}, {1,2}, {2,2}, {3,2}, {4,2}, {5,2}, {6,2}, {7,2}, /* 112 - 119 */
	{0,3}, {1,3}, {2,3}, {8,8}, {8,8}, {8,8}, {8,8}, {1,6}, /* 120 - 127 */
};

const char *rom_names[3][4] = {
	{ "d64rom1.dgn", "d64rom1.rom", "dragrom.dgn", "dragon.rom" },
	{ "coco.rom", "coco_pa.rom", "cocodisk.rom", "coco.dgn" },
	{ "dragon32.rom", "d32rom.dgn", "dragon.rom", "dragrom.dgn" },
};
const char *d64_rom2_names[4] =
	{ "d64rom2.rom", "d64rom2.dgn", "d64_2.rom", "d64_2.dgn" };

static int load_rom(const char *filename, uint8_t *dest, size_t max_size);

void machine_init(void) {
	machine_set_romtype(DRAGON64);
	machine_set_keymap(DRAGON_KEYBOARD);
	dragondos_enabled = 1;
	sam_init();
	pia_init();
	wd2797_init();
	m6809_init();
	vdg_init();
	tape_init();
}

void machine_reset(int hard) {
	uint_fast16_t romsize;
	uint_fast8_t i;
	if (hard) {
		memset(ram0, 0, sizeof(ram0));
		memset(ram1, 0, sizeof(ram1));
		memset(rom0, 0, sizeof(rom0));
		memset(rom1, 0, sizeof(rom1));
		if (machine_romtype == COCO) {
			brk_csrdon = 0xa77c; brk_bitin = 0xa755;
			dragondos_enabled = 0;
		} else {
			brk_csrdon = 0xbde7; brk_bitin = 0xbda5;
		}
		romsize = dragondos_enabled ? sizeof(rom0) : 0x4000;
		for (i=0; load_rom(rom_names[machine_romtype][i], rom0, romsize) && i<4; i++);
		if (IS_DRAGON64) {
			for (i=0; load_rom(d64_rom2_names[i], rom1, 0x4000) && i<4; i++);
			memcpy(rom0+0x4000, rom1+0x4000, 0x4000);
		}
	}
	pia_reset();
	if (IS_DRAGON64) {
		PIA_1B.port_input |= (1<<2);
		PIA_UPDATE_OUTPUT(PIA_1B);
	}
	wd2797_reset();
	sam_reset();
	m6809_reset();
	vdg_reset();
	tape_reset();
}

/* Setting romtype takes effect on next reset */
void machine_set_romtype(int mode) {
	if (mode >= NUM_MACHINES) mode = 0;
	machine_romtype = mode;
}

/* Setting keymap takes effect immediately */
void machine_set_keymap(int mode) {
	if (mode >= NUM_KEYBOARDS) mode = 0;
	machine_keymap = mode;
	switch (mode) {
		default:
		case DRAGON_KEYBOARD:
			memcpy(keymap, dragon_keymap, sizeof(keymap));
			break;
		case COCO_KEYBOARD:
			memcpy(keymap, coco_keymap, sizeof(keymap));
			break;
	}
}

static int load_rom(const char *filename, uint8_t *dest, size_t max_size) {
	FS_FILE fd;
	if ((fd = fs_open(filename, FS_READ)) == -1)
		return -1;
	if (fs_read(fd, dest, 16) < 0) {
		fs_close(fd);
		return -1;
	}
	fprintf(stderr, "Loading %s\n", filename);
	if (dest[0] == 0xa1) {
		fs_read(fd, dest, max_size);
	} else {
		fs_read(fd, dest+16, max_size-16);
	}
	fs_close(fd);
	return 0;
}
