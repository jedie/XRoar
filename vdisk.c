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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "fs.h"
#include "logging.h"
#include "vdisk.h"
#include "vdrive.h"

static struct {
	const char *ext;
	int (*load_func)(const char *, unsigned int);
} dispatch[] = {
	{ "VDK", vdisk_load_vdk },
	{ "JVC", vdisk_load_jvc },
	{ "DSK", vdisk_load_jvc },
	{ NULL,  vdisk_load_jvc }
};

int vdisk_load(const char *filename, unsigned int drive) {
	char ext[4];
	int len = strlen(filename);
	int i;
	if (len < 4)
		return vdisk_load_vdk(filename, drive);
	for (i = 0; i < 3; i++)
		ext[i] = toupper(filename[i+len-3]);
	ext[3] = 0;
	i = 0;
	do {
		if (strcmp(ext, dispatch[i].ext) == 0)
			break;
		i++;
	} while (dispatch[i].ext != NULL);
	return dispatch[i].load_func(filename, drive);
}

int vdisk_load_vdk(const char *filename, unsigned int drive) {
	ssize_t file_size;
	unsigned int header_size;
	unsigned int num_tracks;
	unsigned int num_sides = 1;
	unsigned int num_sectors = 18;
	unsigned int ssize_code = 1, ssize;
	unsigned int track, sector, side;
	uint8_t buf[1024];
	FS_FILE fd;
	if ((file_size = fs_size(filename)) < 0)
		return -1;
	if ((fd = fs_open(filename, FS_READ)) < 0)
		return -1;
	fs_read(fd, buf, 12);
	file_size -= 12;
	if (buf[0] != 'd' || buf[1] != 'k') {
		fs_close(fd);
		return -1;
	}
	header_size = (buf[2] | (buf[3]<<8)) - 12;
	num_tracks = buf[8];
	num_sides = buf[9];
	if (header_size > 0)
		fs_read(fd, buf, header_size);
	ssize = 128 << ssize_code;
	drive %= 3;
	if (vdrive_format_disk(drive, num_tracks, num_sides, num_sectors, ssize_code, 1) < 0) {
		fs_close(fd);
		return -1;
	}
	LOG_DEBUG(2,"Loading VDK virtual disk to drive %d.  %d Tr, %d Si, %d Se.\n", drive, num_tracks, num_sides, num_sectors);
	for (track = 0; track < num_tracks; track++) {
		for (side = 0; side < num_sides; side++) {
			for (sector = 0; sector < num_sectors; sector++) {
				fs_read(fd, buf, ssize);
				vdrive_update_sector(drive, track, side, sector + 1, ssize_code, buf);
			}
		}
	}
	fs_close(fd);
	return 0;
}

int vdisk_load_jvc(const char *filename, unsigned int drive) {
	ssize_t file_size = fs_size(filename);
	unsigned int header_size;
	unsigned int num_tracks, tracks_to_format;
	unsigned int num_sides = 1;
	unsigned int num_sectors = 18;
	unsigned int ssize_code = 1, ssize;
	unsigned int first_sector = 1;
	unsigned int sector_attr = 0;
	unsigned int track, sector, side;
	uint8_t buf[1025];
	FS_FILE fd;
	if (file_size < 0)
		return -1;
	header_size = file_size % 256;
	file_size -= header_size;
	/* Supposedly, we are supposed to default to single sided if there's
	 * no header information overriding, but I found double sided
	 * headerless DSK files. */
	if (file_size > 198144) num_sides = 2;
	fd = fs_open(filename, FS_READ);
	if (fd < 0)
		return -1;
	if (header_size > 0) {
		fs_read(fd, buf, header_size);
		num_sectors = buf[0];
	}
	if (header_size > 1) num_sides = buf[1];
	if (header_size > 2) ssize_code = buf[2] & 3;
	ssize = 128 << ssize_code;
	if (header_size > 3) first_sector = buf[3];
	if (header_size > 4) sector_attr = buf[4] ? 1 : 0;
	if (sector_attr == 0)
		num_tracks = file_size / (num_sectors * ssize) / num_sides;
	else
		num_tracks = file_size / (num_sectors * (ssize+1)) / num_sides;
	drive %= 3;
	tracks_to_format = num_tracks;
	if (tracks_to_format < 40)
		tracks_to_format = 40;
	if (vdrive_format_disk(drive, tracks_to_format, num_sides, num_sectors, ssize_code, first_sector) < 0) {
		fs_close(fd);
		return -1;
	}
	LOG_DEBUG(2,"Loading JVC virtual disk to drive %d.  %d Tr, %d Si, %d Se.\n", drive, num_tracks, num_sides, num_sectors);
	for (track = 0; track < num_tracks; track++) {
		for (side = 0; side < num_sides; side++) {
			for (sector = 0; sector < num_sectors; sector++) {
				fs_read(fd, buf, ssize + sector_attr);
				vdrive_update_sector(drive, track, side, sector + first_sector, ssize_code, &buf[sector_attr]);
			}
		}
	}
	fs_close(fd);
	return 0;
}
