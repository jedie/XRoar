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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "portalib/glib.h"

#include "path.h"

static char *strcattoc_esc(char *dst, const char *src, char c);

/* Find file within supplied colon-separated path.  In path elements, "~/" at
 * the start is expanded to "$HOME/" and "\" escapes the following character
 * (e.g., "\:" to stop a colon being seen as a path separator).
 *
 * Files are only considered if they are regular files (not sockets,
 * directories, etc.) and are readable by the user.  This is not intended as a
 * security check, just a convenience. */

char *find_in_path(const char *path, const char *filename) {
	struct stat statbuf;
	const char *home;
	char *buf;
	int buf_size;

	if (filename == NULL)
		return NULL;
	/* If no path or filename contains a directory, just test file */
	if (path == NULL || *path == 0 || strchr(filename, '/')
#ifdef WINDOWS32
			|| strchr(filename, '\\')
#endif
			) {
		if (stat(filename, &statbuf) == 0) {
			if (S_ISREG(statbuf.st_mode)) {
				/* Only consider a file if user has read
				 * access.  This is NOT a security check, it's
				 * purely for usability. */
				if (access(filename, R_OK) == 0) {
					return g_strdup(filename);
				}
			}
		}
		return NULL;
	}
#ifdef WINDOWS32
	home = getenv("USERPROFILE");
#else
	home = getenv("HOME");
#endif
	/* Buffer at most could hold <path> (or ".") + '/' + <filename> + NUL.
	 * Two characters in <path> may be replaced with $HOME + '/'. */
	buf_size = strlen(path) + strlen(filename) + 3;
	if (home) {
		buf_size += strlen(home) - 1;
	}
	buf = g_try_malloc(buf_size);
	if (buf == NULL)
		return NULL;
	for (;;) {
		*buf = 0;
		/* Prefix $HOME if path elem starts "~/" */
		if (home && *path == '~' && *(path+1) == '/') {
			strcpy(buf, home);
			path += 2;
			if (buf[strlen(buf) - 1] != '/')
				strcat(buf, "/");
		}
		/* Now append path element, "/" if required and the
		 * filename */
		strcattoc_esc(buf, path, ':');
		if (*buf == 0)
			strcpy(buf, "./");
		else if (buf[strlen(buf) - 1] != '/')
			strcat(buf, "/");
		strcat(buf, filename);
		/* Return this one if file is valid */
		if (stat(buf, &statbuf) == 0)
			if (S_ISREG(statbuf.st_mode))
				if (access(buf, R_OK) == 0) {
					return buf;
				}
		/* Skip to next path element */
		while (*path && *path != ':') {
			if (*path == '\\' && *(path+1) != 0)
				path++;  /* skip escaped char */
			path++;
		}
		if (*path != ':')
			break;
		path++;
	}
	g_free(buf);
	return NULL;
}

/* Helper function appends src to the end of dst until the first occurence of
 * c.  "\" escapes the following character. */

static char *strcattoc_esc(char *dst, const char *src, char c) {
	char *ret = dst;
	while (*dst != 0)
		dst++;
	while (*src && *src != c) {
		if (*src == '\\') {
			src++;
			if (*src)
				*(dst++) = *(src++);
		} else {
			*(dst++) = *(src++);
		}
	}
	*dst = 0;
	return ret;
}
