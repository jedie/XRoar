/*  Copyright 2003-2014 Ciaran Anscomb
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

/* Change of behaviour: when not autorunning, these loaders used to adjust the
 * BASIC EXEC address at $009D:$009E.  They no longer do, as BASIC might not be
 * in control. */

#include "config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "fs.h"
#include "hexs19.h"
#include "logging.h"
#include "machine.h"
#include "mc6809.h"
#include "xroar.h"

static int dragon_bin_load(FILE *fd, int autorun);
static int coco_bin_load(FILE *fd, int autorun);

static uint8_t read_nibble(FILE *fd) {
	int in;
	in = fs_read_uint8(fd);
	if (in >= '0' && in <= '9')
		return (in-'0');
	in |= 0x20;
	if (in >= 'a' && in <= 'f')
		return (in-'a')+10;
	return 0xff;
}

static uint8_t read_byte(FILE *fd) {
	return read_nibble(fd) << 4 | read_nibble(fd);
}

static uint16_t read_word(FILE *fd) {
	return read_byte(fd) << 8 | read_byte(fd);
}

static int skip_eol(FILE *fd) {
	int d;
	do {
		d = fs_read_uint8(fd);
	} while (d >= 0 && d != 10);
	if (d >= 0)
		return 1;
	return 0;
}

int intel_hex_read(const char *filename, int autorun) {
	FILE *fd;
	int data;
	uint8_t rsum;
	uint16_t exec = 0;
	struct log_handle *log_hex = NULL;
	if (filename == NULL)
		return -1;
	if (!(fd = fopen(filename, "rb")))
		return -1;
	LOG_DEBUG(1, "Reading Intel HEX record file\n");
	if (xroar_cfg.debug_file & XROAR_DEBUG_FILE_BIN_DATA)
		log_open_hexdump(&log_hex, "Intel HEX read: ");
	while ((data = fs_read_uint8(fd)) >= 0) {
		if (data != ':') {
			fclose(fd);
			if (xroar_cfg.debug_file & XROAR_DEBUG_FILE_BIN_DATA) {
				log_hexdump_flag(log_hex);
				log_close(&log_hex);
			}
			return -1;
		}
		int length = read_byte(fd);
		int addr = read_word(fd);
		int type = read_byte(fd);
		if (type == 0 && (xroar_cfg.debug_file & XROAR_DEBUG_FILE_BIN_DATA))
			log_hexdump_set_addr(log_hex, addr);
		rsum = length + (length >> 8) + addr + (addr >> 8) + type;
		for (int i = 0; i < length; i++) {
			data = read_byte(fd);
			rsum += data;
			if (type == 0) {
				if (xroar_cfg.debug_file & XROAR_DEBUG_FILE_BIN_DATA)
					log_hexdump_byte(log_hex, data);
				machine_ram[addr] = data;
				addr++;
			}
		}
		int sum = read_byte(fd);
		rsum = ~rsum + 1;
		if (sum != rsum) {
			if (xroar_cfg.debug_file & XROAR_DEBUG_FILE_BIN_DATA)
				log_hexdump_flag(log_hex);
		}
		if (skip_eol(fd) == 0)
			break;
		if (type == 1) {
			exec = addr;
			break;
		}
	}

	if (xroar_cfg.debug_file & XROAR_DEBUG_FILE_BIN_DATA)
		log_close(&log_hex);
	if (exec != 0) {
		if (autorun) {
			struct MC6809 *cpu = machine_get_cpu(0);
			if (xroar_cfg.debug_file & XROAR_DEBUG_FILE_BIN)
				LOG_PRINT("Intel HEX: EXEC $%04x - autorunning\n", exec);
			cpu->jump(cpu, exec);
		} else {
			if (xroar_cfg.debug_file & XROAR_DEBUG_FILE_BIN)
				LOG_PRINT("Intel HEX: EXEC $%04x - not autorunning\n", exec);
		}
	}

	fclose(fd);
	return 0;
}

int bin_load(const char *filename, int autorun) {
	FILE *fd;
	int type;
	if (filename == NULL)
		return -1;
	if (!(fd = fopen(filename, "rb")))
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
	LOG_DEBUG(1, "Unknown binary file type.\n");
	fclose(fd);
	return -1;
}

static int dragon_bin_load(FILE *fd, int autorun) {
	int filetype, load, exec;
	size_t length;
	LOG_DEBUG(1, "Reading Dragon BIN file\n");
	filetype = fs_read_uint8(fd);
	(void)filetype;  // XXX verify this makes sense
	load = fs_read_uint16(fd);
	length = fs_read_uint16(fd);
	exec = fs_read_uint16(fd);
	(void)fs_read_uint8(fd);
	if (xroar_cfg.debug_file & XROAR_DEBUG_FILE_BIN)
		LOG_PRINT("Dragon BIN: LOAD $%04zx bytes to $%04x, EXEC $%04x\n", length, load, exec);
	struct log_handle *log_bin = NULL;
	if (xroar_cfg.debug_file & XROAR_DEBUG_FILE_BIN_DATA) {
		log_open_hexdump(&log_bin, "Dragon BIN read: ");
		log_hexdump_set_addr(log_bin, load);
	}
	for (size_t i = 0; i < length; i++) {
		int data = fs_read_uint8(fd);
		if (data < 0) {
			log_hexdump_flag(log_bin);
			log_close(&log_bin);
			LOG_WARN("Dragon BIN: short read\n");
			break;
		}
		machine_write_byte((load + i) & 0xffff, data);
		log_hexdump_byte(log_bin, data);
	}
	log_close(&log_bin);
	if (autorun) {
		struct MC6809 *cpu = machine_get_cpu(0);
		if (xroar_cfg.debug_file & XROAR_DEBUG_FILE_BIN)
			LOG_PRINT("Dragon BIN: EXEC $%04x - autorunning\n", exec);
		cpu->jump(cpu, exec);
	} else {
		if (xroar_cfg.debug_file & XROAR_DEBUG_FILE_BIN)
			LOG_PRINT("Dragon BIN: EXEC $%04x - not autorunning\n", exec);
	}
	fclose(fd);
	return 0;
}

static int coco_bin_load(FILE *fd, int autorun) {
	size_t length;
	int chunk, load, exec;
	LOG_DEBUG(1, "Reading CoCo BIN file\n");
	fseek(fd, 0, SEEK_SET);
	while ((chunk = fs_read_uint8(fd)) >= 0) {
		if (chunk == 0) {
			length = fs_read_uint16(fd);
			load = fs_read_uint16(fd);
			if (xroar_cfg.debug_file & XROAR_DEBUG_FILE_BIN)
				LOG_PRINT("CoCo BIN: LOAD $%04zx bytes to $%04x\n", length, load);
			// Generate a hex dump per chunk
			struct log_handle *log_bin = NULL;
			if (xroar_cfg.debug_file & XROAR_DEBUG_FILE_BIN_DATA) {
				log_open_hexdump(&log_bin, "CoCo BIN: read: ");
				log_hexdump_set_addr(log_bin, load);
			}
			for (size_t i = 0; i < length; i++) {
				int data = fs_read_uint8(fd);
				if (data < 0) {
					log_hexdump_flag(log_bin);
					log_close(&log_bin);
					LOG_WARN("CoCo BIN: short read in data chunk\n");
					break;
				}
				machine_write_byte((load + i) & 0xffff, data);
				log_hexdump_byte(log_bin, data);
			}
			log_close(&log_bin);
			continue;
		} else if (chunk == 0xff) {
			(void)fs_read_uint16(fd);  // skip 0
			exec = fs_read_uint16(fd);
			if (exec < 0) {
				LOG_WARN("CoCo BIN: short read in exec chunk\n");
				break;
			}
			if (autorun) {
				struct MC6809 *cpu = machine_get_cpu(0);
				if (xroar_cfg.debug_file & XROAR_DEBUG_FILE_BIN)
					LOG_PRINT("CoCo BIN: EXEC $%04x - autorunning\n", exec);
				cpu->jump(cpu, exec);
			} else {
				if (xroar_cfg.debug_file & XROAR_DEBUG_FILE_BIN)
					LOG_PRINT("CoCo BIN: EXEC $%04x - not autorunning\n", exec);
			}
			break;
		} else {
			LOG_WARN("CoCo BIN: unknown chunk type 0x%02x\n", chunk);
			break;
		}
	}
	fclose(fd);
	return 0;
}
