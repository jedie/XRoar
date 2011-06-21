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

#include <stdlib.h>
#include <string.h>
#include <gpdef.h>
#include <gpstdlib.h>
#include <gpgraphic.h>
#include <gpstdio.h>
#include <gpfont.h>

#include "fs.h"
#include "types.h"

static int initialised = 0;
static F_HANDLE fd_cache[256];

static char cwd[1024];

static void init(void) {
	int i;
	for (i = 0; i < 256; i++)
		fd_cache[i] = -1;
	GpFatInit();
	strcpy(cwd, "gp:\\gpmm\\dragon\\");
	GpRelativePathSet(cwd);
	initialised = 1;
}

static int next_fd(void) {
	int i;
	if (!initialised) init();
	for (i = 0; i < 256; i++)
		if (fd_cache[i] == -1)
			return i;
	return -1;
}

static char *strrchr2(char *s, int c) {
	char *s1 = NULL, *s2 = NULL;
	for (; *s; s++) {
		if (*s == c) {
			s1 = s2;
			s2 = s;
		}
	}
	return s1;
}

int fs_chdir(const char *path) {
	char *pcopy;
	char *p;
	if (path == NULL)
		return -1;
	pcopy = malloc(strlen(path)+1);
	if (pcopy == NULL)
		return -1;
	strcpy(pcopy, path);
	if (!strcmp(pcopy, ".")) {
		GpRelativePathSet(cwd);
		return 0;
	}
	if (!strcmp(pcopy, "..")) {
		p = strrchr2(cwd, '\\');
		if (p)
			*(p+1) = 0;
		GpRelativePathSet(cwd);
		return 0;
	}
	/* Translate / into \ */
	for (p = pcopy; *p; p++) {
		if (*p == '/')
			*p = '\\';
	}
	if (!strncmp(pcopy, "gp:\\", 4)) {
		strcpy(cwd, pcopy);
		GpRelativePathSet(cwd);
		return 0;
	}
	if (pcopy[0] == '\\') {
		strcpy(cwd, "gp:");
		strcat(cwd, pcopy);
		if (*(p-1) != '\\') strcat(cwd, "\\");
		GpRelativePathSet(cwd);
		return 0;
	}
	strcat(cwd, pcopy);
	if (*(p-1) != '\\') strcat(cwd, "\\");
	GpRelativePathSet(cwd);
	return 0;
}

int fs_open(const char *filename, int flags) {
	F_HANDLE fh;
	const char *basename;
	int fd;
	if ((fd = next_fd()) == -1)
		return -1;
	if ((basename = strrchr(filename, '\\'))
			|| (basename = strrchr(filename, '/'))) {
		int length = (basename - filename) + 1;
		char *dir = malloc(length+1);
		if (!dir) return -1;
		strncpy(dir, filename, length);
		dir[length] = 0;
		fs_chdir(dir);
		free(dir);
		basename++;
	} else {
		basename = filename;
	}
	if (flags & FS_WRITE) {
		if (GpFileOpen(basename, OPEN_W, &fh) != SM_OK) {
			GpFileCreate(basename, ALWAYS_CREATE, &fh);
			if (GpFileOpen(basename, OPEN_W, &fh) != SM_OK) {
				return -1;
			}
		}
	} else {
		if (GpFileOpen(basename, OPEN_R, &fh) != SM_OK)
			return -1;
	}
	SPEED_SLOW;
	fd_cache[fd] = fh;
	return fd;
}

ssize_t fs_read(int fd, void *buffer, size_t size) {
	unsigned long count;
	int rbytes = 0;
	unsigned char *buf = (unsigned char *)buffer;
	ERR_CODE err;
	/* Read in 512 byte chunks, or the GP32 crashes! */
	do {
		err = GpFileRead(fd_cache[fd], buf, size>512?512:size, &count);
		if (err != SM_OK && err != ERR_EOF)
			return -1;
		size -= count;
		buf += count;
		rbytes += count;
	} while (err == SM_OK && size > 0);
	return rbytes;
}

ssize_t fs_write(int fd, const void *buffer, size_t size) {
	ERR_CODE err;
	err = GpFileWrite(fd_cache[fd], buffer, (unsigned long)size);
	if (err != SM_OK)
		return -1;
	return size;
}

void fs_close(int fd) {
	GpFileClose(fd_cache[fd]);
	SPEED_FAST;
}

ssize_t fs_size(const char *filename) {
	unsigned long size;
	GpFileGetSize(filename, &size);
	return (ssize_t)size;
}

char *fs_getcwd(char *buf, size_t size) {
	if (buf == NULL)
		return NULL;
	if ((strlen(cwd) + 1) > size)
		return NULL;
	strcpy(buf, cwd);
	return buf;
}

ssize_t fs_load_file(char *filename, void *buf, size_t size) {
	int count;
	int fd;
	if ((fd = fs_open(filename, FS_READ)) == -1)
		return -1;
	count = fs_read(fd, buf, size);
	fs_close(fd);
	return count;
}

int fs_write_byte(int fd, uint8_t octet) {
	return fs_write(fd, &octet, 1);
}

int fs_write_word16(int fd, uint16_t word16) {
	if (fs_write_byte(fd, word16 >> 8) == -1)
		return -1;
	return fs_write_byte(fd, word16 & 0xff);
}

int fs_write_word32(int fd, uint32_t word32) {
	if (fs_write_word16(fd, word32 >> 16) == -1)
		return -1;
	return fs_write_word16(fd, word32 & 0xffff);
}

int fs_read_byte(int fd, uint8_t *dest) {
	return fs_read(fd, dest, 1);
}

int fs_read_word16(int fd, uint16_t *dest) {
	uint8_t octet;
	int ret;
	if (fs_read(fd, &octet, 1) == -1)
		return -1;
	*dest = octet << 8;
	if ((ret = fs_read(fd, &octet, 1)) == -1)
		return -1;
	*dest |= octet;
	return ret;
}

int fs_read_word32(int fd, uint32_t *dest) {
	uint16_t word16;
	int ret;
	if (fs_read_word16(fd, &word16) == -1)
		return -1;
	*dest = word16 << 16;
	if ((ret = fs_read_word16(fd, &word16)) == -1)
		return -1;
	*dest |= word16;
	return ret;
}
