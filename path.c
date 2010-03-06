/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2010  Ciaran Anscomb
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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "types.h"
#include "path.h"

static char *strcattoc_esc(char *dst, const char *src, char c);

/* Find file within supplied colon-separated path.  In path elements, "~/" at
 * the start is expanded to "$HOME/" and "\" escapes the following character
 * (e.g., "\:" to stop a colon being seen as a path separator).
 *
 * Files are only considered if they are regular files (not sockets,
 * directories, etc.) and are readable by the user.  This is not intended as a
 * security check, just a convenience. */

/* TODO: calls to stat() and access() need to be abstracted out into fs_*.c, as
 * they will differ on GP32.  Perhaps roll them into one function that tests
 * for readable regular file. */

char *find_in_path(const char *path, const char *filename) {
	struct stat statbuf;
	const char *home;
	char *buf;
	int buf_size;

	if (filename == NULL)
		return NULL;
	/* If no path or filename contains a directory, just test file */
	if (path == NULL || strchr(filename, '/')) {
		if (stat(filename, &statbuf) == 0)
			if (statbuf.st_mode & S_IFREG)
				if (access(filename, R_OK) == 0) {
					return strdup(filename);
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
	buf = malloc(buf_size);
	if (buf == NULL)
		return NULL;
	while (*path) {
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
			if (statbuf.st_mode & S_IFREG)
				if (access(buf, R_OK) == 0) {
					return buf;
				}
		/* Skip to next path element */
		while (*path && *path != ':') {
			if (*path == '\\' && *(path+1) != 0)
				path++;  /* skip escaped char */
			path++;
		}
		if (*path == ':')
			path++;
	}
	free(buf);
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
