/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2009  Ciaran Anscomb
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
#include "machine.h"
#include "module.h"
#include "sam.h"
#include "snapshot.h"
#include "tape.h"
#include "vdisk.h"
#include "vdrive.h"
#include "xroar.h"

#ifdef TRACE
# include "m6809_trace.h"
#endif

int requested_frameskip;
int frameskip;
int noratelimit = 0;
#ifdef TRACE
int xroar_trace_enabled = 0;
#else
# define xroar_trace_enabled (0)
#endif

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
	{ NULL, FILETYPE_UNKNOWN }
};

static void do_m6809_sync(void);
static unsigned int trace_read_byte(unsigned int addr);

static void xroar_helptext(void) {
	puts(
"  -ui MODULE            specify user-interface module (-ui help for a list)\n"
"  -vo MODULE            specify video module (-vo help for a list)\n"
"  -ao MODULE            specify audio module (-ao help for a list)\n"
"  -fskip FRAMES         specify frameskip (default: 0)\n"
"  -load FILE            load or attach FILE\n"
"  -run FILE             load or attach FILE and attempt autorun\n"
#ifdef TRACE
"  -trace                start with trace mode on\n"
#endif
"  -h, --help            display this help and exit\n"
"      --version         output version information and exit"
	    );
}

int xroar_init(int argc, char **argv) {
	int i;
	requested_frameskip = 0;

	/* Select a UI module then, possibly using lists specified in that
	 * module, select all other modules */
	ui_module = (UIModule *)module_select_by_arg((Module **)ui_module_list, "-ui", argc, argv);
	/* Select file requester module */
	if (ui_module->filereq_module_list != NULL)
		filereq_module_list = ui_module->filereq_module_list;
	filereq_module = (FileReqModule *)module_select_by_arg((Module **)filereq_module_list, "-filereq", argc, argv);
	/* Select video module */
	if (ui_module->video_module_list != NULL)
		video_module_list = ui_module->video_module_list;
	video_module = (VideoModule *)module_select_by_arg((Module **)video_module_list, "-vo", argc, argv);
	/* Select sound module */
	if (ui_module->sound_module_list != NULL)
		sound_module_list = ui_module->sound_module_list;
	sound_module = (SoundModule *)module_select_by_arg((Module **)sound_module_list, "-ao", argc, argv);
	/* Select keyboard module */
	if (ui_module->keyboard_module_list != NULL)
		keyboard_module_list = ui_module->keyboard_module_list;
	keyboard_module = NULL;
	/* Select joystick module */
	if (ui_module->joystick_module_list != NULL)
		joystick_module_list = ui_module->joystick_module_list;
	joystick_module = NULL;
	/* Look for other relevant command-line options */
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-fskip") && i+1<argc) {
			requested_frameskip = strtol(argv[++i], NULL, 0);
			if (requested_frameskip < 0)
				requested_frameskip = 0;
		} else if (!strcmp(argv[i], "-load")
				|| !strcmp(argv[i], "-snap")) {
			i++;
			if (i >= argc) break;
			load_file = argv[i];
			autorun_loaded_file = 0;
		} else if (!strcmp(argv[i], "-run")) {
			i++;
			if (i >= argc) break;
			load_file = argv[i];
			autorun_loaded_file = 1;
#ifdef TRACE
		} else if (!strcmp(argv[i], "-trace")) {
			xroar_trace_enabled = 1;
#endif
		} else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")
				|| !strcmp(argv[i], "-help")) {
			puts("Usage: xroar [OPTION]...\n");
			machine_helptext();
			cart_helptext();
			xroar_helptext();
			module_helptext((Module *)ui_module, (Module **)ui_module_list);
			module_helptext((Module *)filereq_module, (Module **)filereq_module_list);
			module_helptext((Module *)video_module, (Module **)video_module_list);
			module_helptext((Module *)sound_module, (Module **)sound_module_list);
			module_helptext((Module *)keyboard_module, (Module **)keyboard_module_list);
			module_helptext((Module *)joystick_module, (Module **)joystick_module_list);
			exit(0);
		} else if (!strcmp(argv[i], "--version")) {
			puts(
"XRoar " VERSION "\n"
"Copyright (C) 2009 Ciaran Anscomb\n"
"This is free software.  You may redistribute copies of it under the terms of\n"
"the GNU General Public License <http://www.gnu.org/licenses/gpl.html>.\n"
"There is NO WARRANTY, to the extent permitted by law.\n"
			    );
			exit(0);
		}
	}
	machine_getargs(argc, argv);
	cart_getargs(argc, argv);

	/* Disable DOS if a cassette or cartridge is being loaded */
	if (load_file) {
		load_file_type = xroar_filetype_by_ext(load_file);
		switch (load_file_type) {
			case FILETYPE_CAS:
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
	module_init((Module *)ui_module, argc, argv);
	filereq_module = (FileReqModule *)module_init_from_list((Module **)filereq_module_list, (Module *)filereq_module, argc, argv);
	if (filereq_module == NULL && filereq_module_list != NULL) {
		LOG_WARN("No file requester module initialised.\n");
	}
	video_module = (VideoModule *)module_init_from_list((Module **)video_module_list, (Module *)video_module, argc, argv);
	if (video_module == NULL && video_module_list != NULL) {
		LOG_ERROR("No video module initialised.\n");
		return 1;
	}
	sound_module = (SoundModule *)module_init_from_list((Module **)sound_module_list, (Module *)sound_module, argc, argv);
	if (sound_module == NULL && sound_module_list != NULL) {
		LOG_ERROR("No sound module initialised.\n");
		return 1;
	}
	keyboard_module = (KeyboardModule *)module_init_from_list((Module **)keyboard_module_list, (Module *)keyboard_module, argc, argv);
	if (keyboard_module == NULL && keyboard_module_list != NULL) {
		LOG_WARN("No keyboard module initialised.\n");
	}
	joystick_module = (JoystickModule *)module_init_from_list((Module **)joystick_module_list, (Module *)joystick_module, argc, argv);
	if (joystick_module == NULL && joystick_module_list != NULL) {
		LOG_WARN("No joystick module initialised.\n");
	}
	/* ... subsystems */
	keyboard_init();
	joystick_init();
	machine_init();
	/* Reset everything */
	machine_reset(RESET_HARD);
	if (load_file) {
		int filetype = xroar_filetype_by_ext(load_file);
		switch (filetype) {
			/* Snapshots and carts need to load immediately; the
			   rest can wait */
			case FILETYPE_SNA:
				read_snapshot(load_file);
				break;
			case FILETYPE_ROM:
				cart_insert(load_file, autorun_loaded_file);
				break;
			default:
				event_init(&load_file_event);
				load_file_event.dispatch = do_load_file;
				load_file_event.at_cycle = current_cycle + OSCILLATOR_RATE * 1.5;
				event_queue(&UI_EVENT_LIST, &load_file_event);
				break;
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

void xroar_mainloop(void) {
	M6809State cpu_state;

	m6809_write_cycle = sam_store_byte;
	m6809_nvma_cycles = sam_nvma_cycles;
	m6809_sync = do_m6809_sync;

	while (1) {

		/* Not tracing: */
		m6809_read_cycle = sam_read_byte;
		while (!xroar_trace_enabled) {
			m6809_run(456);
			while (EVENT_PENDING(UI_EVENT_LIST))
				DISPATCH_NEXT_EVENT(UI_EVENT_LIST);
		}

#ifdef TRACE
		/* Tracing: */
		m6809_trace_reset();
		m6809_read_cycle = trace_read_byte;
		while (xroar_trace_enabled) {
			m6809_run(1);
			m6809_get_state(&cpu_state);
			m6809_trace_print(cpu_state.reg_cc, cpu_state.reg_a,
					cpu_state.reg_b, cpu_state.reg_dp,
					cpu_state.reg_x, cpu_state.reg_y,
					cpu_state.reg_u, cpu_state.reg_s);
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

/* Bits set in 'mode' affect behaviour per-filetype:
 *   XROAR_AUTORUN_CAS: try to autorun any CAS file
 *   XROAR_AUTORUN_CART: enable cartridge FIRQ autostarter
 */
int xroar_load_file(const char *filename, int mode) {
	int filetype;
	if (filename == NULL)
		return 1;
	filetype = xroar_filetype_by_ext(filename);
	switch (filetype) {
		case FILETYPE_VDK:
		case FILETYPE_JVC:
		case FILETYPE_DMK:
			vdrive_eject_disk(0);
			return vdrive_insert_disk(0, vdisk_load(filename));
		case FILETYPE_BIN:
			return coco_bin_read(filename);
		case FILETYPE_HEX:
			return intel_hex_read(filename);
		case FILETYPE_SNA:
			return read_snapshot(filename);
		case FILETYPE_ROM:
			return cart_insert(filename, mode & XROAR_AUTORUN_CART);
		case FILETYPE_CAS:
			if (mode & XROAR_AUTORUN_CAS)
				return tape_autorun(filename);
			else
				return tape_open_reading(filename);
		case FILETYPE_WAV:
		default:
			return tape_open_reading(filename);
	}
}

static void do_load_file(void) {
	int flags = 0;
	if (autorun_loaded_file)
		flags = XROAR_AUTORUN_CAS;
	xroar_load_file(load_file, flags);
	if (autorun_loaded_file) {
		switch (load_file_type) {
			case FILETYPE_VDK:
			case FILETYPE_JVC:
			case FILETYPE_DMK:
				if (IS_DRAGON)
					keyboard_queue_string("\rboot\r");
				else
					keyboard_queue_string("\rdos\r");
				break;
			default:
				break;
		}
	}
}

static void do_m6809_sync(void) {
	while (EVENT_PENDING(MACHINE_EVENT_LIST))
		DISPATCH_NEXT_EVENT(MACHINE_EVENT_LIST);
}

#ifdef TRACE
static unsigned int trace_read_byte(unsigned int addr) {
	unsigned int value = sam_read_byte(addr);
	m6809_trace_byte(value, addr);
	return value;
}
#endif
