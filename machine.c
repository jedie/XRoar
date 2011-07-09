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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "types.h"
#include "cart.h"
#include "fs.h"
#include "joystick.h"
#include "keyboard.h"
#include "logging.h"
#include "m6809.h"
#include "m6809_trace.h"
#include "machine.h"
#include "mc6821.h"
#include "misc.h"
#include "module.h"
#include "path.h"
#include "printer.h"
#include "sam.h"
#include "sound.h"
#include "tape.h"
#include "vdg.h"
#include "vdrive.h"
#include "wd279x.h"
#include "xroar.h"

MachineConfig running_config;

unsigned int machine_ram_size = 0x10000;  /* RAM in bytes, up to 64K */
uint8_t machine_ram[0x10000];
uint8_t *machine_rom;
static uint8_t rom0[0x4000];
static uint8_t rom1[0x4000];
MC6821_PIA PIA0, PIA1;
struct cart *machine_cart = NULL;
static struct cart running_cart;

static const char *d32_extbas_roms[] = { "d32", "dragon32", "d32rom", "Dragon Data Ltd - Dragon 32 - IC17", "dragon", NULL };
static const char *d64_extbas_roms[] = { "d64_1", "d64rom1", "Dragon Data Ltd - Dragon 64 - IC17", "dragrom", "dragon", NULL };
static const char *d64_altbas_roms[] = { "d64_2", "d64rom2", "Dragon Data Ltd - Dragon 64 - IC18", NULL };
static const char *coco_bas_roms[] = { "bas13", "bas12", "bas11", "bas10", NULL };
static const char *coco_extbas_roms[] = { "extbas11", "extbas10", NULL };

static struct {
	const char **bas;
	const char **extbas;
	const char **altbas;
} rom_list[] = {
	{ NULL, d32_extbas_roms, NULL },
	{ NULL, d64_extbas_roms, d64_altbas_roms },
	{ coco_bas_roms, coco_extbas_roms, NULL }
};

struct machine_config_template {
	const char *name;
	const char *description;
	int architecture;
	int tv_standard;
	int ram;
};

static struct machine_config_template config_templates[] = {
	{ "dragon32", "Dragon 32", ARCH_DRAGON32, TV_PAL, 32, },
	{ "dragon64", "Dragon 64", ARCH_DRAGON64, TV_PAL, 64, },
	{ "tano", "Tano Dragon (NTSC)", ARCH_DRAGON64, TV_NTSC, 64, },
	{ "coco", "Tandy CoCo (PAL)", ARCH_COCO, TV_PAL, 64, },
	{ "cocous", "Tandy CoCo (NTSC)", ARCH_COCO, TV_NTSC, 64, },
};
#define NUM_CONFIG_TEMPLATES (int)(sizeof(config_templates)/sizeof(struct machine_config_template))

static struct machine_config **configs = NULL;
static int num_configs = NUM_CONFIG_TEMPLATES;

static void initialise_ram(void);

/**************************************************************************/

/* Create config array */
static int alloc_config_array(int size) {
	struct machine_config **new_list;
	int clear_from = num_configs;
	if (!configs) clear_from = 0;
	new_list = xrealloc(configs, size * sizeof(struct machine_config *));
	configs = new_list;
	memset(&configs[clear_from], 0, (size - clear_from) * sizeof(struct machine_config *));
	return 0;
}

/* Populate config from template */
static int populate_config_index(int i) {
	assert(configs != NULL);
	assert(i >= 0 && i < NUM_CONFIG_TEMPLATES);
	if (configs[i])
		return 0;
	configs[i] = xmalloc(sizeof(struct machine_config));
	memset(configs[i], 0, sizeof(struct machine_config));
	configs[i]->name = strdup(config_templates[i].name);
	if (!configs[i]->name) goto failed;
	configs[i]->description = strdup(config_templates[i].description);
	if (!configs[i]->description) goto failed;
	configs[i]->architecture = config_templates[i].architecture;
	configs[i]->keymap = ANY_AUTO;
	configs[i]->tv_standard = config_templates[i].tv_standard;
	configs[i]->ram = config_templates[i].ram;
	configs[i]->index = i;
	return 0;
	/* clean up on error */
failed:
	if (configs[i]->name) free(configs[i]->name);
	free(configs[i]);
	configs[i] = NULL;
	return -1;
}

struct machine_config *machine_config_new(void) {
	struct machine_config *new;
	if (alloc_config_array(num_configs+1) != 0)
		return NULL;
	new = xmalloc(sizeof(struct machine_config));
	memset(new, 0, sizeof(struct machine_config));
	new->index = num_configs;
	new->architecture = ANY_AUTO;
	new->keymap = ANY_AUTO;
	new->tv_standard = ANY_AUTO;
	new->ram = ANY_AUTO;
	configs[num_configs++] = new;
	return new;
}

int machine_config_count(void) {
	return num_configs;
}

struct machine_config *machine_config_index(int i) {
	if (i < 0 || i >= num_configs) {
		return NULL;
	}
	if (!configs) {
		if (alloc_config_array(num_configs) != 0)
			return NULL;
	}
	if (i < NUM_CONFIG_TEMPLATES && !configs[i]) {
		if (populate_config_index(i) != 0)
			return NULL;
	}
	return configs[i];
}

struct machine_config *machine_config_by_name(const char *name) {
	int count, i;
	if (!name) return NULL;
	count = machine_config_count();
	for (i = 0; i < count; i++) {
		struct machine_config *mc = machine_config_index(i);
		if (0 == strcmp(mc->name, name)) {
			return mc;
		}
	}
	return NULL;
}

struct machine_config *machine_config_by_arch(int arch) {
	int count, i;
	count = machine_config_count();
	for (i = 0; i < count; i++) {
		struct machine_config *mc = machine_config_index(i);
		if (mc->architecture == arch) {
			return mc;
		}
	}
	return NULL;
}

static int find_working_arch(void) {
	int arch;
	char *tmp = NULL;
	if ((tmp = machine_find_rom_in_list(d64_extbas_roms))) {
		arch = ARCH_DRAGON64;
	} else if ((tmp = machine_find_rom_in_list(d32_extbas_roms))) {
		arch = ARCH_DRAGON32;
	} else if ((tmp = machine_find_rom_in_list(coco_bas_roms))) {
		arch = ARCH_COCO;
	} else {
		/* Fall back to this, which won't start up properly */
		arch = ARCH_DRAGON64;
	}
	if (tmp)
		free(tmp);
	return arch;
}

struct machine_config *machine_config_first_working(void) {
	return machine_config_by_arch(find_working_arch());
}

void machine_config_complete(struct machine_config *mc) {
	if (!mc->description) {
		mc->description = strdup(mc->name);
	}
	if (mc->tv_standard == ANY_AUTO)
		mc->tv_standard = TV_PAL;
	/* Find actual path to any specified ROMs */
	if (mc->bas_rom) {
		char *tmp = machine_find_rom(mc->bas_rom);
		free(mc->bas_rom);
		mc->bas_rom = tmp;
	}
	if (mc->extbas_rom) {
		char *tmp = machine_find_rom(mc->extbas_rom);
		free(mc->extbas_rom);
		mc->extbas_rom = tmp;
	}
	if (mc->altbas_rom) {
		char *tmp = machine_find_rom(mc->altbas_rom);
		free(mc->altbas_rom);
		mc->altbas_rom = tmp;
	}

	/* Various heuristics to find a working architecture */
	if (mc->architecture == ANY_AUTO) {
		/* TODO: checksum ROMs to help determine arch */
		if (mc->bas_rom) {
			mc->architecture = ARCH_COCO;
		} else if (mc->altbas_rom) {
			mc->architecture = ARCH_DRAGON64;
		} else if (mc->extbas_rom) {
			if (fs_size(mc->extbas_rom) > 0x2000) {
				mc->architecture = ARCH_DRAGON64;
			} else {
				mc->architecture = ARCH_COCO;
			}
		} else {
			mc->architecture = find_working_arch();
		}
	}
	if (mc->ram < 4 || mc->ram > 64) {
		switch (mc->architecture) {
			case ARCH_DRAGON32:
				mc->ram = 32;
				break;
			default:
				mc->ram = 64;
				break;
		}
	}
	if (mc->keymap == ANY_AUTO) {
		switch (mc->architecture) {
		case ARCH_DRAGON64: case ARCH_DRAGON32: default:
			mc->keymap = KEYMAP_DRAGON;
			break;
		case ARCH_COCO:
			mc->keymap = KEYMAP_COCO;
			break;
		}
	}
	/* Now find which ROMs we're actually going to use */
	if (!mc->nobas && !mc->bas_rom) {
		mc->bas_rom = machine_find_rom_in_list(rom_list[mc->architecture].bas);
	}
	if (!mc->noextbas && !mc->extbas_rom) {
		mc->extbas_rom = machine_find_rom_in_list(rom_list[mc->architecture].extbas);
	}
	if (!mc->noaltbas && !mc->altbas_rom) {
		mc->altbas_rom = machine_find_rom_in_list(rom_list[mc->architecture].altbas);
	}
}

/* ---------------------------------------------------------------------- */

#define pia0a_data_postwrite keyboard_row_update
#define pia0a_control_postwrite sound_update

#define pia0b_data_postwrite keyboard_column_update
#define pia0b_control_postwrite sound_update

static void pia0b_data_postwrite_coco64k(void) {
	keyboard_column_update();
	/* PB6 of PIA0 is linked to PB2 of PIA1 on 64K CoCos */
	if (PIA0.b.port_output & 0x40)
		PIA1.b.port_input |= (1<<2);
	else
		PIA1.b.port_input &= ~(1<<2);
}

# define pia1a_data_preread NULL

static void pia1a_data_postwrite(void) {
	sound_update();
	joystick_update();
	tape_update_output();
	if (IS_DRAGON)
		printer_strobe();
}

#define pia1a_control_postwrite tape_update_motor

static void pia1b_data_postwrite(void) {
	if (IS_DRAGON64) {
		machine_rom = (PIA1.b.port_output & 0x04) ? rom0 : rom1;
	}
	sound_update();
	vdg_set_mode();
}
#define pia1b_control_postwrite sound_update

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
	machine_select_fast_sound(xroar_fast_sound);
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

void machine_configure(struct machine_config *mc) {
	if (!mc) return;
	machine_config_complete(mc);
	if (mc->description) {
		LOG_DEBUG(2, "Machine: %s\n", mc->description);
	}
	/* */
	running_config.architecture = mc->architecture;
	running_config.tv_standard = mc->tv_standard;
	running_config.ram = mc->ram;
	xroar_set_keymap(mc->keymap);
	switch (mc->tv_standard) {
	case TV_PAL: default:
		xroar_select_cross_colour(CROSS_COLOUR_OFF);
		break;
	case TV_NTSC:
		xroar_select_cross_colour(CROSS_COLOUR_KBRW);
		break;
	}
	/* Load appropriate ROMs */
	memset(rom0, 0, sizeof(rom0));
	memset(rom1, 0, sizeof(rom1));
	/* ... BASIC */
	if (!mc->nobas && mc->bas_rom) {
		machine_load_rom(mc->bas_rom, rom0 + 0x2000, sizeof(rom0) - 0x2000);
	}
	/* ... Extended BASIC */
	if (!mc->noextbas && mc->extbas_rom) {
		machine_load_rom(mc->extbas_rom, rom0, sizeof(rom0));
	}
	/* ... Alternate BASIC ROM */
	if (!mc->noaltbas && mc->altbas_rom) {
		machine_load_rom(mc->altbas_rom, rom1, sizeof(rom1));
	}
	machine_ram_size = mc->ram * 1024;
	/* This will be under PIA control on a Dragon 64 */
	machine_rom = rom0;
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

void machine_reset(int hard) {
	if (hard) {
		initialise_ram();
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

#ifndef FAST_SOUND
void machine_set_fast_sound(int fast) {
	xroar_fast_sound = fast;
	if (fast) {
		PIA0.a.control_postwrite = NULL;
		PIA0.b.control_postwrite = NULL;
		PIA1.b.control_postwrite = NULL;
	} else  {
		PIA0.a.control_postwrite = pia0a_control_postwrite;
		PIA0.b.control_postwrite = pia0b_control_postwrite;
		PIA1.b.control_postwrite = pia1b_control_postwrite;
	}
}

void machine_select_fast_sound(int fast) {
	if (ui_module->fast_sound_changed_cb) {
		ui_module->fast_sound_changed_cb(fast);
	}
	machine_set_fast_sound(fast);
}
#endif

void machine_insert_cart(struct cart_config *cc) {
	machine_remove_cart();
	if (cc) {
		cart_configure(&running_cart, cc);
		machine_cart = &running_cart;
		if (machine_cart->attach) {
			machine_cart->attach();
		}
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
		machine_ram[loc++] = val;
		machine_ram[loc++] = val;
		machine_ram[loc++] = val;
		machine_ram[loc++] = val;
		if ((loc & 0xff) != 0)
			val ^= 0xff;
	}
}

/**************************************************************************/

static const char *rom_extensions[] = {
	"", ".rom", ".ROM", ".dgn", ".DGN", NULL
};

/* Find a ROM within rom path. */
char *machine_find_rom(const char *romname) {
	char *filename;
	char *path = NULL;
	int i;

	if (romname == NULL)
		return NULL;

	filename = xmalloc(strlen(romname) + 5);
	for (i = 0; rom_extensions[i]; i++) {
		strcpy(filename, romname);
		strcat(filename, rom_extensions[i]);
		path = find_in_path(xroar_rom_path, filename);
		if (path) break;
	}
	free(filename);
	return path;
}

char *machine_find_rom_in_list(const char **list) {
	char *path;
	int i;
	if (list == NULL)
		return NULL;
	for (i = 0; list[i]; i++) {
		if ((path = machine_find_rom(list[i])))
			return path;
	}
	return NULL;
}

int machine_load_rom(const char *path, uint8_t *dest, size_t max_size) {
	char *dot;
	int fd;
	int size;

	if (path == NULL)
		return -1;
	dot = strrchr(path, '.');
	if ((fd = fs_open(path, FS_READ)) == -1) {
		return -1;
	}
	if (dot && strcasecmp(dot, ".dgn") == 0) {
		LOG_DEBUG(2, "Loading DGN: %s\n", path);
		fs_read(fd, dest, 16);
	} else {
		LOG_DEBUG(2, "Loading ROM: %s\n", path);
	}
	size = fs_read(fd, dest, max_size);
	fs_close(fd);
	return size;
}
