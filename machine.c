/*  Copyright 2003-2012 Ciaran Anscomb
 *
 *  This file is part of XRoar.
 *
 *  XRoar is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  XRoar is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XRoar.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "portalib/strings.h"
#include "portalib/glib.h"

#include "types.h"
#include "cart.h"
#include "crc32.h"
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
#include "printer.h"
#include "romlist.h"
#include "sam.h"
#include "sound.h"
#include "tape.h"
#include "vdg.h"
#include "vdrive.h"
#include "wd279x.h"
#include "xroar.h"

unsigned int machine_ram_size = 0x10000;  /* RAM in bytes, up to 64K */
uint8_t machine_ram[0x10000];
uint8_t *machine_rom;
static uint8_t rom0[0x4000];
static uint8_t rom1[0x4000];
MC6821_PIA PIA0, PIA1;
struct cart *machine_cart = NULL;
static struct cart running_cart;
_Bool has_bas, has_extbas, has_altbas;
uint32_t crc_bas, crc_extbas, crc_altbas;

/* Useful configuration side-effect tracking */
static _Bool unexpanded_dragon32 = 0;
static enum {
	RAM_ORGANISATION_4K,
	RAM_ORGANISATION_16K,
	RAM_ORGANISATION_64K
} ram_organisation = RAM_ORGANISATION_64K;
static uint16_t ram_mask = 0xffff;

static struct {
	const char *bas;
	const char *extbas;
	const char *altbas;
} rom_list[] = {
	{ NULL, "@dragon32", NULL },
	{ NULL, "@dragon64", "@dragon64_alt" },
	{ "@coco", "@coco_ext", NULL }
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

static int cycles;
static uint8_t read_cycle(uint16_t A);
static void write_cycle(uint16_t A, uint8_t D);
static void nvma_cycles(int ncycles);
static void vdg_fetch_handler(int nbytes, uint8_t *dest);

/**************************************************************************/

/* Create config array */
static int alloc_config_array(int size) {
	struct machine_config **new_list;
	int clear_from = num_configs;
	if (!configs) clear_from = 0;
	new_list = g_realloc(configs, size * sizeof(struct machine_config *));
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
	configs[i] = g_malloc0(sizeof(struct machine_config));
	configs[i]->name = g_strdup(config_templates[i].name);
	configs[i]->description = g_strdup(config_templates[i].description);
	configs[i]->architecture = config_templates[i].architecture;
	configs[i]->keymap = ANY_AUTO;
	configs[i]->tv_standard = config_templates[i].tv_standard;
	configs[i]->ram = config_templates[i].ram;
	configs[i]->index = i;
	return 0;
}

struct machine_config *machine_config_new(void) {
	struct machine_config *new;
	if (alloc_config_array(num_configs+1) != 0)
		return NULL;
	new = g_malloc0(sizeof(struct machine_config));
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
	if ((tmp = romlist_find("@dragon64"))) {
		arch = ARCH_DRAGON64;
	} else if ((tmp = romlist_find("@dragon32"))) {
		arch = ARCH_DRAGON32;
	} else if ((tmp = romlist_find("@coco"))) {
		arch = ARCH_COCO;
	} else {
		/* Fall back to this, which won't start up properly */
		arch = ARCH_DRAGON64;
	}
	if (tmp)
		g_free(tmp);
	return arch;
}

struct machine_config *machine_config_first_working(void) {
	return machine_config_by_arch(find_working_arch());
}

void machine_config_complete(struct machine_config *mc) {
	if (!mc->description) {
		mc->description = g_strdup(mc->name);
	}
	if (mc->tv_standard == ANY_AUTO)
		mc->tv_standard = TV_PAL;
	/* Various heuristics to find a working architecture */
	if (mc->architecture == ANY_AUTO) {
		/* TODO: checksum ROMs to help determine arch */
		if (mc->bas_rom) {
			mc->architecture = ARCH_COCO;
		} else if (mc->altbas_rom) {
			mc->architecture = ARCH_DRAGON64;
		} else if (mc->extbas_rom) {
			struct stat statbuf;
			mc->architecture = ARCH_DRAGON64;
			if (stat(mc->extbas_rom, &statbuf) == 0) {
				if (statbuf.st_size <= 0x2000) {
					mc->architecture = ARCH_COCO;
				}
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
	if (!mc->nobas && !mc->bas_rom && rom_list[mc->architecture].bas) {
		mc->bas_rom = g_strdup(rom_list[mc->architecture].bas);
	}
	if (!mc->noextbas && !mc->extbas_rom && rom_list[mc->architecture].extbas) {
		mc->extbas_rom = g_strdup(rom_list[mc->architecture].extbas);
	}
	if (!mc->noaltbas && !mc->altbas_rom && rom_list[mc->architecture].altbas) {
		mc->altbas_rom = g_strdup(rom_list[mc->architecture].altbas);
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

	m6809_read_cycle = read_cycle;
	m6809_write_cycle = write_cycle;
	m6809_nvma_cycles = nvma_cycles;
	vdg_fetch_bytes = vdg_fetch_handler;
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
	has_bas = 0;
	crc_bas = 0;
	if (!mc->nobas && mc->bas_rom) {
		char *tmp = romlist_find(mc->bas_rom);
		if (tmp) {
			int size = machine_load_rom(tmp, rom0 + 0x2000, sizeof(rom0) - 0x2000);
			if (size > 0) {
				has_bas = 1;
				crc_bas = crc32_block(CRC32_RESET, rom0 + 0x2000, size);
				LOG_DEBUG(2, "\tCRC = 0x%08x\n", crc_bas);
				if (IS_COCO && xroar_opt_force_crc_match) {
					/* force Color BASIC 1.3 */
					crc_bas = 0xd8f4d15e;
				}
			}
			g_free(tmp);
		}
	}
	/* ... Extended BASIC */
	has_extbas = 0;
	crc_extbas = 0;
	if (!mc->noextbas && mc->extbas_rom) {
		char *tmp = romlist_find(mc->extbas_rom);
		if (tmp) {
			int size = machine_load_rom(tmp, rom0, sizeof(rom0));
			if (size > 0) {
				has_extbas = 1;
				crc_extbas = crc32_block(CRC32_RESET, rom0, size);
				LOG_DEBUG(2, "\tCRC = 0x%08x\n", crc_extbas);
				if (xroar_opt_force_crc_match) {
					if (IS_DRAGON64) {
						/* force Dragon 64 BASIC */
						crc_extbas = 0x84f68bf9;
					} else if (IS_DRAGON32) {
						/* force Dragon 32 BASIC */
						crc_extbas = 0xe3879310;
					}
				}
			}
			if (size > 0x2000) {
				has_bas = 1;
				crc_bas = crc32_block(CRC32_RESET, rom0 + 0x2000, size - 0x2000);
			}
			g_free(tmp);
		}
	}
	/* ... Alternate BASIC ROM */
	has_altbas = 0;
	crc_altbas = 0;
	if (!mc->noaltbas && mc->altbas_rom) {
		char *tmp = romlist_find(mc->altbas_rom);
		if (tmp) {
			int size = machine_load_rom(tmp, rom1, sizeof(rom1));
			if (size > 0) {
				has_altbas = 1;
				crc_altbas = crc32_block(CRC32_RESET, rom1, size);
				LOG_DEBUG(2, "\tCRC = 0x%08x\n", crc_altbas);
			}
			g_free(tmp);
		}
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
	if (IS_COCO && machine_ram_size <= 0x2000)
		ram_organisation = RAM_ORGANISATION_4K;
	else if (IS_COCO && machine_ram_size <= 0x4000)
		ram_organisation = RAM_ORGANISATION_16K;
	else
		ram_organisation = RAM_ORGANISATION_64K;
	if (IS_DRAGON32 && machine_ram_size <= 0x8000) {
		unexpanded_dragon32 = 1;
		ram_mask = 0x7fff;
	} else {
		unexpanded_dragon32 = 0;
		ram_mask = 0xffff;
	}
}

void machine_reset(_Bool hard) {
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

void machine_run(int ncycles) {
	cycles += ncycles;
	m6809_running = 1;
	m6809_run();
}

static uint16_t decode_Z(uint16_t Z) {
	switch (ram_organisation) {
	case RAM_ORGANISATION_4K:
		return (Z & 0x3f) | ((Z & 0x3f00) >> 2) | ((~Z & 0x8000) >> 3);
	case RAM_ORGANISATION_16K:
		return (Z & 0x7f) | ((Z & 0x7f00) >> 1) | ((~Z & 0x8000) >> 1);
	case RAM_ORGANISATION_64K: default:
		return Z & ram_mask;
	}
}

/* Interface to SAM to decode and translate address */
static _Bool do_cpu_cycle(uint16_t A, _Bool RnW, int *S, uint16_t *Z) {
	uint16_t tmp_Z;
	int ncycles;
	_Bool is_ram_access = sam_run(A, RnW, S, &tmp_Z, &ncycles);
	if (is_ram_access) {
		*Z = decode_Z(tmp_Z);
	}
	cycles -= ncycles;
	if (cycles <= 0) m6809_running = 0;
	current_cycle += ncycles;
	while (EVENT_PENDING(MACHINE_EVENT_LIST))
		DISPATCH_NEXT_EVENT(MACHINE_EVENT_LIST);
	m6809_irq = PIA0.a.irq | PIA0.b.irq;
	m6809_firq = PIA1.a.irq | PIA1.b.irq;
	return is_ram_access;
}

static uint8_t read_D = 0;

static uint8_t read_cycle(uint16_t A) {
	int S;
	uint16_t Z;
	(void)do_cpu_cycle(A, 1, &S, &Z);
	switch (S) {
		case 0:
			if (Z < machine_ram_size)
				read_D = machine_ram[Z];
			break;
		case 1:
		case 2:
			read_D = machine_rom[A & 0x3fff];
			break;
		case 3:
			if (machine_cart)
				read_D = machine_cart->mem_data[A & 0x3fff];
			break;
		case 4:
			if (IS_COCO) {
				read_D = mc6821_read(&PIA0, A & 3);
			} else {
				if ((A & 4) == 0) {
					read_D = mc6821_read(&PIA0, A & 3);
				}
				/* Not yet implemented:
				if ((addr & 7) == 4) return serial_stuff;
				if ((addr & 7) == 5) return serial_stuff;
				if ((addr & 7) == 6) return serial_stuff;
				if ((addr & 7) == 7) return serial_stuff; */
			}
			break;
		case 5:
			read_D = mc6821_read(&PIA1, A & 3);
			break;
		case 6:
			if (machine_cart && machine_cart->io_read) {
				read_D = machine_cart->io_read(A);
			}
			break;
		default:
			break;
	}
#ifdef TRACE
	if (xroar_trace_enabled) m6809_trace_byte(read_D, A);
#endif
	return read_D;
}

static void write_cycle(uint16_t A, uint8_t D) {
	int S;
	uint16_t Z;
	_Bool is_ram_access = do_cpu_cycle(A, 0, &S, &Z);
	if ((S & 4) || unexpanded_dragon32) {
		switch (S) {
			case 1:
			case 2:
				D = machine_rom[A & 0x3fff];
				break;
			case 3:
				if (machine_cart)
					D = machine_cart->mem_data[A & 0x3fff];
				break;
			case 4:
				if (IS_COCO) {
					mc6821_write(&PIA0, A & 3, D);
				} else {
					if ((A & 4) == 0) {
						mc6821_write(&PIA0, A & 3, D);
					}
				}
				break;
			case 5:
				mc6821_write(&PIA1, A & 3, D);
				break;
			case 6:
				if (machine_cart && machine_cart->io_write) {
					machine_cart->io_write(A, D);
				}
				break;
			default:
				break;
		}
	}
	if (is_ram_access) {
		machine_ram[Z] = D;
	}
}

static void nvma_cycles(int ncycles) {
	int c = sam_nvma_cycles(ncycles);
	cycles -= c;
	if (cycles <= 0) m6809_running = 0;
	current_cycle += c;
	while (EVENT_PENDING(MACHINE_EVENT_LIST))
		DISPATCH_NEXT_EVENT(MACHINE_EVENT_LIST);
	m6809_irq = PIA0.a.irq | PIA0.b.irq;
	m6809_firq = PIA1.a.irq | PIA1.b.irq;
	read_D = machine_rom[0x3fff];
}

static void vdg_fetch_handler(int nbytes, uint8_t *dest) {
	while (nbytes > 0) {
		uint16_t V = 0;
		_Bool valid;
		int n = sam_vdg_bytes(nbytes, &V, &valid);
		if (dest) {
			if (valid) {
				V = decode_Z(V);
			}
			memcpy(dest, machine_ram + V, n);
			dest += n;
		}
		nbytes -= n;
	}
}

/* simplified read byte for use by convenience functions - ROM and RAM only */
uint8_t machine_read_byte(uint16_t A) {
	int S, ncycles;
	uint16_t Z;
	_Bool is_ram_access = sam_run(A, 1, &S, &Z, &ncycles);
	if (is_ram_access) {
		Z = decode_Z(Z);
		if (Z < machine_ram_size)
			return machine_ram[Z];
		return 0;
	}
	if (S == 1 || S == 2)
		return machine_rom[A & 0x3fff];
	if (S == 3 && machine_cart)
		return machine_cart->mem_data[A & 0x3fff];
	return 0;
}

/* simplified write byte for use by convenience functions - RAM only */
void machine_write_byte(uint16_t A, uint8_t D) {
	int S, ncycles;
	uint16_t Z;
	int is_ram_access = sam_run(A, 0, &S, &Z, &ncycles);
	if (!is_ram_access)
		return;
	Z = decode_Z(Z);
	if (Z < machine_ram_size)
		machine_ram[Z] = D;
}

/* simulate an RTS without otherwise affecting machine state */
void machine_op_rts(M6809State *cpu) {
	unsigned int new_pc = machine_read_byte(cpu->reg_s) << 8;
	new_pc |= machine_read_byte(cpu->reg_s + 1);
	cpu->reg_s += 2;
	cpu->reg_pc = new_pc;
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

int machine_load_rom(const char *path, uint8_t *dest, size_t max_size) {
	char *dot;
	FILE *fd;
	int size;

	if (path == NULL)
		return -1;
	dot = strrchr(path, '.');
	if (!(fd = fopen(path, "rb"))) {
		return -1;
	}
	if (dot && strcasecmp(dot, ".dgn") == 0) {
		LOG_DEBUG(2, "Loading DGN: %s\n", path);
		fread(dest, 1, 16, fd);
	} else {
		LOG_DEBUG(2, "Loading ROM: %s\n", path);
	}
	size = fread(dest, 1, max_size, fd);
	fclose(fd);
	return size;
}
