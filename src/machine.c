/*  Copyright 2003-2013 Ciaran Anscomb
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
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "pl_glib.h"

#include "breakpoint.h"
#include "cart.h"
#include "crc32.h"
#include "fs.h"
#include "hd6309.h"
#include "hd6309_trace.h"
#include "joystick.h"
#include "keyboard.h"
#include "logging.h"
#include "machine.h"
#include "mc6809.h"
#include "mc6809_trace.h"
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
static uint8_t *machine_rom;
static uint8_t rom0[0x4000];
static uint8_t rom1[0x4000];
static struct MC6809 *CPU0 = NULL;
static struct MC6821 *PIA0, *PIA1;
struct cart *machine_cart = NULL;
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
static _Bool have_acia = 0;

static struct {
	const char *bas;
	const char *extbas;
	const char *altbas;
} rom_list[] = {
	{ NULL, "@dragon32", NULL },
	{ NULL, "@dragon64", "@dragon64_alt" },
	{ "@coco", "@coco_ext", NULL }
};

static GSList *config_list = NULL;
static int num_configs = 0;

static void initialise_ram(void);

static int cycles;
static uint8_t read_cycle(uint16_t A);
static void write_cycle(uint16_t A, uint8_t D);
static void vdg_fetch_handler(int nbytes, uint8_t *dest);

static void machine_instruction_posthook(void *dptr);
static _Bool single_step = 0;
static int stop_signal = 0;

/**************************************************************************/

struct machine_config *machine_config_new(void) {
	struct machine_config *new;
	new = g_malloc0(sizeof(*new));
	new->index = num_configs;
	new->architecture = ANY_AUTO;
	new->cpu = CPU_MC6809;
	new->keymap = ANY_AUTO;
	new->tv_standard = ANY_AUTO;
	new->vdg_type = ANY_AUTO;
	new->ram = ANY_AUTO;
	new->cart_enabled = 1;
	config_list = g_slist_append(config_list, new);
	num_configs++;
	return new;
}

int machine_config_count(void) {
	return num_configs;
}

struct machine_config *machine_config_index(int i) {
	for (GSList *l = config_list; l; l = l->next) {
		struct machine_config *mc = l->data;
		if (mc->index == i)
			return mc;
	}
	return NULL;
}

struct machine_config *machine_config_by_name(const char *name) {
	if (!name) return NULL;
	for (GSList *l = config_list; l; l = l->next) {
		struct machine_config *mc = l->data;
		if (0 == strcmp(mc->name, name)) {
			return mc;
		}
	}
	return NULL;
}

struct machine_config *machine_config_by_arch(int arch) {
	for (GSList *l = config_list; l; l = l->next) {
		struct machine_config *mc = l->data;
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
		// Fall back to Dragon 64, which won't start up properly:
		LOG_WARN("Can't find ROMs for any machine.\n");
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
	if (mc->vdg_type == ANY_AUTO)
		mc->vdg_type = VDG_6847;
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
	// Determine a default DOS cartridge if necessary
	if (!mc->default_cart) {
		struct cart_config *cc = cart_find_working_dos(mc);
		if (cc)
			mc->default_cart = g_strdup(cc->name);
	}
}

static const char *machine_arch_string[] = {
	"dragon32",
	"dragon64",
	"coco",
};

static const char *machine_cpu_string[] = {
	"6809",
	"6309",
};

static const char *machine_tv_type_string[] = {
	"pal",
	"ntsc",
};

static const char *machine_vdg_type_string[] = {
	"6847",
	"6847t1",
};

void machine_config_print_all(void) {
	for (GSList *l = config_list; l; l = l->next) {
		struct machine_config *mc = l->data;
		printf("machine %s\n", mc->name);
		if (mc->description) printf("  machine-desc %s\n", mc->description);
		if (mc->architecture >= 0 && mc->architecture < G_N_ELEMENTS(machine_arch_string))
			printf("  machine-arch %s\n", machine_arch_string[mc->architecture]);
		if (mc->cpu >= 0 && mc->cpu < G_N_ELEMENTS(machine_cpu_string))
			printf("  machine-cpu %s\n", machine_cpu_string[mc->cpu]);
		if (mc->vdg_palette) printf("  machine-palette %s\n", mc->vdg_palette);
		if (mc->bas_rom) printf("  bas %s\n", mc->bas_rom);
		if (mc->extbas_rom) printf("  extbas %s\n", mc->extbas_rom);
		if (mc->altbas_rom) printf("  altbas %s\n", mc->altbas_rom);
		if (mc->nobas) printf("  nobas\n");
		if (mc->noextbas) printf("  noextbas\n");
		if (mc->noaltbas) printf("  noaltbas\n");
		if (mc->tv_standard >= 0 && mc->tv_standard < G_N_ELEMENTS(machine_tv_type_string))
			printf("  tv-type %s\n", machine_tv_type_string[mc->tv_standard]);
		if (mc->vdg_type >= 0 && mc->vdg_type < G_N_ELEMENTS(machine_vdg_type_string))
			printf("  vdg-type %s\n", machine_vdg_type_string[mc->vdg_type]);
		if (mc->ram >= 0) printf("  ram %d\n", mc->ram);
		if (mc->default_cart) printf("  machine-cart %s\n", mc->default_cart);
		if (mc->nodos) printf("  nodos\n");
		printf("\n");
	}
}

/* ---------------------------------------------------------------------- */

static void keyboard_update(void) {
	int row = PIA0->a.out_sink;
	int col = PIA0->b.out_source & PIA0->b.out_sink;
	int row_sink = 0xff, col_sink = 0xff;
	keyboard_read_matrix(row, col, &row_sink, &col_sink);
	PIA0->a.in_sink = (PIA0->a.in_sink & 0x80) | (row_sink & 0x7f);
	PIA0->b.in_sink = col_sink;
}

static void joystick_update(void) {
	int port = (PIA0->b.control_register & 0x08) >> 3;
	int axis = (PIA0->a.control_register & 0x08) >> 3;
	int dac_value = (PIA1->a.out_sink & 0xfc) + 2;
	int js_value = joystick_read_axis(port, axis);
	if (js_value >= dac_value)
		PIA0->a.in_sink |= 0x80;
	else
		PIA0->a.in_sink &= 0x7f;
	PIA0->a.in_sink &= ~(joystick_read_buttons() & 3);
}

static void update_sound_mux_source(void) {
	unsigned source = ((PIA0->b.control_register & (1<<3)) >> 2)
	                  | ((PIA0->a.control_register & (1<<3)) >> 3);
	sound_set_mux_source(source);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static void pia0a_data_preread(void) {
	keyboard_update();
	joystick_update();
}

#define pia0a_data_postwrite NULL
#define pia0a_control_postwrite update_sound_mux_source

#define pia0b_data_preread keyboard_update
#define pia0b_data_postwrite NULL
#define pia0b_control_postwrite update_sound_mux_source

static void pia0b_data_preread_coco64k(void) {
	keyboard_update();
	/* PB6 of PIA0 is linked to PB2 of PIA1 on 64K CoCos */
	if (PIA_VALUE_B(PIA1) & (1<<2)) {
		PIA0->b.in_source |= (1<<6);
		PIA0->b.in_sink |= (1<<6);
	} else {
		PIA0->b.in_source &= ~(1<<6);
		PIA0->b.in_sink &= ~(1<<6);
	}
}

#define pia1a_data_preread NULL

static void pia1a_data_postwrite(void) {
	sound_set_dac_level((float)(PIA_VALUE_A(PIA1) & 0xfc) / 252.);
	tape_update_output(PIA1->a.out_sink & 0xfc);
	if (IS_DRAGON) {
		keyboard_update();
		printer_strobe(PIA_VALUE_A(PIA1) & 0x02, PIA_VALUE_B(PIA0));
	}
}

static void pia1a_control_postwrite(void) {
	tape_update_motor(PIA1->a.control_register & 0x08);
}

#define pia1b_data_preread NULL

static void pia1b_data_preread_dragon(void) {
	if (printer_busy())
		PIA1->b.in_sink |= 0x01;
	else
		PIA1->b.in_sink &= ~0x01;
}

static void pia1b_data_preread_coco64k(void) {
	/* PB6 of PIA0 is linked to PB2 of PIA1 on 64K CoCos */
	if (PIA_VALUE_B(PIA0) & (1<<6)) {
		PIA1->b.in_source |= (1<<2);
		PIA1->b.in_sink |= (1<<2);
	} else {
		PIA1->b.in_source &= ~(1<<2);
		PIA1->b.in_sink &= ~(1<<2);
	}
}

static void pia1b_data_postwrite(void) {
	if (IS_DRAGON64) {
		_Bool is_32k = PIA_VALUE_B(PIA1) & 0x04;
		if (is_32k) {
			machine_rom = rom0;
			keyboard_set_chord_mode(keyboard_chord_mode_dragon_32k_basic);
		} else {
			machine_rom = rom1;
			keyboard_set_chord_mode(keyboard_chord_mode_dragon_64k_basic);
		}
	}
	// Single-bit sound
	_Bool sbs_enabled = !((PIA1->b.out_source ^ PIA1->b.out_sink) & (1<<1));
	_Bool sbs_level = PIA1->b.out_source & PIA1->b.out_sink & (1<<1);
	sound_set_sbs(sbs_enabled, sbs_level);
	// VDG mode
	vdg_set_mode(PIA1->b.out_source & PIA1->b.out_sink);
}

static void pia1b_control_postwrite(void) {
	sound_set_mux_enabled(PIA1->b.control_register & 0x08);
}

void machine_init(void) {
	sam_init();

	vdrive_init();
	vdg_init();
	tape_init();

	vdg_fetch_bytes = vdg_fetch_handler;
}

void machine_shutdown(void) {
	machine_remove_cart();
	tape_shutdown();
	vdrive_shutdown();
}

/* VDG edge delegates */

static void vdg_hs(void *dptr, _Bool level) {
	(void)dptr;
	if (level)
		mc6821_set_cx1(&PIA0->a);
	else
		mc6821_reset_cx1(&PIA0->a);
}

// PAL CoCos invert HS
static void vdg_hs_pal_coco(void *dptr, _Bool level) {
	(void)dptr;
	if (level)
		mc6821_reset_cx1(&PIA0->a);
	else
		mc6821_set_cx1(&PIA0->a);
}

static void vdg_fs(void *dptr, _Bool level) {
	(void)dptr;
	if (level)
		mc6821_set_cx1(&PIA0->b);
	else
		mc6821_reset_cx1(&PIA0->b);
}

/* Dragon parallel printer line delegate. */

//ACK is active low
static void printer_ack(void *dptr, _Bool ack) {
	(void)dptr;
	if (ack)
		mc6821_reset_cx1(&PIA1->a);
	else
		mc6821_set_cx1(&PIA1->a);
}

/* Sound output can feed back into the single bit sound pin when it's
 * configured as an input. */

static void single_bit_feedback(void *dptr, _Bool level) {
	(void)dptr;
	if (level) {
		PIA1->b.in_source &= ~(1<<1);
		PIA1->b.in_sink &= ~(1<<1);
	} else {
		PIA1->b.in_source |= (1<<1);
		PIA1->b.in_sink |= (1<<1);
	}
}

/* Tape audio delegate */

static void update_audio_from_tape(void *dptr, float value) {
	(void)dptr;
	sound_set_tape_level(value);
	if (value >= 0.5)
		PIA1->a.in_sink &= ~(1<<0);
	else
		PIA1->a.in_sink |= (1<<0);
}

/* Catridge signalling */

static void cart_firq(void *dptr, _Bool level) {
	(void)dptr;
	if (level)
		mc6821_set_cx1(&PIA1->b);
	else
		mc6821_reset_cx1(&PIA1->b);
}

static void cart_nmi(void *dptr, _Bool level) {
	(void)dptr;
	MC6809_NMI_SET(CPU0, level);
}

static void cart_halt(void *dptr, _Bool level) {
	(void)dptr;
	MC6809_HALT_SET(CPU0, level);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void machine_configure(struct machine_config *mc) {
	if (!mc) return;
	machine_config_complete(mc);
	if (mc->description) {
		LOG_DEBUG(2, "Machine: %s\n", mc->description);
	}
	// CPU
	if (CPU0) {
		CPU0->free(CPU0);
		CPU0 = NULL;
	}
	switch (mc->cpu) {
	case CPU_MC6809: default:
		CPU0 = mc6809_new();
		break;
	case CPU_HD6309:
		CPU0 = hd6309_new();
		break;
	}
	CPU0->read_cycle = read_cycle;
	CPU0->write_cycle = write_cycle;
	CPU0->instr_posthook_dptr = CPU0;
	// PIAs
	if (PIA0) {
		mc6821_free(PIA0);
		PIA0 = NULL;
	}
	if (PIA1) {
		mc6821_free(PIA1);
		PIA1 = NULL;
	}
	PIA0 = mc6821_new();
	PIA0->a.data_preread = pia0a_data_preread;
	PIA0->a.data_postwrite = pia0a_data_postwrite;
	PIA0->a.control_postwrite = pia0a_control_postwrite;
	PIA0->b.data_preread = pia0b_data_preread;
	PIA0->b.data_postwrite = pia0b_data_postwrite;
	PIA0->b.control_postwrite = pia0b_control_postwrite;
	PIA1 = mc6821_new();
	PIA1->a.data_preread = pia1a_data_preread;
	PIA1->a.data_postwrite = pia1a_data_postwrite;
	PIA1->a.control_postwrite = pia1a_control_postwrite;
	PIA1->b.data_preread = pia1b_data_preread;
	PIA1->b.data_postwrite = pia1b_data_postwrite;
	PIA1->b.control_postwrite = pia1b_control_postwrite;

	// Single-bit sound feedback
#ifndef FAST_SOUND
	sound_sbs_feedback = (sound_feedback_delegate){single_bit_feedback, NULL};
#endif

	// Tape
	tape_update_audio = (tape_audio_delegate){update_audio_from_tape, NULL};

	// VDG
	vdg_t1 = (mc->vdg_type == VDG_6847T1);
	if (IS_COCO && IS_PAL) {
		vdg_signal_hs = (vdg_edge_delegate){vdg_hs_pal_coco, NULL};
	} else {
		vdg_signal_hs = (vdg_edge_delegate){vdg_hs, NULL};
	}
	vdg_signal_fs = (vdg_edge_delegate){vdg_fs, NULL};

	// Printer
	printer_signal_ack = (printer_line_delegate){printer_ack, NULL};

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
				if (IS_COCO && xroar_cfg.force_crc_match) {
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
				if (xroar_cfg.force_crc_match) {
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

	/* Default all PIA connections to unconnected (no source, no sink) */
	PIA0->b.in_source = 0;
	PIA1->b.in_source = 0;
	PIA0->a.in_sink = PIA0->b.in_sink = 0xff;
	PIA1->a.in_sink = PIA1->b.in_sink = 0xff;
	/* Machine-specific PIA connections */
	if (IS_DRAGON) {
		/* Centronics printer port - !BUSY */
		PIA1->b.in_source |= (1<<0);
	}
	if (IS_DRAGON64) {
		have_acia = 1;
		PIA1->b.in_source |= (1<<2);
	} else if (IS_COCO && machine_ram_size <= 0x1000) {
		/* 4K CoCo ties PB2 of PIA1 low */
		PIA1->b.in_sink &= ~(1<<2);
	} else if (IS_COCO && machine_ram_size <= 0x4000) {
		/* 16K CoCo pulls PB2 of PIA1 high */
		PIA1->b.in_source |= (1<<2);
	}
	PIA0->b.data_preread = pia0b_data_preread;
	if (IS_DRAGON) {
		/* Dragons need to poll printer BUSY state */
		PIA1->b.data_preread = pia1b_data_preread_dragon;
	}
	if (IS_COCO && machine_ram_size > 0x4000) {
		/* 64K CoCo connects PB6 of PIA0 to PB2 of PIA1->
		 * Deal with this through a postwrite. */
		PIA0->b.data_preread = pia0b_data_preread_coco64k;
		PIA1->b.data_preread = pia1b_data_preread_coco64k;
	}

	if (IS_DRAGON) {
		keyboard_set_chord_mode(keyboard_chord_mode_dragon_32k_basic);
	} else {
		keyboard_set_chord_mode(keyboard_chord_mode_coco_basic);
	}

	unexpanded_dragon32 = 0;
	ram_mask = 0xffff;

	if (IS_COCO) {
		if (machine_ram_size <= 0x2000) {
			ram_organisation = RAM_ORGANISATION_4K;
			ram_mask = 0x3f3f;
		} else if (machine_ram_size <= 0x4000) {
			ram_organisation = RAM_ORGANISATION_16K;
		} else {
			ram_organisation = RAM_ORGANISATION_64K;
			if (machine_ram_size <= 0x8000)
				ram_mask = 0x7fff;
		}
	}

	if (IS_DRAGON) {
		ram_organisation = RAM_ORGANISATION_64K;
		if (IS_DRAGON32 && machine_ram_size <= 0x8000) {
			unexpanded_dragon32 = 1;
			ram_mask = 0x7fff;
		}
	}

#ifndef FAST_SOUND
	machine_select_fast_sound(xroar_cfg.fast_sound);
#endif
}

void machine_reset(_Bool hard) {
	xroar_set_keymap(xroar_machine_config->keymap);
	switch (xroar_machine_config->tv_standard) {
	case TV_PAL: default:
		xroar_select_cross_colour(CROSS_COLOUR_OFF);
		break;
	case TV_NTSC:
		xroar_select_cross_colour(CROSS_COLOUR_KBRW);
		break;
	}
	if (hard) {
		initialise_ram();
	}
	mc6821_reset(PIA0);
	mc6821_reset(PIA1);
	if (machine_cart && machine_cart->reset) {
		machine_cart->reset(machine_cart);
	}
	sam_reset();
	CPU0->reset(CPU0);
#ifdef TRACE
	mc6809_trace_reset();
	hd6309_trace_reset();
#endif
	vdg_reset();
	tape_reset();
}

int machine_run(int ncycles) {
	stop_signal = 0;
	cycles += ncycles;
	CPU0->running = 1;
	CPU0->run(CPU0);
	return stop_signal;
}

void machine_single_step(void) {
	single_step = 1;
	CPU0->running = 0;
	CPU0->instruction_posthook = machine_instruction_posthook;
	do {
		CPU0->run(CPU0);
	} while (single_step);
	vdg_set_mode(PIA1->b.out_source & PIA1->b.out_sink);
	if (xroar_cfg.trace_enabled)
		CPU0->instruction_posthook = NULL;
}

/*
 * Stop emulation and set stop_signal to reflect the reason.
 */

void machine_signal(int sig) {
	vdg_set_mode(PIA1->b.out_source & PIA1->b.out_sink);
	stop_signal = sig;
	CPU0->running = 0;
}

void machine_set_trace(_Bool trace_on) {
	if (trace_on || single_step)
		CPU0->instruction_posthook = machine_instruction_posthook;
	else
		CPU0->instruction_posthook = NULL;
}

/*
 * Device inspection.
 */

int machine_num_cpus(void) {
	return 1;
}

int machine_num_pias(void) {
	return 2;
}

struct MC6809 *machine_get_cpu(int n) {
	if (n != 0)
		return NULL;
	return CPU0;
}

struct MC6821 *machine_get_pia(int n) {
	if (n == 0)
		return PIA0;
	if (n == 1)
		return PIA1;
	return NULL;
}

/*
 * Used when single-stepping or tracing.
 */

static void machine_instruction_posthook(void *dptr) {
	struct MC6809 *cpu = dptr;
	if (xroar_cfg.trace_enabled) {
		switch (xroar_machine_config->cpu) {
		case CPU_MC6809: default:
			mc6809_trace_print(cpu);
			break;
		case CPU_HD6309:
			hd6309_trace_print(cpu);
			break;
		}
	}
	single_step = 0;
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
	if (cycles <= 0) CPU0->running = 0;
	event_current_tick += ncycles;
	event_run_queue(&MACHINE_EVENT_LIST);
	MC6809_IRQ_SET(CPU0, PIA0->a.irq | PIA0->b.irq);
	MC6809_FIRQ_SET(CPU0, PIA1->a.irq | PIA1->b.irq);
	return is_ram_access;
}

/* Same as do_cpu_cycle, but for debugging accesses */
static _Bool debug_cpu_cycle(uint16_t A, _Bool RnW, int *S, uint16_t *Z) {
	uint16_t tmp_Z;
	_Bool is_ram_access = sam_run(A, RnW, S, &tmp_Z, NULL);
	if (is_ram_access) {
		*Z = decode_Z(tmp_Z);
	}
	MC6809_IRQ_SET(CPU0, PIA0->a.irq | PIA0->b.irq);
	MC6809_FIRQ_SET(CPU0, PIA1->a.irq | PIA1->b.irq);
	return is_ram_access;
}

static uint8_t read_D = 0;

static uint8_t read_cycle(uint16_t A) {
	int S;
	uint16_t Z = 0;
	_Bool is_ram_access = do_cpu_cycle(A, 1, &S, &Z);
	/* Thanks to CrAlt on #coco_chat for verifying that RAM accesses
	 * produce a different "null" result on his 16K CoCo */
	if (is_ram_access)
		read_D = 0xff;
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
				machine_cart->read(machine_cart, A, 0, &read_D);
			break;
		case 4:
			if (IS_COCO) {
				read_D = mc6821_read(PIA0, A & 3);
			} else {
				if ((A & 4) == 0) {
					read_D = mc6821_read(PIA0, A & 3);
				} else {
					if (have_acia) {
						/* XXX Dummy ACIA reads */
						switch (A & 3) {
						default:
						case 0:  /* Receive Data */
						case 3:  /* Control */
							read_D = 0x00;
							break;
						case 2:  /* Command */
							read_D = 0x02;
							break;
						case 1:  /* Status */
							read_D = 0x10;
							break;
						}
					}
				}
			}
			break;
		case 5:
			read_D = mc6821_read(PIA1, A & 3);
			break;
		case 6:
			if (machine_cart)
				machine_cart->read(machine_cart, A, 1, &read_D);
			break;
		default:
			break;
	}
#ifdef TRACE
	if (xroar_cfg.trace_enabled) {
		switch (xroar_machine_config->cpu) {
		case CPU_MC6809: default:
			mc6809_trace_byte(read_D, A);
			break;
		case CPU_HD6309:
			hd6309_trace_byte(read_D, A);
			break;
		}
	}
#endif
	bp_wp_read_hook(A);
	return read_D;
}

static void write_cycle(uint16_t A, uint8_t D) {
	int S;
	uint16_t Z = 0;
	// Changing the SAM VDG mode can affect its idea of the current VRAM
	// address, so get the VDG output up to date:
	if (A >= 0xffc0 && A < 0xffc6) {
		vdg_set_mode(PIA1->b.out_source & PIA1->b.out_sink);
	}
	_Bool is_ram_access = do_cpu_cycle(A, 0, &S, &Z);
	if ((S & 4) || unexpanded_dragon32) {
		switch (S) {
			case 1:
			case 2:
				D = machine_rom[A & 0x3fff];
				break;
			case 3:
				if (machine_cart)
					machine_cart->write(machine_cart, A, 0, D);
				break;
			case 4:
				if (IS_COCO) {
					mc6821_write(PIA0, A & 3, D);
				} else {
					if ((A & 4) == 0) {
						mc6821_write(PIA0, A & 3, D);
					}
				}
				break;
			case 5:
				mc6821_write(PIA1, A & 3, D);
				break;
			case 6:
				if (machine_cart)
					machine_cart->write(machine_cart, A, 1, D);
				break;
			// Should call cart's write() whatever the address and
			// set P2 accordingly, but for now this enables orch90:
			case 7:
				if (machine_cart)
					machine_cart->write(machine_cart, A, 0, D);
				break;
			default:
				break;
		}
	}
	if (is_ram_access) {
		machine_ram[Z] = D;
	}
	bp_wp_write_hook(A);
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

void machine_toggle_pause(void) {
	CPU0->halt = !CPU0->halt;
}

/* Read a byte without advancing clock.  Used for debugging & breakpoints. */

uint8_t machine_read_byte(uint16_t A) {
	uint8_t D = read_D;
	int S;
	uint16_t Z = 0;
	_Bool is_ram_access = debug_cpu_cycle(A, 1, &S, &Z);
	if (is_ram_access)
		D = 0xff;
	switch (S) {
		case 0:
			if (Z < machine_ram_size)
				D = machine_ram[Z];
			break;
		case 1:
		case 2:
			D = machine_rom[A & 0x3fff];
			break;
		case 3:
			if (machine_cart)
				machine_cart->read(machine_cart, A, 0, &D);
			break;
		case 4:
			if (IS_COCO) {
				D = mc6821_read(PIA0, A & 3);
			} else {
				if ((A & 4) == 0) {
					D = mc6821_read(PIA0, A & 3);
				} else {
					if (have_acia) {
						/* XXX Dummy ACIA reads */
						switch (A & 3) {
						default:
						case 0:  /* Receive Data */
						case 3:  /* Control */
							D = 0x00;
							break;
						case 2:  /* Command */
							D = 0x02;
							break;
						case 1:  /* Status */
							D = 0x10;
							break;
						}
					}
				}
			}
			break;
		case 5:
			D = mc6821_read(PIA1, A & 3);
			break;
		case 6:
			if (machine_cart)
				machine_cart->read(machine_cart, A, 1, &D);
			break;
		default:
			break;
	}
	return D;
}

/* Write a byte without advancing clock.  Used for debugging & breakpoints. */

void machine_write_byte(uint16_t A, uint8_t D) {
	int S;
	uint16_t Z = 0;
	// Changing the SAM VDG mode can affect its idea of the current VRAM
	// address, so get the VDG output up to date:
	if (A >= 0xffc0 && A < 0xffc6) {
		vdg_set_mode(PIA1->b.out_source & PIA1->b.out_sink);
	}
	_Bool is_ram_access = debug_cpu_cycle(A, 0, &S, &Z);
	if ((S & 4) || unexpanded_dragon32) {
		switch (S) {
			case 1:
			case 2:
				D = machine_rom[A & 0x3fff];
				break;
			case 3:
				if (machine_cart)
					machine_cart->write(machine_cart, A, 0, D);
				break;
			case 4:
				if (IS_COCO) {
					mc6821_write(PIA0, A & 3, D);
				} else {
					if ((A & 4) == 0) {
						mc6821_write(PIA0, A & 3, D);
					}
				}
				break;
			case 5:
				mc6821_write(PIA1, A & 3, D);
				break;
			case 6:
				if (machine_cart)
					machine_cart->write(machine_cart, A, 1, D);
				break;
			default:
				break;
		}
	}
	if (is_ram_access) {
		machine_ram[Z] = D;
	}
}

/* simulate an RTS without otherwise affecting machine state */
void machine_op_rts(struct MC6809 *cpu) {
	unsigned int new_pc = machine_read_byte(cpu->reg_s) << 8;
	new_pc |= machine_read_byte(cpu->reg_s + 1);
	cpu->reg_s += 2;
	cpu->reg_pc = new_pc;
}

#ifndef FAST_SOUND
void machine_set_fast_sound(_Bool fast) {
	xroar_cfg.fast_sound = fast;
}

void machine_select_fast_sound(_Bool fast) {
	if (ui_module->fast_sound_changed_cb) {
		ui_module->fast_sound_changed_cb(fast);
	}
	machine_set_fast_sound(fast);
}
#endif

void machine_insert_cart(struct cart *c) {
	machine_remove_cart();
	if (c) {
		assert(c->read != NULL);
		assert(c->write != NULL);
		machine_cart = c;
		c->signal_firq = (cart_signal_delegate){cart_firq, NULL};
		c->signal_nmi = (cart_signal_delegate){cart_nmi, NULL};
		c->signal_halt = (cart_signal_delegate){cart_halt, NULL};
	}
}

void machine_remove_cart(void) {
	cart_free(machine_cart);
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
	if (dot && g_ascii_strcasecmp(dot, ".dgn") == 0) {
		LOG_DEBUG(2, "Loading DGN: %s\n", path);
		fread(dest, 1, 16, fd);
	} else {
		LOG_DEBUG(2, "Loading ROM: %s\n", path);
	}
	size = fread(dest, 1, max_size, fd);
	fclose(fd);
	return size;
}
