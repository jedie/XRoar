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

#include "types.h"
#include "cart.h"
#include "fs.h"
#include "keyboard.h"
#include "logging.h"
#include "m6809.h"
#include "machine.h"
#include "pia.h"
#include "sam.h"
#include "tape.h"
#include "vdg.h"
#include "vdrive.h"
#include "wd2797.h"
#include "xroar.h"

/* machine_keymap tracks which keymap is in use (good to be able to set it
 * separately). */
unsigned int machine_keymap;
int dragondos_enabled;
Keymap keymap;
uint8_t ram0[0x8000];
uint8_t ram1[0x8000];
uint8_t rom0[0x8000];
uint8_t rom1[0x8000];

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

/* The first ROM in each of these lists is NULL so it can be overwritten
   by switches */

machine_info machines[NUM_MACHINES] = {
	{ "dragon32", "Dragon 32", DRAGON_KEYBOARD, PAL,
		{ NULL, NULL, NULL, NULL, NULL },
		{ NULL, "d32", "dragon32", "d32rom", "dragon" },
		{ NULL, "sdose6", "ddos10", NULL, NULL },
		{ NULL, NULL, NULL, NULL, NULL }
	},
	{ "dragon64", "Dragon 64", DRAGON_KEYBOARD, PAL,
		{ NULL, NULL, NULL, NULL, NULL },
		{ NULL, "d64_1", "d64rom1", "dragrom", "dragon" },
		{ NULL, "sdose6", "ddos10", NULL, NULL },
		{ NULL, "d64_2", "d64rom2", NULL, NULL }
	},
	{ "tano", "Tano Dragon", DRAGON_KEYBOARD, NTSC,
		{ NULL, NULL, NULL, NULL, NULL },
		{ NULL, "d64_1", "d64rom1", "dragrom", "dragon" },
		{ NULL, "sdose6", "ddos10", NULL, NULL },
		{ NULL, "d64_2", "d64rom2", NULL, NULL }
	},
	{ "coco", "Tandy CoCo", COCO_KEYBOARD, NTSC,
		{ NULL, "bas13", "bas12", "bas11", "bas10" },
		{ NULL, "extbas11", "extbas10", NULL, NULL },
		{ NULL, "disk11", "disk10", NULL, NULL },
		{ NULL, NULL, NULL, NULL, NULL }
	}
};
unsigned int machine_romtype;

int noextbas;

static int load_rom(const char *romname, uint8_t *dest, size_t max_size);

void machine_helptext(void) {
	puts("  -machine MACHINE      emulated machine [dragon32|dragon64|tano|coco]");
	puts("  -bas FILENAME         specify BASIC ROM to use (CoCo only)");
	puts("  -extbas FILENAME      specify Extended BASIC ROM to use");
	puts("  -noextbas             disable Extended BASIC");
	puts("  -dos FILENAME         specify DOS ROM (or CoCo Disk BASIC)");
	puts("  -nodos                disable DOS (ROM and hardware emulation)");
#ifdef TRACE
	puts("  -trace                start with trace mode on");
#endif
}

void machine_getargs(int argc, char **argv) {
	int i, j;
	machine_romtype = DRAGON64;
	machine_set_keymap(DRAGON_KEYBOARD);
	noextbas = 0;
	dragondos_enabled = 1;
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-machine")) {
			i++;
			if (i >= argc) break;
			if (!strcmp(argv[i], "help")) {
				for (j = 0; j < NUM_MACHINES; j++) {
					printf("\t%-10s%s\n", machines[j].name, machines[j].description);
				}
				exit(0);
			}
			for (j = 0; j < NUM_MACHINES; j++) {
				if (!strcmp(argv[i], machines[j].name)) {
					machine_romtype = j;
					machine_set_keymap(machines[j].keymap);
				}
			}
		} else if (!strcmp(argv[i], "-bas")) {
			i++;
			if (i >= argc) break;
			machines[machine_romtype].bas[0] = argv[i];
		} else if (!strcmp(argv[i], "-extbas")) {
			i++;
			if (i >= argc) break;
			machines[machine_romtype].extbas[0] = argv[i];
		} else if (!strcmp(argv[i], "-noextbas")) {
			noextbas = 1;
		} else if (!strcmp(argv[i], "-dos")) {
			i++;
			if (i >= argc) break;
			machines[machine_romtype].dos[0] = argv[i];
		} else if (!strcmp(argv[i], "-nodos")) {
			dragondos_enabled = 0;
#ifdef TRACE
		} else if (!strcmp(argv[i], "-trace")) {
			trace = 1;
#endif
		}
	}
}

void machine_init(void) {
	sam_init();
	pia_init();
	wd2797_init();
	vdrive_init();
	m6809_init();
	vdg_init();
	tape_init();
	cart_init();
}

void machine_reset(int hard) {
	unsigned int i;
	machine_info *m = &machines[machine_romtype];
	pia_reset();
	if (hard) {
		memset(ram0, 0, sizeof(ram0));
		memset(ram1, 0, sizeof(ram1));
		memset(rom0, 0, sizeof(rom0));
		memset(rom1, 0, sizeof(rom1));
		for (i=0; i<5 && load_rom(m->rom1[i], rom1, sizeof(rom1)); i++);
		if (!IS_COCO || !noextbas)
			for (i=0; i<5 && load_rom(m->extbas[i], rom0, sizeof(rom0)); i++);
		for (i=0; i<5 && load_rom(m->bas[i], rom0+0x2000, sizeof(rom0)-0x2000); i++);
		if (dragondos_enabled) {
			for (i=0; i<5 && load_rom(m->dos[i], rom0+0x4000, sizeof(rom0)-0x4000); i++);
		}
	}
	if (IS_DRAGON64) {
		PIA_1B.port_input |= (1<<2);
		PIA_UPDATE_OUTPUT(PIA_1B);
	}
	wd2797_reset();
	sam_reset();
	m6809_reset();
	vdg_reset();
	tape_reset();
	cart_reset();
}

/* Setting romtype takes effect on next reset */
void machine_set_romtype(int mode) {
	machine_romtype = mode % NUM_MACHINES;
	machine_set_keymap(machines[machine_romtype].keymap);
	LOG_DEBUG(2, "%s selected\n", machines[machine_romtype].description);
}

/* Setting keymap takes effect immediately */
void machine_set_keymap(int mode) {
	machine_keymap = mode % NUM_KEYBOARDS;
	switch (machine_keymap) {
		default:
		case DRAGON_KEYBOARD:
			memcpy(keymap, dragon_keymap, sizeof(keymap));
			break;
		case COCO_KEYBOARD:
			memcpy(keymap, coco_keymap, sizeof(keymap));
			break;
	}
}

/* load_rom searches a path (specified in ROMPATH macro at compile time) to
 * find roms.  It searches each element in the (colon separated) path for
 * the supplied rom name with ".rom" or ".dgn" as a suffix.  load_rom_f does
 * the actual loading. */

#ifndef ROMPATH
# define ROMPATH ":/usr/local/share/xroar/roms"
#endif

static int load_rom_f(const char *filename, uint8_t *dest, size_t max_size) {
	FS_FILE fd;
	if ((fd = fs_open(filename, FS_READ)) == -1)
		return -1;
	LOG_DEBUG(3, "Loading ROM: %s\n", filename);
	fs_read(fd, dest, max_size);
	fs_close(fd);
	return 0;
}

static int load_dgn_f(const char *filename, uint8_t *dest, size_t max_size) {
	FS_FILE fd;
	if ((fd = fs_open(filename, FS_READ)) == -1)
		return -1;
	LOG_DEBUG(3, "Loading DGN: %s\n", filename);
	fs_read(fd, dest, 16);
	fs_read(fd, dest, max_size);
	fs_close(fd);
	return 0;
}

static char *construct_path(const char *path, const char *filename) {
	char *buf;
	char *until = strchr(path, ':');
	char *home = NULL;
	int path_length, home_length = 0;
	if (!until)
		path_length = strlen(path);
	else
		path_length = until - path;
	if (path[0] == '~' && (path[1] == '/' || path[1] == '\\')) {
		home = getenv("HOME");
		if (home)
			home_length = strlen(home)+1;
		path += 2;
		path_length -= 2;
	}
	buf = malloc(home_length + path_length + strlen(filename) + 2);
	if (!buf)
		return NULL;
	buf[0] = 0;
	if (home_length) {
		strcpy(buf, home);
		strcat(buf, "/");
	}
	if (path_length) {
		strncat(buf, path, path_length);
		buf[home_length + path_length] = '/';
		buf[home_length + path_length + 1] = 0;
	}
	strcat(buf, filename);
	return buf;
}

static int load_rom(const char *romname, uint8_t *dest, size_t max_size) {
	const char *path = ROMPATH;
	char *filename;
	char buf[13];
	unsigned int ret;
	if (!romname)
		return -1;
	/* Try ROM without any extension first */
	if (load_rom_f(romname, dest, max_size) == 0)
		return 0;
	if (strlen(romname) > 8)
		return -1;
	while (*path) {
		strcpy(buf, romname);
		strcat(buf, ".rom");
		filename = construct_path(path, buf);
		if (filename == NULL)
			return -1;
		ret = load_rom_f(filename, dest, max_size);
		free(filename);
		if (ret == 0)
			return 0;
		strcpy(buf, romname);
		strcat(buf, ".dgn");
		filename = construct_path(path, buf);
		if (filename == NULL)
			return -1;
		ret = load_dgn_f(filename, dest, max_size);
		free(filename);
		if (ret == 0)
			return 0;
		while (*path && *path != ':')
			path++;
		if (*path == ':')
			path++;
	}
	return -1;
}
