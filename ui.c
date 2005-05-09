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

#include "ui.h"
#include "logging.h"

#ifdef HAVE_CARBON_UI
extern UIModule ui_carbon_module;
#endif
#ifdef WINDOWS32
extern UIModule ui_windows32_module;
#endif
#ifdef HAVE_GTK_UI
extern UIModule ui_gtk_module;
#endif
#ifdef HAVE_GP32
extern UIModule ui_gp32_module;
#endif
#ifdef HAVE_CLI_UI
extern UIModule ui_cli_module;
#endif

static char *module_option;
static UIModule *modules_head;
UIModule *ui_module;

static void module_add(UIModule *module) {
	UIModule *m;
	module->next = NULL;
	if (modules_head == NULL) {
		modules_head = module;
		return;
	}
	for (m = modules_head; m->next; m = m->next);
	m->next = module;
}

static void module_delete(UIModule *module) {
	UIModule *m;
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
	UIModule *m;
	printf("Available UI drivers:\n");
	for (m = modules_head; m; m = m->next) {
		printf("\t%-10s%s\n", m->name, m->help);
	}
}

/* Tries to init a module, deletes on failure */
static int module_init(UIModule *module) {
	if (module == NULL)
		return 1;
	if (module->init() == 0) {
		ui_module = module;
		return 0;
	}
	LOG_WARN("UI module %s initialisation failed\n", module->name);
	module_delete(module);
	return 1;
}

/* Returns 0 on success */
static int module_init_by_name(char *name) {
	UIModule *m;
	for (m = modules_head; m; m = m->next) {
		if (!strcmp(name, m->name)) {
			return module_init(m);
		}
	}
	LOG_WARN("UI module %s not found\n", name);
	return 1;
}

/* Scan args and record any of relevance to ui modules */
void ui_getargs(int argc, char **argv) {
	int i;
	modules_head = ui_module = NULL;
#ifdef HAVE_CARBON_UI
	module_add(&ui_carbon_module);
#endif
#ifdef WINDOWS32
	module_add(&ui_windows32_module);
#endif
#ifdef HAVE_GTK_UI
	module_add(&ui_gtk_module);
#endif
#ifdef HAVE_GP32
	module_add(&ui_gp32_module);
#endif
#ifdef HAVE_CLI_UI
	module_add(&ui_cli_module);
#endif
	module_option = NULL;
	for (i = 1; i < (argc-1); i++) {
		if (!strcmp(argv[i], "-ui")) {
			if (!strcmp(argv[i+1], "help")) {
				module_help();
				exit(0);
			}
			i++;
			module_option = argv[i];
		}
	}
}

int ui_init(void) {
	if (module_option) { 
		char *name = strtok(module_option, ":");
		while (name && !ui_module) {
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

void ui_shutdown(void) {
	if (ui_module)
		ui_module->shutdown();
}
