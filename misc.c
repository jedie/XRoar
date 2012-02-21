/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2012  Ciaran Anscomb
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "types.h"
#include "misc.h"

void *xmalloc(size_t size) {
	void *m = malloc(size);
	if (!m) {
		perror(NULL);
		exit(1);
	}
	return m;
}

void *xrealloc(void *ptr, size_t size) {
	void *m = realloc(ptr, size);
	if (!m) {
		perror(NULL);
		exit(1);
	}
	return m;
}

char *xstrdup(const char *s1) {
	char *r = strdup(s1);
	if (!r) {
		perror(NULL);
		exit(1);
	}
	return r;
}
