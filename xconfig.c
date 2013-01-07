/*  Copyright 2003-2013 Ciaran Anscomb
 *
 *  This file is part of XRoar.
 *
 *  XRoar is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  XRoar is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XRoar.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "portalib/glib.h"

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
		exit(EXIT_SUCCESS);
	}
	return -1;
}

static void set_option(struct xconfig_option *option, char *arg) {
	switch (option->type) {
		case XCONFIG_BOOL:
			if (option->call)
				option->dest.func_bool(1);
			else
				*(_Bool *)option->dest.object = 1;
			break;
		case XCONFIG_BOOL0:
			if (option->call)
				option->dest.func_bool(0);
			else
				*(_Bool *)option->dest.object = 0;
			break;
		case XCONFIG_INT:
			{
				int val = strtol(arg, NULL, 0);
				if (option->call)
					option->dest.func_int(val);
				else
					*(int *)option->dest.object = val;
			}
			break;
		case XCONFIG_INT0:
			if (option->call)
				option->dest.func_int(0);
			else
				*(int *)option->dest.object = 0;
			break;
		case XCONFIG_INT1:
			if (option->call)
				option->dest.func_int(1);
			else
				*(int *)option->dest.object = 1;
			break;
		case XCONFIG_DOUBLE:
			{
				double val = strtod(arg, NULL);
				if (option->call)
					option->dest.func_double(val);
				else
					*(double *)option->dest.object = val;
			}
			break;
		case XCONFIG_STRING:
			if (option->call)
				option->dest.func_string(arg);
			else
				*(char **)option->dest.object = g_strdup(arg);
			break;
		case XCONFIG_NULL:
			if (option->call)
				option->dest.func_null();
			break;
		case XCONFIG_ENUM: {
			int val = lookup_enum(arg, (struct xconfig_enum *)option->ref);
			if (option->call)
				option->dest.func_int(val);
			else
				*(int *)option->dest.object = val;
			}
			break;
		default:
			break;
	}
}

/* returns 0 if it's a value option to unset */
static int unset_option(struct xconfig_option *option) {
	switch (option->type) {
	case XCONFIG_BOOL:
		if (option->call)
			option->dest.func_bool(0);
		else
			*(_Bool *)option->dest.object = 0;
		return 0;
	case XCONFIG_BOOL0:
		if (option->call)
			option->dest.func_bool(1);
		else
			*(_Bool *)option->dest.object = 1;
		return 0;
	case XCONFIG_INT0:
		if (option->call)
			option->dest.func_int(1);
		else
			*(int *)option->dest.object = 1;
		return 0;
	case XCONFIG_INT1:
		if (option->call)
			option->dest.func_int(0);
		else
			*(int *)option->dest.object = 0;
		return 0;
	case XCONFIG_STRING:
		if (option->call) {
			option->dest.func_string(NULL);
		} else if (*(char **)option->dest.object) {
			g_free(*(char **)option->dest.object);
			*(char **)option->dest.object = NULL;
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
		while (isspace((int)*line))
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
				while (isspace((int)*arg) || *arg == '=') {
					arg++;
				}
			}
		} else {
			arg = strtok(NULL, "\n\v\f\r");
			if (arg) {
				int i;
				for (i = strlen(arg)-1; i >= 0; i--) {
					if (isspace(arg[i]))
						arg[i] = 0;
				}
			}
		}
		if (option->type == XCONFIG_BOOL ||
		    option->type == XCONFIG_BOOL0 ||
		    option->type == XCONFIG_INT0 ||
		    option->type == XCONFIG_INT1 ||
		    option->type == XCONFIG_NULL) {
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
		    option->type == XCONFIG_INT0 ||
		    option->type == XCONFIG_INT1 ||
		    option->type == XCONFIG_NULL) {
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
