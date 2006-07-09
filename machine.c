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

const char *machine_names[NUM_MACHINE_TYPES] = {
	"Dragon 32", "Dragon 64", "Tano Dragon (NTSC)", "Tandy CoCo (PAL)", "Tandy CoCo (NTSC)"
};

MachineConfig machine_defaults[NUM_MACHINE_TYPES] = {
	{ ARCH_DRAGON32, ROMSET_DRAGON32, KEYMAP_DRAGON, TV_PAL,  32, DOS_DRAGONDOS, NULL, NULL, NULL, NULL },
	{ ARCH_DRAGON64, ROMSET_DRAGON64, KEYMAP_DRAGON, TV_PAL,  64, DOS_DRAGONDOS, NULL, NULL, NULL, NULL },
	{ ARCH_DRAGON64, ROMSET_DRAGON64, KEYMAP_DRAGON, TV_NTSC, 64, DOS_DRAGONDOS, NULL, NULL, NULL, NULL },
	{ ARCH_COCO,     ROMSET_COCO,     KEYMAP_COCO,   TV_PAL,  64, DOS_RSDOS,     NULL, NULL, NULL, NULL },
	{ ARCH_COCO,     ROMSET_COCO,     KEYMAP_COCO,   TV_NTSC, 64, DOS_RSDOS,     NULL, NULL, NULL, NULL }
};

int requested_machine = 1;
MachineConfig requested_config = {
	ANY_AUTO, ANY_AUTO, ANY_AUTO, ANY_AUTO, ANY_AUTO, ANY_AUTO, NULL, NULL, NULL, NULL
};
int running_machine;
MachineConfig running_config;

unsigned int machine_page0_ram = 0x8000;  /* Base RAM in bytes, up to 32K */
unsigned int machine_page1_ram = 0x8000;  /* Generally 0 or 32K */
Keymap keymap;
uint8_t ram0[0x8000];
uint8_t ram1[0x8000];
uint8_t rom0[0x8000];
uint8_t rom1[0x8000];

int noextbas;

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

static const char *d32_extbas_roms[] = { "d32", "dragon32", "d32rom", "dragon", NULL };
static const char *d64_extbas_roms[] = { "d64_1", "d64rom1", "dragrom", "dragon", NULL };
static const char *d64_altbas_roms[] = { "d64_2", "d64rom2", NULL };
static const char *coco_bas_roms[] = { "bas13", "bas12", "bas11", "bas10", NULL };
static const char *coco_extbas_roms[] = { "extbas11", "extbas10", NULL };
static const char *dragondos_roms[] = { "sdose6", "ddos10", NULL };
static const char *rsdos_roms[] = { "disk11", "disk10", NULL };

static struct {
	const char **bas;
	const char **extbas;
	const char **altbas;
} rom_list[] = {
	{ NULL, d32_extbas_roms, NULL },
	{ NULL, d64_extbas_roms, d64_altbas_roms },
	{ coco_bas_roms, coco_extbas_roms, NULL }
};

static const char **dos_rom_list[] = { dragondos_roms, rsdos_roms };

static const char *machine_options[NUM_MACHINE_TYPES] = {
	"dragon32", "dragon64", "tano", "coco", "cocous"
};

static int load_rom_from_list(const char *preferred, const char **list,
		uint8_t *dest, size_t max_size);
static int load_rom(const char *romname, uint8_t *dest, size_t max_size);

/**************************************************************************/

void machine_helptext(void) {
	puts("  -machine MACHINE      emulated machine [dragon32|dragon64|tano|coco|cocous]");
	puts("  -bas FILENAME         specify BASIC ROM to use (CoCo only)");
	puts("  -extbas FILENAME      specify Extended BASIC ROM to use");
	puts("  -altbas FILENAME      specify alternate BASIC ROM (Dragon 64)");
	puts("  -noextbas             disable Extended BASIC");
	puts("  -dos FILENAME         specify DOS ROM (or CoCo Disk BASIC)");
	puts("  -nodos                disable DOS (ROM and hardware emulation)");
	puts("  -pal                  emulate PAL (50Hz) video");
	puts("  -ntsc                 emulate NTSC (60Hz) video");
	puts("  -ram KBYTES           specify amount of RAM in K");
#ifdef TRACE
	puts("  -trace                start with trace mode on");
#endif
}

void machine_getargs(int argc, char **argv) {
	int i, j;
	machine_clear_requested_config();
	noextbas = 0;
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-machine") && i+1<argc) {
			i++;
			if (!strcmp(argv[i], "help")) {
				for (j = 0; j < NUM_MACHINE_TYPES; j++) {
					printf("\t%-10s%s\n", machine_options[j], machine_names[j]);
				}
				exit(0);
			}
			for (j = 0; j < NUM_MACHINE_TYPES; j++) {
				if (!strcmp(argv[i], machine_options[j])) {
					requested_machine = j;
				}
			}
		} else if (!strcmp(argv[i], "-bas") && i+1<argc) {
			requested_config.bas_rom = argv[++i];
		} else if (!strcmp(argv[i], "-extbas") && i+1<argc) {
			requested_config.extbas_rom = argv[++i];
		} else if (!strcmp(argv[i], "-altbas") && i+1<argc) {
			requested_config.altbas_rom = argv[++i];
		} else if (!strcmp(argv[i], "-noextbas")) {
			noextbas = 1;
		} else if (!strcmp(argv[i], "-dos") && i+1<argc) {
			requested_config.dos_rom = argv[++i];
		} else if (!strcmp(argv[i], "-nodos")) {
			requested_config.dos_type = DOS_NONE;
		} else if (!strcmp(argv[i], "-ram") && i+1<argc) {
			requested_config.ram = strtol(argv[++i], NULL, 0);
		} else if (!strcmp(argv[i], "-pal")) {
			requested_config.tv_standard = TV_PAL;
		} else if (!strcmp(argv[i], "-ntsc")) {
			requested_config.tv_standard = TV_NTSC;
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
	if (hard) {
		MachineConfig *defaults;
		running_machine = requested_machine % NUM_MACHINE_TYPES;
		LOG_DEBUG(2, "%s selected\n", machine_names[running_machine]);
		defaults = &machine_defaults[running_machine];
		/* Update running config */
		memcpy(&running_config, &requested_config, sizeof(MachineConfig));
		if (running_config.architecture == ANY_AUTO)
			running_config.architecture = defaults->architecture % NUM_ARCHITECTURES;
		if (running_config.romset == ANY_AUTO)
			running_config.romset = defaults->romset % NUM_ROMSETS;
		if (running_config.keymap == ANY_AUTO)
			running_config.keymap = defaults->keymap % NUM_KEYMAPS;
		if (running_config.tv_standard == ANY_AUTO)
			running_config.tv_standard = defaults->tv_standard % NUM_TV_STANDARDS;
		if (running_config.ram == ANY_AUTO)
			running_config.ram = defaults->ram;
		if (running_config.dos_type == ANY_AUTO)
			running_config.dos_type = defaults->dos_type % NUM_DOS_TYPES;
		/* Load appropriate ROMs */
		memset(rom0, 0x7e, sizeof(rom0));
		memset(rom1, 0x7e, sizeof(rom1));
		/* ... BASIC */
		load_rom_from_list(running_config.bas_rom,
				rom_list[running_config.romset].bas,
				rom0 + 0x2000, sizeof(rom0) - 0x2000);
		/* ... Extended BASIC */
		if (!noextbas) {
			load_rom_from_list(running_config.extbas_rom,
					rom_list[running_config.romset].extbas,
					rom0, sizeof(rom0));
		}
		/* ... DOS */
		if (DOS_ENABLED) {
			load_rom_from_list(running_config.dos_rom,
					dos_rom_list[running_config.dos_type - 1],
					rom0 + 0x4000, sizeof(rom0) - 0x4000);
		}
		/* ... Alternate BASIC ROM */
		load_rom_from_list(running_config.altbas_rom,
				rom_list[running_config.romset].altbas,
				rom1, sizeof(rom1));
		/* Configure keymap */
		machine_set_keymap(running_config.keymap);
		/* Configure RAM */
		memset(ram0, 0x00, sizeof(ram0));
		memset(ram1, 0x00, sizeof(ram1));
		machine_set_page0_ram_size(running_config.ram * 1024);
		if (running_config.ram > 32) {
			machine_set_page1_ram_size((running_config.ram-32) * 1024);
		} else {
			machine_set_page1_ram_size(0);
		}
	}
	pia_reset();
	wd2797_reset();
	sam_reset();
	m6809_reset();
	vdg_reset();
	tape_reset();
	cart_reset();
}

void machine_clear_requested_config(void) {
	requested_config.architecture = ANY_AUTO;
	requested_config.romset = ANY_AUTO;
	requested_config.keymap = ANY_AUTO;
	requested_config.tv_standard = ANY_AUTO;
	requested_config.ram = ANY_AUTO;
	requested_config.dos_type = ANY_AUTO;
	requested_config.bas_rom = NULL;
	requested_config.extbas_rom = NULL;
	requested_config.altbas_rom = NULL;
	requested_config.dos_rom = NULL;
}

/* Keymap can be changed on-the-fly */
void machine_set_keymap(int map) {
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

/* Set RAM sizes for page0, page1 */
void machine_set_page0_ram_size(unsigned int size) {
	if (size > 0x8000)
		size = 0x8000;
	machine_page0_ram = size;
	if (size < 0x8000)
		memset(ram0 + size, 0x7e, 0x8000 - size);
}

void machine_set_page1_ram_size(unsigned int size) {
	if (size > 0x8000)
		size = 0x8000;
	machine_page1_ram = size;
	if (size < 0x8000)
		memset(ram1 + size, 0x7e, 0x8000 - size);
}

/**************************************************************************/

/* load_rom searches a path (specified in ROMPATH macro at compile time) to
 * find roms.  It searches each element in the (colon separated) path for
 * the supplied rom name with ".rom" or ".dgn" as a suffix.  load_rom_f does
 * the actual loading. */

#ifndef ROMPATH
# define ROMPATH ":/usr/local/share/xroar/roms"
#endif

static int load_rom_f(const char *filename, uint8_t *dest, size_t max_size) {
	int fd;
	if ((fd = fs_open(filename, FS_READ)) == -1)
		return -1;
	LOG_DEBUG(3, "Loading ROM: %s\n", filename);
	fs_read(fd, dest, max_size);
	fs_close(fd);
	return 0;
}

static int load_dgn_f(const char *filename, uint8_t *dest, size_t max_size) {
	int fd;
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

static int load_rom_from_list(const char *preferred, const char **list,
		uint8_t *dest, size_t max_size) {
	int i;
	if (list == NULL)
		return 1;
	if (preferred && !load_rom(preferred, dest, max_size))
		return 1;
	for (i = 0; list[i] && load_rom(list[i], dest, max_size); i++);
	return 1;
}
