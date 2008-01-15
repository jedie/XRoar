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

#include "types.h"
#include "sam.h"
#include "logging.h"
#include "fs.h"
#include "m6809.h"
#include "hexs19.h"
#include "machine.h"

static uint8_t read_nibble(int fd) {
	uint8_t in;
	fs_read_byte(fd, &in);
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
	int r;
	uint8_t d;
	do {
		r = fs_read_byte(fd, &d);
	} while (r > 0 && d != 10);
	if (r > 0)
		return 1;
	return 0;
}

int intel_hex_read(char *filename) {
	int fd;
	int i, length, addr, type, sum;
	uint8_t data;
	if (filename == NULL)
		return -1;
	if ((fd = fs_open(filename, FS_READ)) == -1)
		return -1;
	LOG_DEBUG(2, "Reading HEX file\n");
	while (fs_read_byte(fd, &data) > 0) {
		if (data != ':') {
			fs_close(fd);
			return -1;
		}
		length = read_byte(fd);
		addr = read_word(fd);
		type = read_byte(fd);
		if (type == 0) {
			LOG_DEBUG(3,"Loading %d bytes to %04x\n", length, addr);
		}
		for (i = 0; i < length; i++) {
			data = read_byte(fd);
			if (type == 0) {
				LOG_DEBUG(5,"%02x ", (int)data);
				if (addr < 0x8000)
					ram0[addr] = data;
				else
					ram1[addr-0x8000] = data;
				addr++;
			}
		}
		if (type == 0) {
			LOG_DEBUG(5,"\n");
		}
		sum = read_byte(fd);
		if (skip_eol(fd) == 0)
			break;
		if (type == 1)
			break;
	}
	fs_close(fd);
	return 0;
}

int coco_bin_read(char *filename) {
	int fd;
	uint8_t tmp;
	unsigned int length, load, exec;
	int r;
	if (filename == NULL)
		return -1;
	if ((fd = fs_open(filename, FS_READ)) == -1)
		return -1;
	LOG_DEBUG(2, "Reading BIN file\n");
	while (fs_read_byte(fd, &tmp) > 0) {
		if (tmp == 0) {
			fs_read_byte(fd, &tmp); length = (tmp << 8);
			fs_read_byte(fd, &tmp); length |= tmp;
			fs_read_byte(fd, &tmp); load = (tmp << 8);
			fs_read_byte(fd, &tmp); load |= tmp;
			LOG_DEBUG(3,"\tLoading %d bytes to $%04x\n", length, load);
			if (load < 0x8000) {
				unsigned int bytes = length;
				if ((load + length) > 0x8000) {
					bytes = 0x8000 - load;
				}
				r = fs_read(fd, &ram0[load], bytes);
				load += bytes;
				length -= bytes;
			}
			if (load >= 0x8000 && length > 0) {
				r = fs_read(fd, &ram1[load-0x8000], length);
			}
			continue;
		}
		if (tmp == 0xff) {
			fs_read_byte(fd, &tmp); /* skip 0 */
			fs_read_byte(fd, &tmp); /* skip 0 */
			fs_read_byte(fd, &tmp); exec = (tmp << 8);
			fs_read_byte(fd, &tmp); exec |= tmp;
			LOG_DEBUG(3,"\tExecuting from $%04x\n", exec);
			m6809_jump(exec);
			break;
		}
	}
	fs_close(fd);
	return 0;
}
