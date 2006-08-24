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

#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "logging.h"
#include "module.h"

/**** Default user interface module list ****/

extern UIModule ui_sdl_module;
extern UIModule ui_gp32_module;
static UIModule *default_ui_module_list[] = {
#ifdef HAVE_SDL
	&ui_sdl_module,
#endif
#ifdef HAVE_GP32
	&ui_gp32_module,
#endif
	NULL
};

/**** Default file requester module list ****/

extern FileReqModule filereq_carbon_module;
extern FileReqModule filereq_windows32_module;
extern FileReqModule filereq_gtk_module;
extern FileReqModule filereq_cli_module;
static FileReqModule *default_filereq_module_list[] = {
#ifdef HAVE_CARBON
	&filereq_carbon_module,
#endif
#ifdef WINDOWS32
	&filereq_windows32_module,
#endif
#ifdef HAVE_GTK
	&filereq_gtk_module,
#endif
#ifdef HAVE_CLI
	&filereq_cli_module,
#endif
	NULL
};

/**** Default sound module list ****/

extern SoundModule sound_macosx_module;
extern SoundModule sound_sun_module;
extern SoundModule sound_sdl_module;
extern SoundModule sound_oss_module;
extern SoundModule sound_jack_module;
extern SoundModule sound_rtc_module;
extern SoundModule sound_gp32_module;
extern SoundModule sound_null_module;
static SoundModule *default_sound_module_list[] = {
#ifdef HAVE_MACOSX_AUDIO
	&sound_macosx_module,
#endif
#ifdef HAVE_SUN_AUDIO
	&sound_sun_module,
#endif
#ifdef HAVE_SDL
	&sound_sdl_module,
#endif
#ifdef HAVE_OSS_AUDIO
	&sound_oss_module,
#endif
#ifdef HAVE_JACK_AUDIO
	&sound_jack_module,
#endif
#ifdef HAVE_RTC
	&sound_rtc_module,
#endif
#ifdef HAVE_GP32
	&sound_gp32_module,
#endif
	&sound_null_module,
	NULL
};

/**** Default joystick module list ****/

extern JoystickModule joystick_sdl_module;
static JoystickModule *default_joystick_module_list[] = {
#ifdef HAVE_SDL
	&joystick_sdl_module,
#endif
	NULL
};

UIModule **ui_module_list = default_ui_module_list;
UIModule *ui_module = NULL;
FileReqModule **filereq_module_list = default_filereq_module_list;
FileReqModule *filereq_module = NULL;
VideoModule **video_module_list = NULL;
VideoModule *video_module = NULL;
SoundModule **sound_module_list = default_sound_module_list;
SoundModule *sound_module = NULL;
KeyboardModule **keyboard_module_list = NULL;
KeyboardModule *keyboard_module = NULL;
JoystickModule **joystick_module_list = default_joystick_module_list;
JoystickModule *joystick_module = NULL;

void module_print_list(Module **list) {
	int i;
	if (list == NULL || list[0]->common.name == NULL) {
		puts("\tNone found.");
		return;
	}
	for (i = 0; list[i]; i++) {
		printf("\t%-10s%s\n", list[i]->common.name, list[i]->common.description);
	}
}

Module *module_select(Module **list, const char *name) {
	int i;
	if (list == NULL)
		return NULL;
	for (i = 0; list[i]; i++) {
		if (!strcmp(list[i]->common.name, name))
			return list[i];
	}
	return NULL;
}

Module *module_select_by_arg(Module **list, const char *arg, int argc, char **argv) {
	int i;
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], arg) == 0 && i+1<argc) {
			i++;
			if (strcmp(argv[i], "help") == 0) {
				module_print_list(list);
				exit(0);
			}
			return module_select(list, argv[i]);
		}
	}
	return list[0];
}

Module *module_init(Module *module, int argc, char **argv) {
	if (module != NULL && module->common.init(argc, argv) == 0) {
		module->common.initialised = 1;
		return module;
	}
	return NULL;
}

Module *module_init_from_list(Module **list, Module *module, int argc, char **argv) {
	int i;
	/* First attempt to initialise selected module (if given) */
	if (module_init(module, argc, argv))
		return module;
	if (list == NULL)
		return NULL;
	/* If that fails, try every *other* module in the list */
	for (i = 0; list[i]; i++) {
		if (list[i] != module && (module_init(list[i], argc, argv)))
			return list[i];
	}
	return NULL;
}

void module_shutdown(Module *module) {
	if (module && module->common.initialised)
		module->common.shutdown();
}

void module_helptext(Module *module) {
	if (module && module->common.helptext) {
		module->common.helptext();
	}
}
