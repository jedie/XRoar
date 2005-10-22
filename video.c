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

#include "video.h"
#include "vdg.h"
#include "logging.h"

#ifdef HAVE_SDLGL
extern VideoModule video_sdlgl_module;
#endif
#ifdef HAVE_SDL
extern VideoModule video_sdlyuv_module;
extern VideoModule video_sdl_module;
#endif
#ifdef HAVE_GP32
extern VideoModule video_gp32_module;
#endif

static char *module_option;
static VideoModule *modules_head;
VideoModule *video_module;

int video_artifact_mode;
int video_want_fullscreen;

static void module_add(VideoModule *module) {
	VideoModule *m;
	module->next = NULL;
	if (modules_head == NULL) {
		modules_head = module;
		return;
	}
	for (m = modules_head; m->next; m = m->next);
	m->next = module;
}

static void module_delete(VideoModule *module) {
	VideoModule *m;
	if (modules_head == module) {
		modules_head = module->next;
		free(modules_head);
		return;
	}
	for (m = modules_head; m; m = m->next) {
		if (m->next == module) {
			m->next = module->next;
			free(module);
			return;
		}
	}
}

static void module_help(void) {
	VideoModule *m;
	printf("Available video drivers:\n");
	for (m = modules_head; m; m = m->next) {
		printf("\t%-10s%s\n", m->name, m->help);
	}
}

/* Tries to init a module, deletes on failure */
static int module_init(VideoModule *module) {
	if (module == NULL)
		return 1;
	if (module->init() == 0) {
		video_module = module;
		return 0;
	}
	LOG_WARN("Video module %s initialisation failed\n", module->name);
	module_delete(module);
	return 1;
}

/* Returns 0 on success */
static int module_init_by_name(char *name) {
	VideoModule *m;
	for (m = modules_head; m; m = m->next) {
		if (!strcmp(name, m->name)) {
			return module_init(m);
		}
	}
	LOG_WARN("Video module %s not found\n", name);
	return 1;
}

void video_helptext(void) {
	puts("  -vo MODULE            specify video module (-vo help for a list)");
	puts("  -fs                   request initial full-screen mode");
}

/* Scan args and record any of relevance to video modules */
void video_getargs(int argc, char **argv) {
	int i;
	modules_head = video_module = NULL;
#ifdef HAVE_SDLGL
	module_add(&video_sdlgl_module);
#endif
#ifdef HAVE_SDL
# ifdef PREFER_NOYUV
	module_add(&video_sdl_module);
	module_add(&video_sdlyuv_module);
# else
	module_add(&video_sdlyuv_module);
	module_add(&video_sdl_module);
# endif
#endif
#ifdef HAVE_GP32
	module_add(&video_gp32_module);
#endif
	module_option = NULL;
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-vo")) {
			i++;
			if (i >= argc) break;
			if (!strcmp(argv[i], "help")) {
				module_help();
				exit(0);
			}
			module_option = argv[i];
		} else if (strcmp(argv[i], "-fs") == 0) {
			video_want_fullscreen = 1;
		}
	}
}

int video_init(void) {
	if (module_option) { 
		char *name = strtok(module_option, ":");
		while (name && !video_module) {
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

void video_shutdown(void) {
	if (video_module)
		video_module->shutdown();
}

void video_next(void) {
	VideoModule *v;
	video_module->shutdown();
	v = video_module->next;
	for (v = video_module->next; v; v = v->next) {
		if (module_init(v) == 0) {
			video_module->vdg_reset();
			vdg_reset();
			return;
		}
	}
	for (v = modules_head; v && v != video_module; v = v->next) {
		if (module_init(v) == 0) {
			video_module->vdg_reset();
			vdg_reset();
			return;
		}
	}
	exit(1);
}
