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

#include <inttypes.h>
#include <stdio.h>

#include "fs.h"

int fs_write_uint8(FILE *stream, int value) {
	uint8_t out = value;
	return fwrite(&out, 1, 1, stream);
}

int fs_write_uint16(FILE *stream, int value) {
	uint8_t out[2];
	out[0] = value >> 8;
	out[1] = value & 0xff;
	return fwrite(out, 1, 2, stream);
}

int fs_write_uint16_le(FILE *stream, int value) {
	uint8_t out[2];
	out[0] = value & 0xff;
	out[1] = value >> 8;
	return fwrite(out, 1, 2, stream);
}

int fs_read_uint8(FILE *stream) {
	uint8_t in;
	if (fread(&in, 1, 1, stream) < 1)
		return -1;
	return in;
}

int fs_read_uint16(FILE *stream) {
	uint8_t in[2];
	if (fread(in, 1, 2, stream) < 2)
		return -1;
	return (in[0] << 8) | in[1];
}

int fs_read_uint16_le(FILE *stream) {
	uint8_t in[2];
	if (fread(in, 1, 2, stream) < 2)
		return -1;
	return (in[1] << 8) | in[0];
}
