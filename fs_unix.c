/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2004  Ciaran Anscomb
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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "fs.h"
#include "types.h"

static const char *fs_error = "";

void fs_init(void) {
}

FS_FILE fs_open(char *filename, int flags) {
	int fd;
	if (flags & FS_WRITE)
		fd = open(filename, O_CREAT|O_WRONLY, 0644);
	else
		fd = open(filename, O_RDONLY);
	if (fd == -1) {
		fs_error = strerror(errno);
		return -1;
	}
	return fd;
}

ssize_t fs_read(FS_FILE fd, void *buffer, size_t size) {
	int count = 0, tries = 3;
	ssize_t r;
	unsigned char *buf = (unsigned char *)buffer;
	do {
		do {
			r = read(fd, buf, size);
		} while (r == -1 && errno == EINTR && --tries);
		if (r == -1) {
			fs_error = strerror(errno);
			return -1;
		}
		size -= r;
		buf += r;
		count += r;
	} while (r && size > 0);
	return count;
}

ssize_t fs_write(FS_FILE fd, void *buffer, size_t size) {
	int count = 0, tries = 3;
	ssize_t r;
	unsigned char *buf = (unsigned char *)buffer;
	do {
		do {
			r = write(fd, buf, size);
		} while (r == -1 && errno == EINTR && --tries);
		if (r == -1) {
			fs_error = strerror(errno);
			return -1;
		}
		size -= r;
		buf += r;
		count += r;
	} while (r && size > 0);
	return count;
}

void fs_close(FS_FILE fd) {
	close(fd);
}

ssize_t fs_load_file(char *filename, void *buf, size_t size) {
	ssize_t count;
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
