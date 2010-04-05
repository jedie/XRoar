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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "types.h"
#include "cart.h"
#include "deltados.h"
#include "dragondos.h"
#include "fs.h"
#include "joystick.h"
#include "keyboard.h"
#include "logging.h"
#include "m6809.h"
#include "m6809_trace.h"
#include "machine.h"
#include "mc6821.h"
#include "module.h"
#include "path.h"
#include "rsdos.h"
#include "sam.h"
#include "tape.h"
#include "vdg.h"
#include "vdrive.h"
#include "wd279x.h"
#include "xroar.h"

const char *machine_names[NUM_MACHINE_TYPES] = {
	"Dragon 32", "Dragon 64", "Tano Dragon (NTSC)", "Tandy CoCo (PAL)", "Tandy CoCo (NTSC)"
};

MachineConfig machine_defaults[NUM_MACHINE_TYPES] = {
	{ ARCH_DRAGON32, ROMSET_DRAGON32, KEYMAP_DRAGON, TV_PAL,  CROSS_COLOUR_OFF,  32, DOS_DRAGONDOS, NULL, NULL, NULL, NULL },
	{ ARCH_DRAGON64, ROMSET_DRAGON64, KEYMAP_DRAGON, TV_PAL,  CROSS_COLOUR_OFF,  64, DOS_DRAGONDOS, NULL, NULL, NULL, NULL },
	{ ARCH_DRAGON64, ROMSET_DRAGON64, KEYMAP_DRAGON, TV_NTSC, CROSS_COLOUR_KBRW, 64, DOS_DRAGONDOS, NULL, NULL, NULL, NULL },
	{ ARCH_COCO,     ROMSET_COCO,     KEYMAP_COCO,   TV_PAL,  CROSS_COLOUR_OFF,  64, DOS_RSDOS,     NULL, NULL, NULL, NULL },
	{ ARCH_COCO,     ROMSET_COCO,     KEYMAP_COCO,   TV_NTSC, CROSS_COLOUR_KBRW, 64, DOS_RSDOS,     NULL, NULL, NULL, NULL }
};

int requested_machine;
MachineConfig requested_config = {
	ANY_AUTO, ANY_AUTO, ANY_AUTO, ANY_AUTO, ANY_AUTO, ANY_AUTO, ANY_AUTO, NULL, NULL, NULL, NULL
};
int running_machine;
MachineConfig running_config;

unsigned int machine_ram_size = 0x10000;  /* RAM in bytes, up to 64K */
uint8_t ram0[0x10000];
uint8_t rom0[0x4000];
uint8_t rom1[0x4000];
MC6821_PIA PIA0, PIA1;
struct cart *machine_cart = NULL;

Cycle current_cycle;

static const char *d32_extbas_roms[] = { "d32", "dragon32", "d32rom", "Dragon Data Ltd - Dragon 32 - IC17", "dragon", NULL };
static const char *d64_extbas_roms[] = { "d64_1", "d64rom1", "Dragon Data Ltd - Dragon 64 - IC17", "dragrom", "dragon", NULL };
static const char *d64_altbas_roms[] = { "d64_2", "d64rom2", "Dragon Data Ltd - Dragon 64 - IC18", NULL };
static const char *coco_bas_roms[] = { "bas13", "bas12", "bas11", "bas10", NULL };
static const char *coco_extbas_roms[] = { "extbas11", "extbas10", NULL };
static const char *dragondos_roms[] = {
	"dplus49b", "dplus48", "DOSPLUS",
	"sdose6", "PNP - SuperDOS E6", "sdose5", "sdose4",
	"ddos40", "ddos15", "ddos10",
	"cdos20", "CDOS20",
	NULL
};
static const char *deltados_roms[] = {
	"delta", "deltados", "Premier Micros - DeltaDOS",
	NULL
};
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

static const char **dos_rom_list[] = {
	dragondos_roms, rsdos_roms, deltados_roms
};

const char *dos_type_names[NUM_DOS_TYPES] = {
	"No DOS cartridge", "DragonDOS", "RS-DOS", "Delta System"
};

static const char *machine_options[NUM_MACHINE_TYPES] = {
	"dragon32", "dragon64", "tano", "coco", "cocous"
};

static const char *dos_type_options[NUM_DOS_TYPES] = {
	"none", "dragondos", "rsdos", "delta"
};

static void initialise_ram(void);

static char *find_rom(const char *romname);
static char *find_rom_in_list(const char *preferred, const char **list);
static int load_rom_from_list(const char *preferred, const char **list,
		uint8_t *dest, size_t max_size);

/**************************************************************************/

void machine_getargs(void) {
	machine_clear_requested_config();
	requested_machine = ANY_AUTO;
	if (xroar_opt_machine) {
		int i;
		if (0 == strcmp(xroar_opt_machine, "help")) {
			for (i = 0; i < NUM_MACHINE_TYPES; i++) {
				printf("\t%-10s%s\n", machine_options[i], machine_names[i]);
			}
			exit(0);
		}
		for (i = 0; i < NUM_MACHINE_TYPES; i++) {
			if (!strcmp(xroar_opt_machine, machine_options[i])) {
				requested_machine = i;
			}
		}
	}
	if (xroar_opt_dostype) {
		int i;
		if (0 == strcmp(xroar_opt_dostype, "help")) {
			for (i = 0; i < NUM_DOS_TYPES; i++) {
				printf("\t%-10s%s\n", dos_type_options[i], dos_type_names[i]);
			}
			exit(0);
		}
		for (i = 0; i < NUM_DOS_TYPES; i++) {
			if (!strcmp(xroar_opt_dostype, dos_type_options[i])) {
				requested_config.dos_type = i;
			}
		}
	}
	requested_config.bas_rom = xroar_opt_bas;
	requested_config.extbas_rom = xroar_opt_extbas;
	requested_config.altbas_rom = xroar_opt_altbas;
	requested_config.dos_rom = xroar_opt_dos;
	if (xroar_opt_nodos) {
		requested_config.dos_type = DOS_NONE;
		requested_config.dos_rom = NULL;
	}
	if (xroar_opt_ram > 0) {
		requested_config.ram = xroar_opt_ram;
	}
	requested_config.tv_standard = xroar_opt_tv;
}

static void update_sound(void) {
	int value;
	if (!(PIA1.b.control_register & 0x08)) {
		/* Single-bit sound */
		value = (PIA1.b.port_output & 0x02) ? 0x3f : 0;
	} else {
		if (PIA0.b.control_register & 0x08) {
			/* Sound disabled */
			value = 0;
		} else {
			/* DAC output */
			value = (PIA1.a.port_output & 0xfc) >> 1;
		}
	}
#ifndef FAST_SOUND
	if (value >= 0x4c)
		PIA1.b.port_input |= 0x02;
	else
		PIA1.b.port_input &= 0xfd;
#endif
	sound_module->update(value);
}

#define pia0a_data_postwrite keyboard_row_update
#define pia0a_control_postwrite update_sound

#define pia0b_data_postwrite keyboard_column_update
#define pia0b_control_postwrite update_sound

static void pia0b_data_postwrite_coco64k(void) {
	keyboard_column_update();
	/* PB6 of PIA0 is linked to PB2 of PIA1 on 64K CoCos */
	if (PIA0.b.port_output & 0x40)
		PIA1.b.port_input |= (1<<2);
	else
		PIA1.b.port_input &= ~(1<<2);
}

#ifdef HAVE_SNDFILE
# define pia1a_data_preread tape_update_input
#else
# define pia1a_data_preread NULL
#endif

static void pia1a_data_postwrite(void) {
	update_sound();
	joystick_update();
	tape_update_output();
}

#define pia1a_control_postwrite tape_update_motor

static void pia1b_data_postwrite(void) {
	if (IS_DRAGON64) {
		sam_mapped_rom = (PIA1.b.port_output & 0x04) ? rom0 : rom1;
	}
	update_sound();
	vdg_set_mode();
}
#define pia1b_control_postwrite update_sound

void machine_init(void) {
	sam_init();
	mc6821_init(&PIA0);
	PIA0.a.data_postwrite = pia0a_data_postwrite;
	PIA0.b.data_postwrite = pia0b_data_postwrite;
	mc6821_init(&PIA1);
	PIA1.a.data_preread = pia1a_data_preread;
	PIA1.a.data_postwrite = pia1a_data_postwrite;
	PIA1.a.control_postwrite = pia1a_control_postwrite;
	PIA1.b.data_postwrite = pia1b_data_postwrite;
#ifndef FAST_SOUND
	if (!xroar_fast_sound) {
		PIA0.a.control_postwrite = pia0a_control_postwrite;
		PIA0.b.control_postwrite = pia0b_control_postwrite;
		PIA1.b.control_postwrite = pia1b_control_postwrite;
	}
#endif
	wd279x_init();
	vdrive_init();
	m6809_init();
	vdg_init();
	tape_init();
}

void machine_shutdown(void) {
	tape_shutdown();
	vdrive_shutdown();
}

/* If user didn't specify a machine, find the first one that has the bare
 * minimum of ROMs to get going */
static void find_working_machine(void) {
	char *tmp = NULL;
	if ((tmp = find_rom_in_list(requested_config.extbas_rom, d64_extbas_roms))) {
		if (requested_config.tv_standard == TV_NTSC)
			requested_machine = MACHINE_TANO;
		else
			requested_machine = MACHINE_DRAGON64;
	} else if ((tmp = find_rom_in_list(NULL, d32_extbas_roms))) {
		requested_machine = MACHINE_DRAGON32;
	} else if ((tmp = find_rom_in_list(requested_config.bas_rom, coco_bas_roms))) {
		if (requested_config.tv_standard == TV_NTSC)
			requested_machine = MACHINE_COCOUS;
		else
			requested_machine = MACHINE_COCO;
	} else {
		/* Fall back to this, which won't start up properly */
		requested_machine = MACHINE_DRAGON64;
	}
	if (tmp)
		free(tmp);
}

static void find_working_dos_type(void) {
	char *tmp = NULL;
	running_config.dos_type = DOS_NONE;  /* default to none */
	if (IS_DRAGON) {
		if ((tmp = find_rom_in_list(requested_config.dos_rom, dragondos_roms))) {
			running_config.dos_type = DOS_DRAGONDOS;
		} else if ((tmp = find_rom_in_list(NULL, deltados_roms))) {
			running_config.dos_type = DOS_DELTADOS;
		}
	} else {
		if ((tmp = find_rom_in_list(requested_config.dos_rom, rsdos_roms))) {
			running_config.dos_type = DOS_RSDOS;
		}
	}
	if (tmp)
		free(tmp);
}

void machine_reset(int hard) {
	if (hard) {
		MachineConfig *defaults;
		if (requested_machine == ANY_AUTO)
			find_working_machine();
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
		if (running_config.cross_colour_phase == ANY_AUTO)
			running_config.cross_colour_phase = defaults->cross_colour_phase % NUM_CROSS_COLOUR_PHASES;
		if (running_config.ram == ANY_AUTO)
			running_config.ram = defaults->ram;
		if (running_config.dos_type == ANY_AUTO)
			find_working_dos_type();
		/* Load appropriate ROMs */
		memset(rom0, 0x7e, sizeof(rom0));
		memset(rom1, 0x7e, sizeof(rom1));
		/* ... BASIC */
		if (!xroar_opt_nobas) {
			load_rom_from_list(running_config.bas_rom,
					rom_list[running_config.romset].bas,
					rom0 + 0x2000, sizeof(rom0) - 0x2000);
		}
		/* ... Extended BASIC */
		if (!xroar_opt_noextbas) {
			load_rom_from_list(running_config.extbas_rom,
					rom_list[running_config.romset].extbas,
					rom0, sizeof(rom0));
		}
		/* ... Alternate BASIC ROM */
		if (!xroar_opt_noaltbas) {
			load_rom_from_list(running_config.altbas_rom,
					rom_list[running_config.romset].altbas,
					rom1, sizeof(rom1));
		}
		/* ... DOS */
		if (DOS_ENABLED) {
			char *dos_rom = find_rom_in_list(running_config.dos_rom, dos_rom_list[running_config.dos_type - 1]);
			LOG_DEBUG(2, "%s selected\n", dos_type_names[running_config.dos_type]);
			machine_remove_cart();
			switch (running_config.dos_type) {
				case DOS_DRAGONDOS:
					machine_insert_cart(dragondos_new(dos_rom));
					break;
				case DOS_RSDOS:
					machine_insert_cart(rsdos_new(dos_rom));
					break;
				case DOS_DELTADOS:
					machine_insert_cart(deltados_new(dos_rom));
					break;
			}
		} else if (machine_cart) {
			if (machine_cart->type != CART_ROM && machine_cart->type != CART_RAM) {
				machine_remove_cart();
			}
		}
		/* Configure keymap */
		keyboard_set_keymap(running_config.keymap);
		/* Configure RAM */
		if (running_config.ram < 4) running_config.ram = 4;
		if (running_config.ram > 64) running_config.ram = 64;
		machine_ram_size = running_config.ram * 1024;
		initialise_ram();
		/* This will be under PIA control on a Dragon 64 */
		sam_mapped_rom = rom0;
		/* Machine-specific PIA connections */
		PIA1.b.tied_low |= (1<<2);
		PIA1.b.port_input &= ~(1<<2);
		if (IS_DRAGON64) {
			PIA1.b.port_input |= (1<<2);
		} else if (IS_COCO && machine_ram_size <= 0x1000) {
			/* 4K CoCo ties PB2 of PIA1 low */
			PIA1.b.tied_low &= ~(1<<2);
		} else if (IS_COCO && machine_ram_size <= 0x4000) {
			/* 16K CoCo pulls PB2 of PIA1 high */
			PIA1.b.port_input |= (1<<2);
		}
		PIA0.b.data_postwrite = pia0b_data_postwrite;
		if (IS_COCO && machine_ram_size > 0x4000) {
			/* 64K CoCo connects PB6 of PIA0 to PB2 of PIA1.
			 * Deal with this through a postwrite. */
			PIA0.b.data_postwrite = pia0b_data_postwrite_coco64k;
		}
	}
	mc6821_reset(&PIA0);
	mc6821_reset(&PIA1);
	if (machine_cart && machine_cart->reset) {
		machine_cart->reset();
	}
	sam_reset();
	m6809_reset();
#ifdef TRACE
	m6809_trace_reset();
#endif
	vdg_reset();
	tape_reset();
}

void machine_clear_requested_config(void) {
	requested_config.architecture = ANY_AUTO;
	requested_config.romset = ANY_AUTO;
	requested_config.keymap = ANY_AUTO;
	requested_config.tv_standard = ANY_AUTO;
	requested_config.cross_colour_phase = ANY_AUTO;
	requested_config.ram = ANY_AUTO;
	requested_config.dos_type = ANY_AUTO;
	requested_config.bas_rom = NULL;
	requested_config.extbas_rom = NULL;
	requested_config.altbas_rom = NULL;
	requested_config.dos_rom = NULL;
}

void machine_insert_cart(struct cart *cart) {
	machine_remove_cart();
	machine_cart = cart;
	if (cart) {
		switch (cart->type) {
			case CART_DRAGONDOS:
				requested_config.dos_type = DOS_DRAGONDOS;
				break;
			case CART_DELTADOS:
				requested_config.dos_type = DOS_DELTADOS;
				break;
			case CART_RSDOS:
				requested_config.dos_type = DOS_RSDOS;
				break;
			default:
				requested_config.dos_type = DOS_NONE;
				break;
		}
		running_config.dos_type = requested_config.dos_type;
	}
}

void machine_remove_cart(void) {
	if (machine_cart && machine_cart->detach) {
		machine_cart->detach();
	}
	machine_cart = NULL;
}

/* Intialise RAM contents */
static void initialise_ram(void) {
	int loc = 0, val = 0xff;
	/* Don't know why, but RAM seems to start in this state: */
	while (loc < 0x10000) {
		ram0[loc++] = val;
		ram0[loc++] = val;
		ram0[loc++] = val;
		ram0[loc++] = val;
		if ((loc & 0xff) != 0)
			val ^= 0xff;
	}
}

/**************************************************************************/

/* load_rom_from_list searches a path to find the preferred ROM, or one from
 * the supplied list.  It searches for each rom name as-is, and with ".rom" or
 * ".dgn" extensions added.  */

static const char *rom_extensions[] = {
	"", ".rom", ".ROM", ".dgn", ".DGN", NULL
};

/* Find a ROM within rom path. */
static char *find_rom(const char *romname) {
	char *filename;
	char *path = NULL;
	int i;

	if (romname == NULL)
		return NULL;

	filename = malloc(strlen(romname) + 5);
	for (i = 0; rom_extensions[i]; i++) {
		strcpy(filename, romname);
		strcat(filename, rom_extensions[i]);
		path = find_in_path(xroar_rom_path, filename);
		if (path) break;
	}
	free(filename);
	return path;
}

static char *find_rom_in_list(const char *preferred, const char **list) {
	char *path;
	int i;
	if (preferred && (path = find_rom(preferred)))
		return path;
	if (list == NULL)
		return NULL;
	for (i = 0; list[i]; i++) {
		if ((path = find_rom(list[i])))
			return path;
	}
	return NULL;
}

static int load_rom_from_list(const char *preferred, const char **list,
		uint8_t *dest, size_t max_size) {
	char *path;
	int ret;

	path = find_rom_in_list(preferred, list);
	if (path == NULL)
		return -1;
	ret = machine_load_rom(path, dest, max_size);
	free(path);
	return ret;
}

int machine_load_rom(const char *path, uint8_t *dest, size_t max_size) {
	char *dot;
	int fd;

	if (path == NULL)
		return -1;
	dot = strrchr(path, '.');
	if ((fd = fs_open(path, FS_READ)) == -1) {
		return -1;
	}
	if (dot && strcmp(dot, ".dgn") == 0) {
		LOG_DEBUG(3, "Loading DGN: %s\n", path);
		fs_read(fd, dest, 16);
	} else {
		LOG_DEBUG(3, "Loading ROM: %s\n", path);
	}
	fs_read(fd, dest, max_size);
	fs_close(fd);
	return 0;
}
