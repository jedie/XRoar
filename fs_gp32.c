/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2005  Ciaran Anscomb
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
#include <gpdef.h>
#include <gpstdlib.h>
#include <gpgraphic.h>
#include <gpstdio.h>
#include <gpfont.h>

#include "fs.h"
#include "types.h"

static char cwd[1024];

void fs_init(void) {
	GpFatInit();
	strcpy(cwd, "gp:\\gpmm\\dragon\\");
	GpRelativePathSet(cwd);
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

int fs_chdir(char *path) {
	char *p;
	if (!path)
		return -1;
	if (!strcmp(path, "."))
		return 0;
	if (!strcmp(path, "..")) {
		p = strrchr2(cwd, '\\');
		if (p)
			*(p+1) = 0;
		GpRelativePathSet(cwd);
		return 0;
	}
	for (p = path; *p; p++) {
		if (*p == '/')
			*p = '\\';
	}
	if (!strncmp(path, "gp:\\", 4)) {
		strcpy(cwd, path);
		GpRelativePathSet(cwd);
		return 0;
	}
	if (path[0] == '\\') {
		strcpy(cwd, "gp:");
		strcat(cwd, path);
		GpRelativePathSet(cwd);
		return 0;
	}
	strcat(cwd, path);
	strcat(cwd, "\\");
	GpRelativePathSet(cwd);
	return 0;
}

FS_FILE fs_open(const char *filename, int flags) {
	F_HANDLE fd;
	if (flags & FS_WRITE) {
		if (GpFileCreate(filename, ALWAYS_CREATE, &fd) != SM_OK)
			return -1;
	} else {
		if (GpFileOpen(filename, OPEN_R, &fd) != SM_OK)
			return -1;
	}
	return fd;
}

ssize_t fs_read(FS_FILE fd, void *buffer, size_t size) {
	unsigned long count;
	int rbytes = 0;
	unsigned char *buf = (unsigned char *)buffer;
	ERR_CODE err;
	/* Read in 512 byte chunks, or the GP32 crashes! */
	do {
		err = GpFileRead(fd, buf, size>512?512:size, &count);
		if (err != SM_OK && err != ERR_EOF)
			return -1;
		size -= count;
		buf += count;
		rbytes += count;
	} while (err == SM_OK && size > 0);
	return rbytes;
}

ssize_t fs_write(FS_FILE fd, const void *buffer, size_t size) {
	ERR_CODE err;
	/* Haven't done any file writing yet, so don't know if similar
	 * contortions are required */
	err = GpFileWrite(fd, buffer, (unsigned long)size);
	if (err != SM_OK)
		return -1;
	return size;
}

void fs_close(FS_FILE fd) {
	GpFileClose(fd);
}

int fs_scandir(const char *dir, struct dirent ***namelist,
		int (*filter)(struct dirent *),
		int (*compar)(const void *, const void *)) {
	struct dirent **array;
	struct dirent entry;
	struct dirent *new;
	unsigned long i, count, p_num, dummy;
	GPDIRENTRY gpentry;
	GPFILEATTR gpentry_attr;
	if (!strcmp(dir, ".")) {
		dir = cwd;
	}
	GpDirEnumNum(dir, &p_num);
	array = (struct dirent **)malloc((p_num+1) * sizeof(struct dirent *));
	if (array == NULL)
		return -1;
	count = 0;
	for (i = 0; i < p_num; i++) {
		GpDirEnumList(dir, i, 1, &gpentry, &dummy);
		GpFileAttr(gpentry.name,  &gpentry_attr);
		memcpy(entry.d_name, gpentry.name, 16);
		entry.attr = gpentry_attr.attr;
		entry.size = gpentry_attr.size;
		if (filter == NULL || filter(&entry)) {
			new = (struct dirent *)malloc(sizeof(struct dirent));
			if (new == NULL) {
				/* one fails, they all fail */
				for (; count; count--)
					free(array[count-1]);
				free(array);
				return -1;
			}
			memcpy(new, &entry, sizeof(struct dirent));
			array[count++] = new;
		}
	}
	array[count] = NULL;
	*namelist = array;
	/* Sort results */
	if (count > 1 && compar)
		qsort(array, count, sizeof(struct dirent *), compar);
	return count;
}

void fs_freedir(struct dirent ***namelist) {
	struct dirent **cur;
	for (cur = *namelist; *cur; cur++)
		free(*cur);
	free(*namelist);
}

ssize_t fs_load_file(char *filename, void *buf, size_t size) {
	int count;
	FS_FILE fd;
	if ((fd = fs_open(filename, FS_READ)) == -1)
		return -1;
	count = fs_read(fd, buf, size);
	fs_close(fd);
	return count;
}

int fs_write_byte(FS_FILE fd, uint8_t octet) {
	return fs_write(fd, &octet, 1);
}

int fs_write_word16(FS_FILE fd, uint16_t word16) {
	if (fs_write_byte(fd, word16 >> 8) == -1)
		return -1;
	return fs_write_byte(fd, word16 & 0xff);
}

int fs_write_word32(FS_FILE fd, uint32_t word32) {
	if (fs_write_word16(fd, word32 >> 16) == -1)
		return -1;
	return fs_write_word16(fd, word32 & 0xffff);
}

int fs_read_byte(FS_FILE fd, uint8_t *dest) {
	return fs_read(fd, dest, 1);
}

int fs_read_word16(FS_FILE fd, uint16_t *dest) {
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

int fs_read_word32(FS_FILE fd, uint32_t *dest) {
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
