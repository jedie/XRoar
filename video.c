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
#include "video.h"
#include "vdg.h"
#include "logging.h"

extern VideoModule video_sdlgl_module;
extern VideoModule video_sdlyuv_module;
extern VideoModule video_sdl_module;
extern VideoModule video_gp32_module;

static VideoModule *module_list[] = {
#ifdef HAVE_SDLGL
	&video_sdlgl_module,
#endif
#ifdef HAVE_SDL
# ifdef PREFER_NOYUV
	&video_sdl_module,
	&video_sdlyuv_module,
# else
	&video_sdlyuv_module,
	&video_sdl_module,
# endif
#endif
#ifdef HAVE_GP32
	&video_gp32_module,
#endif
	NULL
};

static int selected_module = 0;
VideoModule *video_module = NULL;

int video_artifact_mode;
int video_want_fullscreen;

static void module_help(void) {
	int i;
	printf("Available video drivers:\n");
	for (i = 0; module_list[i]; i++) {
		printf("\t%-10s%s\n", module_list[i]->name, module_list[i]->help);
	}
}

/* Tries to init selected module */
static int module_init(void) {
	if (module_list[selected_module] == NULL)
		return 0;
	if (module_list[selected_module]->init() == 0) {
		video_module = module_list[selected_module];
		return 1;
	}
	LOG_WARN("Video module %s initialisation failed\n", module_list[selected_module]->name);
	return 0;
}

/* Returns 0 on success */
static int select_module_by_name(char *name) {
	int i;
	for (i = 0; module_list[i]; i++) {
		if (!strcmp(name, module_list[i]->name)) {
			selected_module = i;
			return 0;
		}
	}
	LOG_WARN("Video module %s not found\n", name);
	selected_module = 0;
	return 1;
}

void video_helptext(void) {
	puts("  -vo MODULE            specify video module (-vo help for a list)");
	puts("  -fs                   request initial full-screen mode");
}

/* Scan args and record any of relevance to video modules */
void video_getargs(int argc, char **argv) {
	int i;
	video_module = NULL;
	selected_module = 0;
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-vo")) {
			i++;
			if (i >= argc) break;
			if (!strcmp(argv[i], "help")) {
				module_help();
				exit(0);
			}
			select_module_by_name(argv[i]);
		} else if (strcmp(argv[i], "-fs") == 0) {
			video_want_fullscreen = 1;
		}
	}
}

/* Try and initialised currently selected module, iterate through
 * rest of list if that one fails. */
int video_init(void) {
	int old_module = selected_module;
	if (module_init())
		return 0;
	do {
		selected_module++;
		if (module_list[selected_module] == NULL)
			selected_module = 0;
		if (module_init())
			return 0;
	} while (selected_module != old_module);
	return 1;
}

void video_shutdown(void) {
	if (video_module)
		video_module->shutdown();
}

void video_next(void) {
	video_shutdown();
	selected_module++;
	if (video_init())
		exit(1);
	video_module->vdg_reset();
	vdg_reset();
}
