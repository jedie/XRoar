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

#include "types.h"
#include "logging.h"
#include "cart.h"
#include "events.h"
#include "fs.h"
#include "hexs19.h"
#include "joystick.h"
#include "keyboard.h"
#include "m6809.h"
#include "m6809_trace.h"
#include "machine.h"
#include "module.h"
#include "path.h"
#include "sam.h"
#include "snapshot.h"
#include "tape.h"
#include "vdisk.h"
#include "vdrive.h"
#include "xconfig.h"
#include "xroar.h"

#ifdef FAST_VDG
# define DEFAULT_CCR CROSS_COLOUR_SIMPLE
#else
# define DEFAULT_CCR CROSS_COLOUR_5BIT
#endif

/**************************************************************************/
/* Command line arguments */

/* Emulator interface */
static const char *xroar_opt_ui = NULL;
static const char *xroar_opt_filereq = NULL;
static const char *xroar_opt_vo = NULL;
char *xroar_opt_gl_filter = NULL;
static const char *xroar_opt_ao = NULL;
#ifndef FAST_SOUND
int xroar_fast_sound = 0;
#endif
int xroar_fullscreen = 0;
static const char *xroar_opt_ccr = NULL;
int xroar_frameskip = 0;
char *xroar_opt_keymap = NULL;
char *xroar_opt_joy_left = NULL;
char *xroar_opt_joy_right = NULL;
int xroar_tapehack = 0;
#ifdef TRACE
int xroar_trace_enabled = 0;
#else
# define xroar_trace_enabled (0)
#endif

/* Emulated machine */
char *xroar_opt_machine = NULL;
char *xroar_opt_bas = NULL;
char *xroar_opt_extbas = NULL;
char *xroar_opt_altbas = NULL;
int xroar_opt_nobas = 0;
int xroar_opt_noextbas = 0;
int xroar_opt_noaltbas = 0;
char *xroar_opt_dostype = NULL;
char *xroar_opt_dos = NULL;
int xroar_opt_nodos = 0;
int xroar_opt_tv = TV_PAL;
static void set_pal(void);
static void set_ntsc(void);
int xroar_opt_ram = 0;

/* Automatically load or run files */
static const char *xroar_opt_load = NULL;
static const char *xroar_opt_run = NULL;

/* CLI information to hand off to config reader */
static struct xconfig_option xroar_options[] = {
	/* Emulator interface */
	{ XCONFIG_STRING,   "ui",           &xroar_opt_ui },
	{ XCONFIG_STRING,   "filereq",      &xroar_opt_filereq },
	{ XCONFIG_STRING,   "vo",           &xroar_opt_vo },
	{ XCONFIG_STRING,   "gl-filter",    &xroar_opt_gl_filter },
	{ XCONFIG_STRING,   "ao",           &xroar_opt_ao },
#ifndef FAST_SOUND
	{ XCONFIG_BOOL,     "fast-sound",   &xroar_fast_sound },
#endif
	{ XCONFIG_BOOL,     "fs",           &xroar_fullscreen },
	{ XCONFIG_STRING,   "ccr",          &xroar_opt_ccr },
	{ XCONFIG_INT,      "fskip",        &xroar_frameskip },
	{ XCONFIG_STRING,   "keymap",       &xroar_opt_keymap },
	{ XCONFIG_STRING,   "joy-left",     &xroar_opt_joy_left },
	{ XCONFIG_STRING,   "joy-right",    &xroar_opt_joy_right },
	{ XCONFIG_BOOL,     "tapehack",     &xroar_tapehack },
#ifdef TRACE
	{ XCONFIG_BOOL,     "trace",        &xroar_trace_enabled },
#endif
	/* Emulated machine */
	{ XCONFIG_STRING,   "machine",      &xroar_opt_machine },
	{ XCONFIG_STRING,   "bas",          &xroar_opt_bas },
	{ XCONFIG_STRING,   "extbas",       &xroar_opt_extbas },
	{ XCONFIG_STRING,   "altbas",       &xroar_opt_altbas },
	{ XCONFIG_BOOL,     "nobas",        &xroar_opt_nobas },
	{ XCONFIG_BOOL,     "noextbas",     &xroar_opt_noextbas },
	{ XCONFIG_BOOL,     "noaltbas",     &xroar_opt_noaltbas },
	{ XCONFIG_STRING,   "dostype",      &xroar_opt_dostype },
	{ XCONFIG_STRING,   "dos",          &xroar_opt_dos },
	{ XCONFIG_BOOL,     "nodos",        &xroar_opt_nodos },
	{ XCONFIG_CALL_0,   "pal",          &set_pal },
	{ XCONFIG_CALL_0,   "ntsc",         &set_ntsc },
	{ XCONFIG_INT,      "ram",          &xroar_opt_ram },
	/* Automatically load or run files */
	{ XCONFIG_STRING,   "load",         &xroar_opt_load },
	{ XCONFIG_STRING,   "cartna",       &xroar_opt_load },
	{ XCONFIG_STRING,   "snap",         &xroar_opt_load },
	{ XCONFIG_STRING,   "run",          &xroar_opt_run },
	{ XCONFIG_STRING,   "cart",         &xroar_opt_run },
	{ XCONFIG_END, NULL, NULL }
};

/**************************************************************************/
/* Global flags */

int xroar_cross_colour_renderer = DEFAULT_CCR;
int xroar_noratelimit = 0;

static struct {
	const char *option;
	const char *description;
	int value;
} cross_colour_options[NUM_CROSS_COLOUR_RENDERERS] = {
	{ "simple", "four colour palette", CROSS_COLOUR_SIMPLE },
#ifndef FAST_VDG
	{ "5bit",   "5-bit lookup table",  CROSS_COLOUR_5BIT   },
#endif
};

/**************************************************************************/

const char *xroar_rom_path;
const char *xroar_conf_path;

event_t *xroar_ui_events = NULL;
event_t *xroar_machine_events = NULL;

static event_t load_file_event;
static void do_load_file(void);
static const char *load_file = NULL;
static int load_file_type = FILETYPE_UNKNOWN;
static int autorun_loaded_file = 0;

static struct {
	const char *ext;
	int filetype;
} filetypes[] = {
	{ "VDK", FILETYPE_VDK },
	{ "JVC", FILETYPE_JVC },
	{ "DSK", FILETYPE_JVC },
	{ "DMK", FILETYPE_DMK },
	{ "BIN", FILETYPE_BIN },
	{ "HEX", FILETYPE_HEX },
	{ "CAS", FILETYPE_CAS },
	{ "WAV", FILETYPE_WAV },
	{ "SN",  FILETYPE_SNA },
	{ "ROM", FILETYPE_ROM },
	{ "BAS", FILETYPE_ASC },
	{ "ASC", FILETYPE_ASC },
	{ NULL, FILETYPE_UNKNOWN }
};

static void do_m6809_sync(void);
static unsigned int trace_read_byte(unsigned int addr);
static void trace_done_instruction(M6809State *state);

/**************************************************************************/

static void set_pal(void) {
	xroar_opt_tv = TV_PAL;
}

static void set_ntsc(void) {
	xroar_opt_tv = TV_NTSC;
}

static void versiontext(void) {
	puts(
"XRoar " VERSION "\n"
"Copyright (C) 2010 Ciaran Anscomb\n"
"This is free software.  You may redistribute copies of it under the terms of\n"
"the GNU General Public License <http://www.gnu.org/licenses/gpl.html>.\n"
"There is NO WARRANTY, to the extent permitted by law."
	    );
}

static void helptext(void) {
	puts(
"Usage: xroar [OPTION]...\n"
"XRoar is a Dragon emulator.  Due to hardware similarities, XRoar also\n"
"emulates the Tandy Colour Computer (CoCo) models 1 & 2.\n"

"\n Emulator interface:\n"
"  -ui MODULE            user-interface module (-ui help for list)\n"
"  -vo MODULE            video module (-vo help for list)\n"
#ifdef HAVE_SDLGL
"  -gl-filter FILTER     OpenGL texture filter (auto, linear, nearest)\n"
#endif
"  -ao MODULE            audio module (-ao help for list)\n"
#ifndef FAST_SOUND
"  -fast-sound           faster but less accurate sound\n"
#endif
"  -fs                   start emulator full-screen if possible\n"
"  -ccr RENDERER         cross-colour renderer (-ccr help for list)\n"
"  -fskip FRAMES         frameskip (default: 0)\n"
"  -keymap CODE          host keyboard type (uk, us, fr, de)\n"
"  -joy-left JOYSPEC     map left joystick\n"
"  -joy-right JOYSPEC    map right joystick\n"
"  -tapehack             enable tape hacking mode\n"
#ifdef TRACE
"  -trace                start with trace mode on\n"
#endif

"\n Emulated machine:\n"
"  -machine MACHINE      emulated machine (-machine help for list)\n"
"  -bas FILENAME         BASIC ROM to use (CoCo only)\n"
"  -extbas FILENAME      Extended BASIC ROM to use\n"
"  -altbas FILENAME      alternate BASIC ROM (Dragon 64)\n"
"  -noextbas             disable Extended BASIC\n"
"  -dostype DOS          type of DOS cartridge (-dostype help for list)\n"
"  -dos FILENAME         DOS ROM (or CoCo Disk BASIC)\n"
"  -nodos                disable DOS (ROM and hardware emulation)\n"
"  -pal                  emulate PAL (50Hz) video\n"
"  -ntsc                 emulate NTSC (60Hz) video\n"
"  -ram KBYTES           specify amount of RAM in K\n"

"\n Automatically load or run files:\n"
"  -load FILENAME        load or attach FILENAME\n"
"  -run FILENAME         load or attach FILENAME and attempt autorun\n"

"\n Other options:\n"
"  -h, --help            display this help and exit\n"
"      --version         output version information and exit\n"

"\nA JOYSPEC maps physical joystick axes and buttons to one of the emulated\n"
"machine's joysticks.  JOYSPEC is of the form 'DEV,AXIS:DEV,AXIS:DEV,BUTTON',\n"
"mapping two axes and a button to the X, Y and firebutton on the emulated\n"
"joystick respectively."
	);
}

#ifndef ROMPATH
# define ROMPATH "."
#endif
#ifndef CONFPATH
# define CONFPATH "."
#endif

int xroar_init(int argc, char **argv) {
	int argn = 1, ret;
	char *conffile;

	xroar_rom_path = getenv("XROAR_ROM_PATH");
	if (!xroar_rom_path)
		xroar_rom_path = ROMPATH;

	xroar_conf_path = getenv("XROAR_CONF_PATH");
	if (!xroar_conf_path)
		xroar_conf_path = CONFPATH;

	/* If a configuration file is found, parse it */
	conffile = find_in_path(xroar_conf_path, "xroar.conf");
	if (conffile) {
		xconfig_parse_file(xroar_options, conffile);
	}
	/* Parse command line options */
	ret = xconfig_parse_cli(xroar_options, argc, argv, &argn);
	if (ret == XCONFIG_MISSING_ARG) {
		LOG_DEBUG(0, "%s: missing argument to `%s'\n", argv[0], argv[argn]);
		exit(1);
	} else if (ret == XCONFIG_BAD_OPTION) {
		if (0 == strcmp(argv[argn], "-h")
				|| 0 == strcmp(argv[argn], "--help")) {
			helptext();
			exit(0);
		} else if (0 == strcmp(argv[argn], "--version")) {
			versiontext();
			exit(0);
		} else {
			LOG_DEBUG(0, "%s: unrecognised option `%s'\n", argv[0], argv[argn]);
			exit(1);
		}
	}

	/* Select a UI module then, possibly using lists specified in that
	 * module, select all other modules */
	ui_module = (UIModule *)module_select_by_arg((Module **)ui_module_list, xroar_opt_ui);
	if (ui_module == NULL) {
		LOG_DEBUG(0, "%s: ui module `%s' not found\n", argv[0], xroar_opt_ui);
		exit(1);
	}
	/* Select file requester module */
	if (ui_module->filereq_module_list != NULL)
		filereq_module_list = ui_module->filereq_module_list;
	filereq_module = (FileReqModule *)module_select_by_arg((Module **)filereq_module_list, xroar_opt_filereq);
	/* Select video module */
	if (ui_module->video_module_list != NULL)
		video_module_list = ui_module->video_module_list;
	video_module = (VideoModule *)module_select_by_arg((Module **)video_module_list, xroar_opt_vo);
	/* Select sound module */
	if (ui_module->sound_module_list != NULL)
		sound_module_list = ui_module->sound_module_list;
	sound_module = (SoundModule *)module_select_by_arg((Module **)sound_module_list, xroar_opt_ao);
	/* Select keyboard module */
	if (ui_module->keyboard_module_list != NULL)
		keyboard_module_list = ui_module->keyboard_module_list;
	keyboard_module = NULL;
	/* Select joystick module */
	if (ui_module->joystick_module_list != NULL)
		joystick_module_list = ui_module->joystick_module_list;
	joystick_module = NULL;

	/* Check other command-line options */
	if (xroar_frameskip < 0)
		xroar_frameskip = 0;
	if (xroar_opt_ccr) {
		int i;
		if (0 == strcmp(xroar_opt_ccr, "help")) {
			for (i = 0; i < NUM_CROSS_COLOUR_RENDERERS; i++) {
				printf("\t%-10s%s\n", cross_colour_options[i].option, cross_colour_options[i].description);
			}
			exit(0);
		}
		for (i = 0; i < NUM_CROSS_COLOUR_RENDERERS; i++) {
			if (!strcmp(xroar_opt_ccr, cross_colour_options[i].option)) {
				xroar_cross_colour_renderer = cross_colour_options[i].value;
			}
		}
	}
	if (xroar_opt_load) {
		load_file = xroar_opt_load;
		autorun_loaded_file = 0;
	} else if (xroar_opt_run) {
		load_file = xroar_opt_run;
		autorun_loaded_file = 1;
	} else if (argn < argc) {
		load_file = strdup(argv[argn]);
		autorun_loaded_file = 1;
	}

	machine_getargs();

	/* Disable DOS if a cassette or cartridge is being loaded */
	if (load_file) {
		load_file_type = xroar_filetype_by_ext(load_file);
		switch (load_file_type) {
			case FILETYPE_CAS:
			case FILETYPE_ASC:
			case FILETYPE_WAV:
			case FILETYPE_ROM:
				requested_config.dos_type = DOS_NONE;
				break;
			default:
				break;
		}
	}

	/* Initialise everything */
	current_cycle = 0;
	/* ... modules */
	module_init((Module *)ui_module);
	filereq_module = (FileReqModule *)module_init_from_list((Module **)filereq_module_list, (Module *)filereq_module);
	if (filereq_module == NULL && filereq_module_list != NULL) {
		LOG_WARN("No file requester module initialised.\n");
	}
	video_module = (VideoModule *)module_init_from_list((Module **)video_module_list, (Module *)video_module);
	if (video_module == NULL && video_module_list != NULL) {
		LOG_ERROR("No video module initialised.\n");
		return 1;
	}
	sound_module = (SoundModule *)module_init_from_list((Module **)sound_module_list, (Module *)sound_module);
	if (sound_module == NULL && sound_module_list != NULL) {
		LOG_ERROR("No sound module initialised.\n");
		return 1;
	}
	keyboard_module = (KeyboardModule *)module_init_from_list((Module **)keyboard_module_list, (Module *)keyboard_module);
	if (keyboard_module == NULL && keyboard_module_list != NULL) {
		LOG_WARN("No keyboard module initialised.\n");
	}
	joystick_module = (JoystickModule *)module_init_from_list((Module **)joystick_module_list, (Module *)joystick_module);
	if (joystick_module == NULL && joystick_module_list != NULL) {
		LOG_WARN("No joystick module initialised.\n");
	}
	/* ... subsystems */
	keyboard_init();
	joystick_init();
	machine_init();

	/* Load carts before initial reset */
	if (load_file && load_file_type == FILETYPE_ROM) {
		xroar_load_file(load_file, autorun_loaded_file);
		load_file = NULL;
	}
	/* Reset everything */
	machine_reset(RESET_HARD);
	if (load_file) {
		if (load_file_type == FILETYPE_SNA) {
			/* Load snapshots immediately */
			xroar_load_file(load_file, autorun_loaded_file);
		} else if (!autorun_loaded_file && load_file_type != FILETYPE_BIN
				&& load_file_type != FILETYPE_HEX) {
			/* Everything else except CoCo binaries and hex
			 * records can be attached now if not autorunning */
			xroar_load_file(load_file, 0);
		} else {
			/* For everything else, defer loading the file */
			event_init(&load_file_event);
			load_file_event.dispatch = do_load_file;
			load_file_event.at_cycle = current_cycle + OSCILLATOR_RATE * 2;
			event_queue(&UI_EVENT_LIST, &load_file_event);
		}
	}
	return 0;
}

void xroar_shutdown(void) {
	machine_shutdown();
	module_shutdown((Module *)joystick_module);
	module_shutdown((Module *)keyboard_module);
	module_shutdown((Module *)sound_module);
	module_shutdown((Module *)video_module);
	module_shutdown((Module *)filereq_module);
	module_shutdown((Module *)ui_module);
}

static void tapehack_instruction_hook(M6809State *cpu_state) {
	if (IS_COCO) {
		if (cpu_state->reg_pc == 0xa719) {
			tape_sync();
		}
		if (cpu_state->reg_pc == 0xa75c) {
			tape_bit_out(cpu_state->reg_cc & 1);
		}
		if (cpu_state->reg_pc == 0xa77c) {
			tape_desync(256);
		}
	} else {
		if (cpu_state->reg_pc == 0xb94d) {
			tape_sync();
		}
		if (cpu_state->reg_pc == 0xbdac) {
			tape_bit_out(cpu_state->reg_cc & 1);
		}
		if (cpu_state->reg_pc == 0xbde7) {
			tape_desync(256);
		}
		if (cpu_state->reg_pc == 0xb97e) {
			tape_desync(2);
		}
	}
}

void xroar_mainloop(void) {
	m6809_write_cycle = sam_store_byte;
	m6809_nvma_cycles = sam_nvma_cycles;
	m6809_sync = do_m6809_sync;
	if (xroar_tapehack) {
		m6809_instruction_hook = tapehack_instruction_hook;
	}

	while (1) {

		/* Not tracing: */
		m6809_read_cycle = sam_read_byte;
		m6809_interrupt_hook = NULL;
		m6809_instruction_posthook = NULL;
		while (!xroar_trace_enabled) {
			m6809_run(456);
			while (EVENT_PENDING(UI_EVENT_LIST))
				DISPATCH_NEXT_EVENT(UI_EVENT_LIST);
		}

#ifdef TRACE
		/* Tracing: */
		m6809_read_cycle = trace_read_byte;
		m6809_interrupt_hook = m6809_trace_irq;
		m6809_instruction_posthook = trace_done_instruction;
		while (xroar_trace_enabled) {
			m6809_run(456);
			while (EVENT_PENDING(UI_EVENT_LIST))
				DISPATCH_NEXT_EVENT(UI_EVENT_LIST);
		}
#endif

	}
}

int xroar_filetype_by_ext(const char *filename) {
	char *ext;
	int i;
	ext = strrchr(filename, '.');
	if (ext == NULL)
		return FILETYPE_UNKNOWN;
	ext++;
	for (i = 0; filetypes[i].ext; i++) {
		if (!strncasecmp(ext, filetypes[i].ext, strlen(filetypes[i].ext)))
			return filetypes[i].filetype;
	}
	return FILETYPE_UNKNOWN;
}

int xroar_load_file(const char *filename, int autorun) {
	int filetype;
	if (filename == NULL)
		return 1;
	filetype = xroar_filetype_by_ext(filename);
	switch (filetype) {
		case FILETYPE_VDK:
		case FILETYPE_JVC:
		case FILETYPE_DMK:
			vdrive_eject_disk(0);
			if (vdrive_insert_disk(0, vdisk_load(filename)) == 0) {
				if (autorun) {
					if (IS_DRAGON) {
						keyboard_queue_string("\033BOOT\r");
					} else {
						keyboard_queue_string("\033DOS\r");
					}
				}
				return 0;
			}
			return 1;
		case FILETYPE_BIN:
			return bin_load(filename, autorun);
		case FILETYPE_HEX:
			return intel_hex_read(filename);
		case FILETYPE_SNA:
			return read_snapshot(filename);
		case FILETYPE_ROM:
			machine_insert_cart(cart_rom_new(filename, autorun));
			return 0;
		case FILETYPE_CAS:
		case FILETYPE_ASC:
			if (autorun)
				return tape_autorun(filename);
			else
				return tape_open_reading(filename);
		case FILETYPE_WAV:
		default:
			return tape_open_reading(filename);
	}
}

static void do_load_file(void) {
	xroar_load_file(load_file, autorun_loaded_file);
}

static void do_m6809_sync(void) {
	while (EVENT_PENDING(MACHINE_EVENT_LIST))
		DISPATCH_NEXT_EVENT(MACHINE_EVENT_LIST);
	irq  = PIA0.a.irq | PIA0.b.irq;
	firq = PIA1.a.irq | PIA1.b.irq;
}

#ifdef TRACE
static unsigned int trace_read_byte(unsigned int addr) {
	unsigned int value = sam_read_byte(addr);
	m6809_trace_byte(value, addr);
	return value;
}

static void trace_done_instruction(M6809State *state) {
	m6809_trace_print(state->reg_cc, state->reg_a,
			state->reg_b, state->reg_dp,
			state->reg_x, state->reg_y,
			state->reg_u, state->reg_s);
}
#endif
