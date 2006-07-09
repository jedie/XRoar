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
#include "ui.h"
#include "logging.h"

extern UIModule ui_carbon_module;
extern UIModule ui_windows32_module;
extern UIModule ui_gtk_module;
extern UIModule ui_gp32_module;
extern UIModule ui_cli_module;

static UIModule *module_list[] = {
#ifdef HAVE_CARBON
	&ui_carbon_module,
#endif
#ifdef WINDOWS32
	&ui_windows32_module,
#endif
#ifdef HAVE_GTK_UI
	&ui_gtk_module,
#endif
#ifdef HAVE_GP32
	&ui_gp32_module,
#endif
#ifdef HAVE_CLI_UI
	&ui_cli_module,
#endif
	NULL
};

static int selected_module = 0;
UIModule *ui_module = NULL;

static void module_help(void) {
	int i;
	printf("Available UI drivers:\n");
	for (i = 0; module_list[i]; i++) {
		printf("\t%-10s%s\n", module_list[i]->name, module_list[i]->help);
	}
}

/* Tries to init selected module */
static int module_init(void) {
	if (module_list[selected_module] == NULL)
		return 0;
	if (module_list[selected_module]->init() == 0) {
		ui_module = module_list[selected_module];
		return 1;
	}
	LOG_WARN("UI module %s initialisation failed\n", module_list[selected_module]->name);
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
	LOG_WARN("UI module %s not found\n", name);
	selected_module = 0;
	return 1;
}

void ui_helptext(void) {
	puts("  -ui MODULE            specify user-interface module (-ui help for a list)");
}

/* Scan args and record any of relevance to ui modules */
void ui_getargs(int argc, char **argv) {
	int i;
	ui_module = NULL;
	selected_module = 0;
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-ui")) {
			i++;
			if (i >= argc) break;
			if (!strcmp(argv[i], "help")) {
				module_help();
				exit(0);
			}
			select_module_by_name(argv[i]);
		}
	}
}

/* Try and initialised currently selected module, iterate through
 * rest of list if that one fails. */
int ui_init(void) {
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

void ui_shutdown(void) {
	if (ui_module)
		ui_module->shutdown();
}
