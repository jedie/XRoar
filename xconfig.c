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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logging.h"
#include "xconfig.h"

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

static int lookup_enum(const char *name, struct xconfig_enum *list) {
	int i;
	for (i = 0; list[i].name; i++) {
		if (0 == strcmp(name, list[i].name)) {
			return list[i].value;
		}
	}
	/* Only check this afterwards, as "help" could be a valid name */
	if (0 == strcmp(name, "help")) {
		for (i = 0; list[i].name; i++) {
			printf("\t%-10s %s\n", list[i].name, list[i].description);
		}
		exit(0);
	}
	return -1;
}

static void set_option(struct xconfig_option *option, const char *arg) {
	switch (option->type) {
		case XCONFIG_BOOL:
			*(int *)option->dest = 1;
			break;
		case XCONFIG_BOOL0:
			*(int *)option->dest = 0;
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
		case XCONFIG_STRING0:
			if (*(char **)option->dest) {
				free(*(char **)option->dest);
				*(char **)option->dest = NULL;
			}
			break;
		case XCONFIG_CALL_0:
			((void (*)(void))option->dest)();
			break;
		case XCONFIG_CALL_1:
			((void (*)(const char *))option->dest)(arg);
			break;
		case XCONFIG_ENUM:
			*(int *)option->dest = lookup_enum(arg, (struct xconfig_enum *)option->ref);
			break;
		default:
			break;
	}
}

/* returns 0 if it's a value option to unset */
static int unset_option(struct xconfig_option *option) {
	switch (option->type) {
	case XCONFIG_BOOL:
		*(int *)option->dest = 0;
		return 0;
	case XCONFIG_BOOL0:
		*(int *)option->dest = 1;
		return 0;
	case XCONFIG_STRING:
		if (*(char **)option->dest) {
			free(*(char **)option->dest);
			*(char **)option->dest = NULL;
		}
		return 0;
	default:
		break;
	}
	return -1;
}

/* Simple parser: one directive per line, "option argument" */
enum xconfig_result xconfig_parse_file(struct xconfig_option *options,
		const char *filename) {
	struct xconfig_option *option;
	char *line, *opt, *arg;
	FILE *cfg;
	int line_number;
	char buf[256];
	int ret = XCONFIG_OK;

	cfg = fopen(filename, "r");
	if (cfg == NULL) return XCONFIG_FILE_ERROR;
	line_number = 0;
	while ((line = fgets(buf, sizeof(buf), cfg))) {
		line_number++;
		while (isspace(*line))
			line++;
		if (*line == 0 || *line == '#')
			continue;
		opt = strtok(line, "\t\n\v\f\r =");
		if (opt == NULL) continue;
		while (*opt == '-') opt++;
		option = find_option(options, opt);
		if (option == NULL) {
			if (0 == strncmp(opt, "no-", 3)) {
				option = find_option(options, opt + 3);
				if (option && unset_option(option) == 0) {
					continue;
				}
			}
			ret = XCONFIG_BAD_OPTION;
			LOG_ERROR("Unrecognised option `%s'\n", opt);
			continue;
		}
		if (option->type == XCONFIG_STRING) {
			/* preserve spaces */
			arg = strtok(NULL, "\n\v\f\r");
			if (arg) {
				while (isspace(*arg) || *arg == '=') {
					arg++;
				}
			}
		} else {
			arg = strtok(NULL, "\t\n\v\f\r =");
		}
		if (option->type == XCONFIG_BOOL ||
		    option->type == XCONFIG_BOOL0 ||
		    option->type == XCONFIG_CALL_0) {
			set_option(option, NULL);
			continue;
		}
		if (arg == NULL) {
			ret = XCONFIG_MISSING_ARG;
			LOG_ERROR("Missing argument to `%s'\n", opt);
			continue;
		}
		set_option(option, arg);
	}
	fclose(cfg);
	return ret;
}

enum xconfig_result xconfig_parse_cli(struct xconfig_option *options,
		int argc, char **argv, int *argn) {
	struct xconfig_option *option;
	int _argn;
	char *opt;

	_argn = argn ? *argn : 1;
	while (_argn < argc) {
		if (argv[_argn][0] != '-') {
			break;
		}
		if (0 == strcmp("--", argv[_argn])) {
			_argn++;
			break;
		}
		opt = argv[_argn]+1;
		if (*opt == '-') opt++;
		option = find_option(options, opt);
		if (option == NULL) {
			if (0 == strncmp(opt, "no-", 3)) {
				option = find_option(options, opt + 3);
				if (option && unset_option(option) == 0) {
					_argn++;
					continue;
				}
			}
			if (argn) *argn = _argn;
			LOG_ERROR("Unrecognised option `%s'\n", opt);
			return XCONFIG_BAD_OPTION;
		}
		if (option->type == XCONFIG_BOOL ||
		    option->type == XCONFIG_BOOL0 ||
		    option->type == XCONFIG_CALL_0) {
			set_option(option, NULL);
			_argn++;
			continue;
		}
		if ((_argn + 1) >= argc) {
			if (argn) *argn = _argn;
			LOG_ERROR("Missing argument to `%s'\n", opt);
			return XCONFIG_MISSING_ARG;
		}
		set_option(option, argv[_argn+1]);
		_argn += 2;
	}
	if (argn) *argn = _argn;
	return XCONFIG_OK;
}
