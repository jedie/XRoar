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
#include "filereq.h"
#include "logging.h"

extern FileReqModule filereq_carbon_module;
extern FileReqModule filereq_windows32_module;
extern FileReqModule filereq_gtk_module;
extern FileReqModule filereq_cli_module;

static FileReqModule *module_list[] = {
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

/* File requesters listed in order of preference, so just get first
   one that initialises. */
FileReqModule *filereq_init(void) {
	int i;
	for (i = 0; module_list[i]; i++) {
		if (module_list[i]->init()) {
			return module_list[i];
		}
	}
	LOG_WARN("No file requester chosen.\n");
	return NULL;
}
