/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2011  Ciaran Anscomb
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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xconfig.h"

/* For error reporting: */
char *xconfig_option;
int xconfig_line_number;

static char buf[256];

static struct xconfig_option *find_option(struct xconfig_option *options,
		const char *opt) {
	int i;
	for (i = 0; options[i].type != XCONFIG_END; i++) {
		if (0 == strcmp(options[i].name, opt)) {
			return &options[i];
		}
	}
	return NULL;
}

static void set_option(struct xconfig_option *option, const char *arg) {
	switch (option->type) {
		case XCONFIG_BOOL:
			*(int *)option->dest = 1;
			break;
		case XCONFIG_INT:
			*(int *)option->dest = strtol(arg, NULL, 0);
			break;
		case XCONFIG_DOUBLE:
			*(double *)option->dest = strtod(arg, NULL);
			break;
		case XCONFIG_STRING:
			*(char **)option->dest = strdup(arg);
			break;
		case XCONFIG_CALL_0:
			((void (*)(void))option->dest)();
			break;
		case XCONFIG_CALL_1:
			((void (*)(const char *))option->dest)(arg);
			break;
		default:
			break;
	}
}

/* Simple parser: one directive per line, "option argument" */
enum xconfig_result xconfig_parse_file(struct xconfig_option *options,
		const char *filename) {
	struct xconfig_option *option;
	char *line, *arg;
	FILE *cfg;
	cfg = fopen(filename, "r");
	if (cfg == NULL) return XCONFIG_FILE_ERROR;
	xconfig_line_number = 0;
	while ((line = fgets(buf, sizeof(buf), cfg))) {
		xconfig_line_number++;
		while (isspace(*line))
			line++;
		if (*line == 0 || *line == '#')
			continue;
		xconfig_option = strtok(line, "\t\n\v\f\r =");
		if (xconfig_option == NULL) continue;
		arg = strtok(NULL, "\t\n\v\f\r =");
		while (*xconfig_option == '-') xconfig_option++;
		option = find_option(options, xconfig_option);
		if (option == NULL) {
			fclose(cfg);
			return XCONFIG_BAD_OPTION;
		}
		if (option->type == XCONFIG_BOOL
		    || option->type == XCONFIG_CALL_0) {
			set_option(option, NULL);
			continue;
		}
		if (arg == NULL) {
			fclose(cfg);
			return XCONFIG_MISSING_ARG;
		}
		set_option(option, arg);
	}
	fclose(cfg);
	return XCONFIG_OK;
}

enum xconfig_result xconfig_parse_cli(struct xconfig_option *options,
		int argc, char **argv, int *argn) {
	struct xconfig_option *option;
	int _argn;
	_argn = argn ? *argn : 1;

	while (_argn < argc) {
		if (argv[_argn][0] != '-') {
			break;
		}
		if (0 == strcmp("--", argv[_argn])) {
			_argn++;
			break;
		}
		xconfig_option = argv[_argn]+1;
		if (*xconfig_option == '-') xconfig_option++;
		option = find_option(options, xconfig_option);
		if (option == NULL) {
			if (argn) *argn = _argn;
			return XCONFIG_BAD_OPTION;
		}
		if (option->type == XCONFIG_BOOL
				|| option->type == XCONFIG_CALL_0) {
			set_option(option, NULL);
			_argn++;
			continue;
		}
		if ((_argn + 1) >= argc) {
			if (argn) *argn = _argn;
			return XCONFIG_MISSING_ARG;
		}
		set_option(option, argv[_argn+1]);
		_argn += 2;
	}
	if (argn) *argn = _argn;
	return XCONFIG_OK;
}
