/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2007  Ciaran Anscomb
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
#include <string.h>

#include "types.h"
#include "logging.h"
#include "fs.h"
#include "module.h"

static int init(int argc, char **argv);
static void shutdown(void);
static char *get_filename(const char **extensions);

FileReqModule filereq_cli_module = {
	{ "cli", "Command-line file requester",
	  init, 0, shutdown, NULL },
	get_filename, get_filename
};

static char fnbuf[256];

static int init(int argc, char **argv) {
	(void)argc;
	(void)argv;
	LOG_DEBUG(2, "Command-line file requester selected.\n");
	return 0;
}

static void shutdown(void) {
}

static char *get_filename(const char **extensions) {
	char *cr;
	(void)extensions;  /* unused */
	printf("Filename? ");
	fgets(fnbuf, sizeof(fnbuf), stdin);
	cr = strrchr(fnbuf, '\n');
	if (cr)
		*cr = 0;
	return fnbuf;
}
