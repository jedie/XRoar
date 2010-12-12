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
#include "vdg.h"
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
int xroar_opt_ao_rate = 0;
int xroar_opt_volume = 100;
#ifndef FAST_SOUND
int xroar_fast_sound = 0;
#endif
int xroar_opt_fullscreen = 0;
static const char *xroar_opt_ccr = NULL;
int xroar_frameskip = 0;
char *xroar_opt_keymap = NULL;
int xroar_kbd_translate = 0;
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
int xroar_opt_tv = ANY_AUTO;
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
	{ XCONFIG_INT,      "ao-rate",      &xroar_opt_ao_rate },
	{ XCONFIG_INT,      "volume",       &xroar_opt_volume },
#ifndef FAST_SOUND
	{ XCONFIG_BOOL,     "fast-sound",   &xroar_fast_sound },
#endif
	{ XCONFIG_BOOL,     "fs",           &xroar_opt_fullscreen },
	{ XCONFIG_STRING,   "ccr",          &xroar_opt_ccr },
	{ XCONFIG_INT,      "fskip",        &xroar_frameskip },
	{ XCONFIG_STRING,   "keymap",       &xroar_opt_keymap },
	{ XCONFIG_BOOL,     "kbd-translate", &xroar_kbd_translate },
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

const char *xroar_disk_exts[] = { "DMK", "JVC", "VDK", "DSK", NULL };
const char *xroar_tape_exts[] = { "CAS", NULL };
const char *xroar_snap_exts[] = { "SNA", NULL };
const char *xroar_cart_exts[] = { "ROM", NULL };

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

void (*xroar_machine_changed_cb)(int machine_type) = NULL;
void (*xroar_dos_changed_cb)(int dos_type) = NULL;
void (*xroar_fullscreen_changed_cb)(int fullscreen) = NULL;
void (*xroar_keymap_changed_cb)(int keymap) = NULL;
void (*xroar_kbd_translate_changed_cb)(int kbd_translate) = NULL;
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
"  -ao-rate HZ           set audio sample rate (if allowed by module)\n"
"  -volume VOLUME        audio volume (0 - 100)\n"
#ifndef FAST_SOUND
"  -fast-sound           faster but less accurate sound\n"
#endif
"  -fs                   start emulator full-screen if possible\n"
"  -ccr RENDERER         cross-colour renderer (-ccr help for list)\n"
"  -fskip FRAMES         frameskip (default: 0)\n"
"  -keymap CODE          host keyboard type (uk, us, fr, de)\n"
"  -kbd-translate        enable keyboard translation\n"
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
		ret = xconfig_parse_file(xroar_options, conffile);
		if (ret == XCONFIG_MISSING_ARG) {
			LOG_DEBUG(0, "%s:%d: missing argument to `%s'\n", conffile, xconfig_line_number, xconfig_option);
			exit(1);
		}
		if (ret == XCONFIG_BAD_OPTION) {
			LOG_DEBUG(0, "%s:%d: unrecognised option `%s'\n", conffile, xconfig_line_number, xconfig_option);
			exit(1);
		}
	}
	/* Parse command line options */
	ret = xconfig_parse_cli(xroar_options, argc, argv, &argn);
	if (ret == XCONFIG_MISSING_ARG) {
		LOG_DEBUG(0, "%s: missing argument to `%s'\n", argv[0], xconfig_option);
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
			LOG_DEBUG(0, "%s: unrecognised option `%s'\n", argv[0], xconfig_option);
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
	if (xroar_opt_volume < 0) {
		xroar_opt_volume = 0;
	}
	if (xroar_opt_volume > 100) {
		xroar_opt_volume = 100;
	}
	xroar_opt_volume = (256 * xroar_opt_volume) / 100;

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

	/* Notify UI of starting options: */
	xroar_set_kbd_translate(xroar_kbd_translate);

	/* Load carts before initial reset */
	if (load_file && load_file_type == FILETYPE_ROM) {
		xroar_load_file_by_type(load_file, autorun_loaded_file);
		load_file = NULL;
	}
	/* Reset everything */
	machine_reset(RESET_HARD);
	if (load_file) {
		if (load_file_type == FILETYPE_SNA) {
			/* Load snapshots immediately */
			xroar_load_file_by_type(load_file, autorun_loaded_file);
		} else if (!autorun_loaded_file && load_file_type != FILETYPE_BIN
				&& load_file_type != FILETYPE_HEX) {
			/* Everything else except CoCo binaries and hex
			 * records can be attached now if not autorunning */
			xroar_load_file_by_type(load_file, 0);
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
	m6809_read_cycle = sam_read_byte;
	m6809_write_cycle = sam_store_byte;
	m6809_nvma_cycles = sam_nvma_cycles;
	m6809_sync = do_m6809_sync;
	m6809_interrupt_hook = NULL;
	m6809_instruction_posthook = NULL;
	if (xroar_tapehack) {
		m6809_instruction_hook = tapehack_instruction_hook;
	}

	xroar_set_trace(xroar_trace_enabled);

	/* If UI module has its own idea of a main loop, delegate to that */
	if (ui_module->run) {
		ui_module->run();
		return;
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

int xroar_load_file_by_type(const char *filename, int autorun) {
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
	xroar_load_file_by_type(load_file, autorun_loaded_file);
}

static void do_m6809_sync(void) {
	while (EVENT_PENDING(MACHINE_EVENT_LIST))
		DISPATCH_NEXT_EVENT(MACHINE_EVENT_LIST);
	if (PIA0.a.irq | PIA0.b.irq) {
		m6809_irq_set();
	} else {
		m6809_irq_clear();
	}
	if (PIA1.a.irq | PIA1.b.irq) {
		m6809_firq_set();
	} else {
		m6809_firq_clear();
	}
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

/* Helper functions */

void xroar_set_trace(int mode) {
	int set_to;
	switch (mode) {
		case XROAR_OFF: default:
			set_to = 0;
			break;
		case XROAR_ON:
			set_to = 1;
			break;
		case XROAR_TOGGLE: case XROAR_CYCLE:
			set_to = !xroar_trace_enabled;
			break;
	}
	xroar_trace_enabled = set_to;
	if (xroar_trace_enabled) {
		m6809_read_cycle = trace_read_byte;
		m6809_interrupt_hook = m6809_trace_irq;
		m6809_instruction_posthook = trace_done_instruction;
	} else {
		m6809_read_cycle = sam_read_byte;
		m6809_interrupt_hook = NULL;
		m6809_instruction_posthook = NULL;
	}
}

void xroar_new_disk(int drive) {
	char *filename = filereq_module->save_filename(xroar_disk_exts);

	if (filename == NULL)
		return;
	int filetype = xroar_filetype_by_ext(filename);
	vdrive_eject_disk(drive);
	/* Default to 40T 1H.  XXX: need interface to select */
	struct vdisk *new_disk = vdisk_blank_disk(1, 40, VDISK_LENGTH_5_25);
	if (new_disk == NULL)
		return;
	LOG_DEBUG(4, "Creating blank disk in drive %d\n", 1 + drive);
	switch (filetype) {
		case FILETYPE_VDK:
		case FILETYPE_JVC:
		case FILETYPE_DMK:
			break;
		default:
			filetype = FILETYPE_DMK;
			break;
	}
	new_disk->filetype = filetype;
	new_disk->filename = strdup(filename);
	new_disk->file_write_protect = VDISK_WRITE_ENABLE;
	vdrive_insert_disk(drive, new_disk);
}

void xroar_insert_disk(int drive) {
	char *filename = filereq_module->load_filename(xroar_disk_exts);
	if (filename) {
		vdrive_eject_disk(drive);
		vdrive_insert_disk(drive, vdisk_load(filename));
	}
}

void xroar_toggle_write_back(int drive) {
	struct vdisk *disk = vdrive_disk_in_drive(drive);
	if (disk != NULL) {
		if (disk->file_write_protect == VDISK_WRITE_ENABLE) {
			disk->file_write_protect = VDISK_WRITE_PROTECT;
			LOG_DEBUG(2, "Write back disallowed for disk in drive %d.\n", drive);
		} else {
			disk->file_write_protect = VDISK_WRITE_ENABLE;
			LOG_DEBUG(2, "Write back allowed for disk in drive %d.\n", drive);
		}
	}
}

void xroar_toggle_write_protect(int drive) {
	struct vdisk *disk = vdrive_disk_in_drive(drive);
	if (disk != NULL) {
		if (disk->write_protect == VDISK_WRITE_ENABLE) {
			disk->write_protect = VDISK_WRITE_PROTECT;
			LOG_DEBUG(2, "Disk in drive %d write protected.\n", drive);
		} else {
			disk->write_protect = VDISK_WRITE_ENABLE;
			LOG_DEBUG(2, "Disk in drive %d write enabled.\n", drive);
		}
	}
}

void xroar_cycle_cross_colour(void) {
	running_config.cross_colour_phase++;
	if (running_config.cross_colour_phase > 2)
		running_config.cross_colour_phase = 0;
	vdg_set_mode();
}

void xroar_quit(void) {
	exit(0);
}

void xroar_fullscreen(int action) {
	static int lock = 0;
	if (lock) return;
	lock = 1;
	int set_to;
	switch (action) {
		case XROAR_OFF:
			set_to = 0;
			break;
		case XROAR_ON:
			set_to = 1;
			break;
		case XROAR_TOGGLE:
		default:
			set_to = !video_module->is_fullscreen;
			break;
	}
	video_module->set_fullscreen(set_to);
	if (xroar_fullscreen_changed_cb) {
		xroar_fullscreen_changed_cb(set_to);
	}
	lock = 0;
}

void xroar_load_file(const char **exts) {
	char *filename = filereq_module->load_filename(exts);
	if (filename) {
		xroar_load_file_by_type(filename, 0);
	}
}

void xroar_run_file(const char **exts) {
	char *filename = filereq_module->load_filename(exts);
	if (filename) {
		xroar_load_file_by_type(filename, 1);
	}
}

void xroar_set_keymap(int keymap) {
	static int lock = 0;
	if (lock) return;
	lock = 1;
	int new;
	switch (keymap) {
		case XROAR_CYCLE:
			new = (running_config.keymap + 1) % NUM_KEYMAPS;
			break;
		default:
			new = keymap;
			break;
	}
	if (new >= 0 && new < NUM_KEYMAPS) {
		keyboard_set_keymap(new);
		if (xroar_keymap_changed_cb) {
			xroar_keymap_changed_cb(new);
		}
	}
	lock = 0;
}

void xroar_set_kbd_translate(int kbd_translate) {
	static int lock = 0;
	if (lock) return;
	lock = 1;
	switch (kbd_translate) {
		case XROAR_TOGGLE: case XROAR_CYCLE:
			xroar_kbd_translate = !xroar_kbd_translate;
			break;
		default:
			xroar_kbd_translate = kbd_translate;
			break;
	}
	if (xroar_kbd_translate_changed_cb) {
		xroar_kbd_translate_changed_cb(xroar_kbd_translate);
	}
	lock = 0;
}

void xroar_set_machine(int machine_type) {
	static int lock = 0;
	if (lock) return;
	lock = 1;
	int new = requested_machine;
	switch (machine_type) {
		case XROAR_TOGGLE:
			break;
		case XROAR_CYCLE:
			new = (new + 1) % NUM_MACHINE_TYPES;
			break;
		default:
			new = machine_type;
			break;
	}
	if (new >= 0 && new < NUM_MACHINE_TYPES) {
		machine_clear_requested_config();
		requested_machine = new;
		machine_reset(RESET_HARD);
		if (xroar_machine_changed_cb) {
			xroar_machine_changed_cb(new);
		}
	}
	lock = 0;
}

void xroar_set_dos(int dos_type) {
	static int lock = 0;
	if (lock) return;
	lock = 1;
	int new = requested_config.dos_type;
	switch (dos_type) {
		case XROAR_TOGGLE:
			new = (new != DOS_NONE) ? DOS_NONE : ANY_AUTO;
			break;
		case XROAR_CYCLE:
			new = (new + 1) % NUM_DOS_TYPES;
			break;
		default:
			new = dos_type;
			break;
	}
	if (new == ANY_AUTO || (new >= 0 && new < NUM_DOS_TYPES)) {
		requested_config.dos_type = new;
		if (xroar_dos_changed_cb) {
			xroar_dos_changed_cb(new);
		}
	}
	lock = 0;
}

void xroar_save_snapshot(void) {
	char *filename = filereq_module->save_filename(xroar_snap_exts);
	if (filename) {
		write_snapshot(filename);
	}
}

void xroar_write_tape(void) {
	char *filename = filereq_module->save_filename(xroar_tape_exts);
	if (filename) {
		tape_open_writing(filename);
	}
}

void xroar_soft_reset(void) {
	machine_reset(RESET_SOFT);
}

void xroar_hard_reset(void) {
	machine_reset(RESET_HARD);
}
