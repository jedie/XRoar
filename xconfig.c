/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2009  Ciaran Anscomb
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
#include "xconfig.h"

enum xconfig_result xconfig_parse_cli(struct xconfig_option *options,
		int argc, char **argv, int *argn) {
	int _argn, i;
	_argn = argn ? *argn : 1;

	while (_argn < argc) {
		if (argv[_argn][0] != '-') {
			break;
		}
		if (0 == strcmp("--", argv[_argn])) {
			_argn++;
			break;
		}
		for (i = 0; options[i].type != XCONFIG_END; i++) {
			if (0 == strcmp(options[i].name, argv[_argn]+1)) {
				if (options[i].type == XCONFIG_BOOL) {
					*(int *)options[i].dest = 1;
				} else {
					if ((_argn + 1) >= argc) {
						if (argn) *argn = _argn;
						return XCONFIG_MISSING_ARG;
					}
					switch (options[i].type) {
						case XCONFIG_INT:
							*(int *)options[i].dest = strtol(argv[++_argn], NULL, 0);
							break;
						case XCONFIG_FLOAT:
							*(float *)options[i].dest = strtof(argv[++_argn], NULL);
							break;
						case XCONFIG_STRING:
							*(char **)options[i].dest = strdup(argv[++_argn]);
							break;
						default:
							break;
					}
				}
				break;
			}
		}
		if (options[i].type == XCONFIG_END) {
			if (argn) *argn = _argn;
			return XCONFIG_BAD_OPTION;
		}
		_argn++;
	}
	if (argn) *argn = _argn;
	return XCONFIG_OK;
}
