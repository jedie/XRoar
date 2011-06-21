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

static const int whence_values[3] = { SEEK_SET, SEEK_CUR, SEEK_END };

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

off_t fs_lseek(int fd, off_t offset, int whence) {
	int real_whence;
	if (whence < 0 || whence > 2) {
		return -1;
	}
	real_whence = whence_values[whence];
	return lseek(fd, offset, real_whence);
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

int fs_write_uint8(int fd, int value) {
	uint8_t out = value;
	return fs_write(fd, &out, 1);
}

int fs_write_uint16(int fd, int value) {
	uint8_t out[2];
	out[0] = value >> 8;
	out[1] = value & 0xff;
	return fs_write(fd, out, 2);
}

int fs_write_uint16_le(int fd, int value) {
	uint8_t out[2];
	out[0] = value & 0xff;
	out[1] = value >> 8;
	return fs_write(fd, out, 2);
}

int fs_read_uint8(int fd) {
	uint8_t in;
	if (fs_read(fd, &in, 1) < 1)
		return -1;
	return in;
}

int fs_read_uint16(int fd) {
	uint8_t in[2];
	if (fs_read(fd, in, 2) < 2)
		return -1;
	return (in[0] << 8) | in[1];
}

int fs_read_uint16_le(int fd) {
	uint8_t in[2];
	if (fs_read(fd, in, 2) < 2)
		return -1;
	return (in[1] << 8) | in[0];
}
