/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2005  Ciaran Anscomb
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

#include "sound.h"
#include "logging.h"

#ifdef HAVE_MACOSX_AUDIO
extern SoundModule sound_macosx_module;
#endif
#ifdef HAVE_SDL_AUDIO
extern SoundModule sound_sdl_module;
#endif
#ifdef HAVE_OSS_AUDIO
extern SoundModule sound_oss_module;
#endif
#ifdef HAVE_SUN_AUDIO
extern SoundModule sound_sun_module;
#endif
#ifdef HAVE_JACK_AUDIO
extern SoundModule sound_jack_module;
#endif
#ifdef HAVE_NULL_AUDIO
extern SoundModule sound_null_module;
#endif
#ifdef HAVE_GP32
extern SoundModule sound_gp32_module;
#endif

static char *module_option;
static SoundModule *modules_head;
SoundModule *sound_module;

static void module_add(SoundModule *module) {
	SoundModule *m;
	module->next = NULL;
	if (modules_head == NULL) {
		modules_head = module;
		return;
	}
	for (m = modules_head; m->next; m = m->next);
	m->next = module;
}

static void module_delete(SoundModule *module) {
	SoundModule *m;
	if (module == NULL)
		return;
	if (modules_head == module) {
		modules_head = module->next;
		free(modules_head);
		return;
	}
	for (m = modules_head; m; m = m->next) {
		if (m->next == module) {
			m->next = module->next;
			return;
		}
	}
}

static void module_help(void) {
	SoundModule *m;
	printf("Available sound drivers:\n");
	for (m = modules_head; m; m = m->next) {
		printf("\t%-10s%s\n", m->name, m->help);
	}
}

/* Tries to init a module, deletes on failure */
static int module_init(SoundModule *module) {
	if (module == NULL)
		return 1;
	if (module->init() == 0) {
		sound_module = module;
		return 0;
	}
	LOG_WARN("Sound module %s initialisation failed\n", module->name);
	module_delete(module);
	return 1;
}

/* Returns 0 on success */
static int module_init_by_name(char *name) {
	SoundModule *m;
	for (m = modules_head; m; m = m->next) {
		if (!strcmp(name, m->name)) {
			return module_init(m);
		}
	}
	LOG_WARN("Sound module %s not found\n", name);
	return 1;
}

/* Scan args and record any of relevance to sound modules */
void sound_getargs(int argc, char **argv) {
	int i;
	modules_head = sound_module = NULL;
#ifdef HAVE_MACOSX_AUDIO
	module_add(&sound_macosx_module);
#endif
#ifdef HAVE_SDL_AUDIO
	module_add(&sound_sdl_module);
#endif
#ifdef HAVE_OSS_AUDIO
	module_add(&sound_oss_module);
#endif
#ifdef HAVE_SUN_AUDIO
	module_add(&sound_sun_module);
#endif
#ifdef HAVE_JACK_AUDIO
	module_add(&sound_jack_module);
#endif
#ifdef HAVE_NULL_AUDIO
	module_add(&sound_null_module);
#endif
#ifdef HAVE_GP32
	module_add(&sound_gp32_module);
#endif
	module_option = NULL;
	for (i = 1; i < (argc-1); i++) {
		if (!strcmp(argv[i], "-ao")) {
			if (!strcmp(argv[i+1], "help")) {
				module_help();
				exit(0);
			}
			i++;
			module_option = argv[i];
		}
	}
}

int sound_init(void) {
	if (module_option) {
		char *name = strtok(module_option, ":");
		while (name && !sound_module) {
			if (module_init_by_name(name) == 0) {
				return 0;
			}
			name = strtok(NULL, ":");
		}
	}
	while (modules_head) {
		/* Depends on module_init() deleting failed modules */
		if (module_init(modules_head) == 0)
			return 0;
	}
	return 1;
}

void sound_shutdown(void) {
	if (sound_module)
		sound_module->shutdown();
}

void sound_next(void) {
	SoundModule *m;
	sound_module->shutdown();
	m = sound_module->next;
	for (m = sound_module->next; m; m = m->next) {
		if (module_init(m) == 0) {
			sound_module->reset();
			return;
		}
	}
	for (m = modules_head; m && m != sound_module; m = m->next) {
		if (module_init(m) == 0) {
			sound_module->reset();
			return;
		}
	}
	exit(1);
}
