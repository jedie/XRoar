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

#include "types.h"
#include "logging.h"
#include "fs.h"
#include "m6809.h"
#include "hexs19.h"
#include "machine.h"

static int dragon_bin_load(int fd, int autorun);
static int coco_bin_load(int fd, int autorun);

static uint8_t read_nibble(int fd) {
	int in;
	in = fs_read_uint8(fd);
	if (in >= '0' && in <= '9')
		return (in-'0');
	in |= 0x20;
	if (in >= 'a' && in <= 'f')
		return (in-'a')+10;
	return 0xff;
}

static uint8_t read_byte(int fd) {
	return read_nibble(fd) << 4 | read_nibble(fd);
}

static uint16_t read_word(int fd) {
	return read_byte(fd) << 8 | read_byte(fd);
}

static int skip_eol(int fd) {
	int d;
	do {
		d = fs_read_uint8(fd);
	} while (d >= 0 && d != 10);
	if (d >= 0)
		return 1;
	return 0;
}

int intel_hex_read(const char *filename) {
	int fd;
	int data, length, addr, type, sum;
	int i;
	if (filename == NULL)
		return -1;
	if ((fd = fs_open(filename, FS_READ)) == -1)
		return -1;
	LOG_DEBUG(2, "Reading HEX file\n");
	while ((data = fs_read_uint8(fd)) >= 0) {
		if (data != ':') {
			fs_close(fd);
			return -1;
		}
		length = read_byte(fd);
		addr = read_word(fd);
		type = read_byte(fd);
		if (type == 0) {
			LOG_DEBUG(3,"Loading $%x bytes to %04x\n", length, addr);
		}
		for (i = 0; i < length; i++) {
			data = read_byte(fd);
			if (type == 0) {
				LOG_DEBUG(5,"%02x ", (int)data);
				ram0[addr] = data;
				addr++;
			}
		}
		if (type == 0) {
			LOG_DEBUG(5,"\n");
		}
		sum = read_byte(fd);
		(void)sum;  /* XXX check this */
		if (skip_eol(fd) == 0)
			break;
		if (type == 1)
			break;
	}
	fs_close(fd);
	return 0;
}

int bin_load(const char *filename, int autorun) {
	int fd, type;
	if (filename == NULL)
		return -1;
	if ((fd = fs_open(filename, FS_READ)) == -1)
		return -1;
	type = fs_read_uint8(fd);
	switch (type) {
	case 0x55:
		return dragon_bin_load(fd, autorun);
	case 0x00:
		return coco_bin_load(fd, autorun);
	default:
		break;
	}
	LOG_DEBUG(2, "Unknown binary file type.\n");
	fs_close(fd);
	return -1;
}

static int dragon_bin_load(int fd, int autorun) {
	int filetype, length, load, exec;
	LOG_DEBUG(2, "Reading Dragon BIN file\n");
	filetype = fs_read_uint8(fd);
	(void)filetype;  /* XXX verify this makes sense */
	load = fs_read_uint16(fd);
	length = fs_read_uint16(fd);
	exec = fs_read_uint16(fd);
	(void)fs_read_uint8(fd);
	LOG_DEBUG(3,"\tLoading $%x bytes to $%04x\n", length, load);
	fs_read(fd, &ram0[load], length);
	if (autorun) {
		LOG_DEBUG(3,"\tExecuting from $%04x\n", exec);
		m6809_jump(exec);
	} else {
		ram0[0x9d] = exec >> 8;
		ram0[0x9e] = exec & 0xff;
	}
	fs_close(fd);
	return 0;
}

static int coco_bin_load(int fd, int autorun) {
	int data, length, load, exec;
	LOG_DEBUG(2, "Reading CoCo BIN file\n");
	fs_lseek(fd, 0, FS_SEEK_SET);
	while ((data = fs_read_uint8(fd)) >= 0) {
		if (data == 0) {
			length = fs_read_uint16(fd);
			load = fs_read_uint16(fd);
			LOG_DEBUG(3,"\tLoading $%x bytes to $%04x\n", length, load);
			fs_read(fd, &ram0[load], length);
			continue;
		}
		if (data == 0xff) {
			(void)fs_read_uint16(fd);  /* skip 0 */
			exec = fs_read_uint16(fd);
			if (autorun) {
				LOG_DEBUG(3,"\tExecuting from $%04x\n", exec);
				m6809_jump(exec);
			} else {
				ram0[0x9d] = exec >> 8;
				ram0[0x9e] = exec & 0xff;
			}
			break;
		}
	}
	fs_close(fd);
	return 0;
}
