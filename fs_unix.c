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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "types.h"
#include "fs.h"

#ifdef WINDOWS32
# define WRFLAGS (O_CREAT|O_WRONLY|O_TRUNC|O_BINARY)
# define RDFLAGS (O_RDONLY|O_BINARY)
#else
# define WRFLAGS (O_CREAT|O_WRONLY|O_TRUNC)
# define RDFLAGS (O_RDONLY)
#endif

static const char *fs_error = "";

int fs_chdir(const char *path) {
	return chdir(path);
}

int fs_open(const char *filename, int flags) {
	int fd;
	if (filename == NULL)
		return -1;
	if (flags & FS_WRITE)
		fd = open(filename, WRFLAGS, 0644);
	else
		fd = open(filename, RDFLAGS);
	if (fd == -1) {
		fs_error = strerror(errno);
		return -1;
	}
	return fd;
}

ssize_t fs_read(int fd, void *buffer, size_t size) {
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

ssize_t fs_write(int fd, const void *buffer, size_t size) {
	int count = 0, tries = 3;
	ssize_t r;
	const unsigned char *buf = (const unsigned char *)buffer;
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

void fs_close(int fd) {
	close(fd);
}

ssize_t fs_size(const char *filename) {
	struct stat sb;
	int ret;
	ret = stat(filename, &sb);
	if (ret == 0)
		return (ssize_t)sb.st_size;
	return -1;
}

char *fs_getcwd(char *buf, size_t size) {
	return getcwd(buf, size);
}

ssize_t fs_load_file(char *filename, void *buf, size_t size) {
	ssize_t count;
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
