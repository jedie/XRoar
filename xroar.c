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
#include "logging.h"
#include "cart.h"
#include "events.h"
#include "fs.h"
#include "m6809.h"
#include "machine.h"
#include "module.h"
#include "pia.h"
#include "sam.h"
#include "snapshot.h"
#include "wd279x.h"
#include "xroar.h"

int requested_frameskip;
int frameskip;
int noratelimit = 0;
int video_artifact_mode = 0;

#ifdef TRACE
int trace = 0;
#endif

static const char *snapshot_load = NULL;

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
	{ NULL, FILETYPE_UNKNOWN }
};

static void xroar_helptext(void) {
	puts("  -ui MODULE            specify user-interface module (-ui help for a list)");
	puts("  -vo MODULE            specify video module (-vo help for a list)");
	puts("  -ao MODULE            specify audio module (-ao help for a list)");
	puts("  -fskip FRAMES         specify frameskip (default: 0)");
	puts("  -snap FILENAME        load snapshot after initialising");
#ifdef TRACE
	puts("  -trace                start with trace mode on");
#endif
	puts("  -h, --help            display this help and exit");
	puts("      --version         output version information and exit");
}

int xroar_init(int argc, char **argv) {
	int i;
#ifdef TRACE
	trace = 0;
#endif
	requested_frameskip = 0;

	/* Select a UI module then, possibly using lists specified in that
	 * module, select all other modules */
	ui_module = (UIModule *)module_select_by_arg((Module **)ui_module_list, "-ui", argc, argv);
	/* Select file requester module */
	if (ui_module->filereq_module_list != NULL)
		filereq_module_list = ui_module->filereq_module_list;
	filereq_module = NULL;
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
		} else if (!strcmp(argv[i], "-snap")) {
			i++;
			if (i >= argc) break;
			snapshot_load = argv[i];
#ifdef TRACE
		} else if (!strcmp(argv[i], "-trace")) {
			trace = 1;
#endif
		} else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")
				|| !strcmp(argv[i], "-help")) {
			printf("Usage: xroar [OPTION]...\n\n");
			module_helptext((Module *)ui_module);
			module_helptext((Module *)filereq_module);
			module_helptext((Module *)video_module);
			module_helptext((Module *)sound_module);
			module_helptext((Module *)keyboard_module);
			module_helptext((Module *)joystick_module);
			machine_helptext();
			cart_helptext();
			xroar_helptext();
			exit(0);
		} else if (!strcmp(argv[i], "--version")) {
			printf("XRoar " VERSION "\n");
			puts("Copyright (C) 2006 Ciaran Anscomb");
			puts("This is free software.  You may redistribute copies of it under the terms of");
			puts("the GNU General Public License <http://www.gnu.org/licenses/gpl.html>.");
			puts("There is NO WARRANTY, to the extent permitted by law.");
			exit(0);
		}
	}
	machine_getargs(argc, argv);
	cart_getargs(argc, argv);

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
	keyboard_reset();
	joystick_reset();
	machine_reset(RESET_HARD);
	if (snapshot_load)
		read_snapshot(snapshot_load);
	return 0;
}

void xroar_shutdown(void) {
	tape_shutdown();
	module_shutdown((Module *)joystick_module);
	module_shutdown((Module *)keyboard_module);
	module_shutdown((Module *)sound_module);
	module_shutdown((Module *)video_module);
	module_shutdown((Module *)filereq_module);
	module_shutdown((Module *)ui_module);
}

void xroar_mainloop(void) {
	while (1) {
		while (EVENT_PENDING)
			DISPATCH_NEXT_EVENT;
		m6809_cycle(event_list->at_cycle);
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
