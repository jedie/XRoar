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

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "portalib/glib.h"

#include "crc16.h"
#include "fs.h"
#include "logging.h"
#include "module.h"
#include "vdisk.h"
#include "xroar.h"

#define MAX_SIDES (2)
#define MAX_TRACKS (256)

static struct vdisk *vdisk_load_vdk(const char *filename);
static struct vdisk *vdisk_load_jvc(const char *filename);
static struct vdisk *vdisk_load_dmk(const char *filename);
static int vdisk_save_vdk(struct vdisk *disk);
static int vdisk_save_jvc(struct vdisk *disk);
static int vdisk_save_dmk(struct vdisk *disk);

static struct {
	int filetype;
	struct vdisk *(*load_func)(const char *);
	int (*save_func)(struct vdisk *);
} dispatch[] = {
	{ FILETYPE_VDK, vdisk_load_vdk, vdisk_save_vdk },
	{ FILETYPE_JVC, vdisk_load_jvc, vdisk_save_jvc },
	{ FILETYPE_DMK, vdisk_load_dmk, vdisk_save_dmk },
	{ -1, NULL, NULL }
};

struct vdisk *vdisk_blank_disk(int num_sides, int num_tracks,
		int track_length) {
	struct vdisk *new;
	uint8_t *new_track_data;
	unsigned int data_size;
	/* Ensure multiples of track_length will stay 16-bit aligned */
	if ((track_length % 2) != 0)
		track_length++;
	if (num_sides < 1 || num_sides > MAX_SIDES
			|| num_tracks < 1 || num_tracks > MAX_TRACKS
			|| track_length < 129 || track_length > 0x2940) {
		return NULL;
	}
	new = g_try_malloc(sizeof(struct vdisk));
	if (new == NULL)
		return NULL;
	data_size = num_tracks * num_sides * track_length;
	new_track_data = g_try_malloc0(data_size);
	if (new_track_data == NULL) {
		g_free(new);
		return NULL;
	}
	new->filetype = FILETYPE_DMK;
	new->filename = NULL;
	new->file_write_protect = !xroar_opt_disk_write_back;
	new->write_protect = 0;
	new->num_sides = num_sides;
	new->num_tracks = num_tracks;
	new->track_length = track_length;
	new->track_data = new_track_data;
	return new;
}

void vdisk_destroy(struct vdisk *disk) {
	if (disk == NULL) return;
	if (disk->filename) {
		g_free(disk->filename);
		disk->filename = NULL;
	}
	g_free(disk->track_data);
	g_free(disk);
}

struct vdisk *vdisk_load(const char *filename) {
	int filetype;
	int i;
	if (filename == NULL) return NULL;
	filetype = xroar_filetype_by_ext(filename);
	for (i = 0; dispatch[i].filetype >= 0 && dispatch[i].filetype != filetype; i++);
	if (dispatch[i].load_func == NULL) {
		LOG_WARN("No reader for virtual disk file type.\n");
		return NULL;
	}
	return dispatch[i].load_func(filename);
}

int vdisk_save(struct vdisk *disk, int force) {
	char *backup_filename;
	int i;
	if (disk == NULL) return 1;
	if (!force && disk->file_write_protect) {
		LOG_DEBUG(2, "Not saving disk file: write-back is disabled.\n");
		return 0;
	}
	if (disk->filename == NULL) {
		disk->filename = filereq_module->save_filename(NULL);
		if (disk->filename == NULL) {
			 LOG_WARN("No filename given: not writing disk file.\n");
			 return 1;
		}
		disk->filetype = xroar_filetype_by_ext(disk->filename);
	}
	for (i = 0; dispatch[i].filetype >= 0 && dispatch[i].filetype != disk->filetype; i++);
	if (dispatch[i].save_func == NULL) {
		LOG_WARN("No writer for virtual disk file type.\n");
		return 1;
	}
	{
		int bf_len = strlen(disk->filename) + 5;
		backup_filename = g_alloca(bf_len);
		/* Rename old file to filename.bak if that .bak file does not
		 * already exist */
		if (backup_filename != NULL) {
			snprintf(backup_filename, bf_len, "%s.bak", disk->filename);
			struct stat statbuf;
			if (stat(backup_filename, &statbuf) != 0) {
				rename(disk->filename, backup_filename);
			}
		}
	}
	return dispatch[i].save_func(disk);
}

static struct vdisk *vdisk_load_vdk(const char *filename) {
	struct vdisk *disk;
	ssize_t file_size;
	unsigned int header_size;
	unsigned int num_tracks;
	unsigned int num_sides = 1;
	unsigned int num_sectors = 18;
	unsigned int ssize_code = 1, ssize;
	unsigned int track, sector, side;
	uint8_t buf[1024];
	FILE *fd;
	struct stat statbuf;
	if (stat(filename, &statbuf) != 0)
		return NULL;
	file_size = statbuf.st_size;
	if (!(fd = fopen(filename, "rb")))
		return NULL;
	fread(buf, 12, 1, fd);
	file_size -= 12;
	(void)file_size;  /* check this matches what's going to be read */
	if (buf[0] != 'd' || buf[1] != 'k') {
		fclose(fd);
		return NULL;
	}
	header_size = (buf[2] | (buf[3]<<8)) - 12;
	num_tracks = buf[8];
	num_sides = buf[9];
	if (header_size != (buf[11] >> 3)) {
		LOG_WARN("Possibly corrupt VDK file: mismatched header size and name length\n");
	}
	if (header_size > 0) {
		fread(buf, header_size, 1, fd);
	}
	ssize = 128 << ssize_code;
	disk = vdisk_blank_disk(num_sides, num_tracks, VDISK_LENGTH_5_25);
	if (disk == NULL) {
		fclose(fd);
		return NULL;
	}
	disk->filetype = FILETYPE_VDK;
	disk->filename = g_strdup(filename);
	if (vdisk_format_disk(disk, VDISK_DOUBLE_DENSITY, num_sectors, 1, ssize_code) < 0) {
		fclose(fd);
		vdisk_destroy(disk);
		return NULL;
	}
	LOG_DEBUG(2,"Loading VDK virtual disk: %dT %dH %dS (%d-byte)\n", num_tracks, num_sides, num_sectors, ssize);
	for (track = 0; track < num_tracks; track++) {
		for (side = 0; side < num_sides; side++) {
			for (sector = 0; sector < num_sectors; sector++) {
				fread(buf, ssize, 1, fd);
				vdisk_update_sector(disk, side, track, sector + 1, ssize, buf);
			}
		}
	}
	fclose(fd);
	return disk;
}

/* VDK header entry meanings taken from the source to PC-Dragon II
 * see http://www.emulator.org.uk/ */
static int vdisk_save_vdk(struct vdisk *disk) {
	unsigned int track, sector, side;
	uint8_t buf[1024];
	FILE *fd;
	if (disk == NULL)
		return -1;
	if (!(fd = fopen(disk->filename, "wb")))
		return -1;
	LOG_DEBUG(2,"Writing VDK virtual disk: %dT %dH (%d-byte)\n", disk->num_tracks, disk->num_sides, disk->track_length);
	buf[0] = 'd';   /* magic */
	buf[1] = 'k';   /* magic */
	buf[2] = 12;    /* header size LSB */
	buf[3] = 0;     /* header size MSB */
	buf[4] = 0x10;  /* VDK version */
	buf[5] = 0x10;  /* VDK backwards compatibility version */
	buf[6] = 'X';	/* file source - 'X' for XRoar */
	buf[7] = 0;	/* version of file source */
	buf[8] = disk->num_tracks;
	buf[9] = disk->num_sides;
	buf[10] = 0;    /* flags */
	buf[11] = 0;    /* name length & compression flag (we write neither) */
	fwrite(buf, 12, 1, fd);
	for (track = 0; track < disk->num_tracks; track++) {
		for (side = 0; side < disk->num_sides; side++) {
			for (sector = 0; sector < 18; sector++) {
				vdisk_fetch_sector(disk, side, track, sector + 1, 256, buf);
				fwrite(buf, 256, 1, fd);
			}
		}
	}
	fclose(fd);
	return 0;
}

static struct vdisk *vdisk_load_jvc(const char *filename) {
	struct vdisk *disk;
	ssize_t file_size;
	unsigned int header_size;
	unsigned int num_tracks;
	unsigned int num_sides = 1;
	unsigned int num_sectors = 18;
	unsigned int ssize_code = 1, ssize;
	unsigned int first_sector = 1;
	unsigned int sector_attr = 0;
	unsigned int track, sector, side;
	uint8_t buf[1024];
	FILE *fd;
	struct stat statbuf;
	if (stat(filename, &statbuf) != 0)
		return NULL;
	file_size = statbuf.st_size;
	header_size = file_size % 256;
	file_size -= header_size;
	/* Supposedly, we are supposed to default to single sided if there's
	 * no header information overriding, but I found double sided
	 * headerless DSK files. */
	if (file_size > 198144) num_sides = 2;
	if (!(fd = fopen(filename, "rb")))
		return NULL;
	if (header_size > 0) {
		fread(buf, header_size, 1, fd);
		num_sectors = buf[0];
	}
	if (header_size > 1) num_sides = buf[1];
	if (header_size > 2) ssize_code = buf[2] & 3;
	ssize = 128 << ssize_code;
	if (header_size > 3) first_sector = buf[3];
	if (header_size > 4) sector_attr = buf[4] ? 1 : 0;
	int bytes_per_sector = ssize + sector_attr;
	int bytes_per_track = num_sectors * bytes_per_sector * num_sides;
	num_tracks = file_size / bytes_per_track;
	/* if there is at least one more sector of data, allow an extra track */
	if ((file_size % bytes_per_track) >= bytes_per_sector) {
		num_tracks++;
	}
	disk = vdisk_blank_disk(num_sides, num_tracks, VDISK_LENGTH_5_25);
	if (disk == NULL) {
		fclose(fd);
		return NULL;
	}
	disk->filetype = FILETYPE_JVC;
	disk->filename = g_strdup(filename);
	if (vdisk_format_disk(disk, VDISK_DOUBLE_DENSITY, num_sectors, first_sector, ssize_code) < 0) {
		fclose(fd);
		vdisk_destroy(disk);
		return NULL;
	}
	LOG_DEBUG(2,"Loading JVC virtual disk: %dT %dH %dS (%d-byte)\n", num_tracks, num_sides, num_sectors, ssize);
	for (track = 0; track < num_tracks; track++) {
		for (side = 0; side < num_sides; side++) {
			for (sector = 0; sector < num_sectors; sector++) {
				if (sector_attr) {
					/* skip attribute byte */
					(void)fs_read_uint8(fd);
				}
				if (fread(buf, ssize, 1, fd) < 1) {
					memset(buf, 0, ssize);
				}
				vdisk_update_sector(disk, side, track, sector + first_sector, ssize, buf);
			}
		}
	}
	fclose(fd);
	return disk;
}

static int vdisk_save_jvc(struct vdisk *disk) {
	unsigned int track, sector, side;
	uint8_t buf[1024];
	FILE *fd;
	if (disk == NULL)
		return -1;
	if (!(fd = fopen(disk->filename, "wb")))
		return -1;
	LOG_DEBUG(2,"Writing JVC virtual disk: %dT %dH (%d-byte)\n", disk->num_tracks, disk->num_sides, disk->track_length);
	/* XXX: assume 18 tracks per sector */
	fs_write_uint8(fd, 18);
	fs_write_uint8(fd, disk->num_sides);
	fs_write_uint8(fd, 1);  /* size code = 1 (XXX: assume 256 bytes) */
	fs_write_uint8(fd, 1);  /* first sector = 1 */
	for (track = 0; track < disk->num_tracks; track++) {
		for (side = 0; side < disk->num_sides; side++) {
			for (sector = 0; sector < 18; sector++) {
				vdisk_fetch_sector(disk, side, track, sector + 1, 256, buf);
				fwrite(buf, 256, 1, fd);
			}
		}
	}
	fclose(fd);
	return 0;
}

/* XRoar extension to DMK: byte 11 of header contains actual virtual
 * disk write protect status, as value in header[0] is interpreted as the
 * write protect status of the file. */

static struct vdisk *vdisk_load_dmk(const char *filename) {
	struct vdisk *disk;
	uint8_t header[16];
	ssize_t file_size;
	unsigned int num_sides;
	unsigned int num_tracks;
	unsigned int track_length;
	unsigned int track, side;
	FILE *fd;
	struct stat statbuf;
	if (stat(filename, &statbuf) != 0)
		return NULL;
	file_size = statbuf.st_size;
	if (!(fd = fopen(filename, "rb")))
		return NULL;
	fread(header, 16, 1, fd);
	num_tracks = header[1];
	track_length = (header[3] << 8) | header[2];  /* yes, little-endian! */
	num_sides = (header[4] & 0x10) ? 1 : 2;
	if (header[4] & 0x40)
		LOG_WARN("DMK is flagged single-density only\n");
	if (header[4] & 0x80)
		LOG_WARN("DMK is flagged density-agnostic\n");
	file_size -= 16;
	(void)file_size;  /* check this matches what's going to be read */
	disk = vdisk_blank_disk(num_sides, num_tracks, VDISK_LENGTH_5_25);
	if (disk == NULL) {
		fclose(fd);
		return NULL;
	}
	LOG_DEBUG(2,"Loading DMK virtual disk: %dT %dH (%d-byte)\n", num_tracks, num_sides, track_length);
	disk->filetype = FILETYPE_DMK;
	disk->filename = g_strdup(filename);
	disk->file_write_protect = header[0] ? 1 : 0;
	if (header[11] == 0
			|| header[11] == 0xff) {
		disk->write_protect = header[11] ? 1 : 0;
	} else {
		disk->write_protect = disk->file_write_protect;
	}
	for (track = 0; track < num_tracks; track++) {
		for (side = 0; side < num_sides; side++) {
			uint16_t *idams = vdisk_track_base(disk, side, track);
			uint8_t *buf = (uint8_t *)idams + 128;
			int i;
			if (idams == NULL) continue;
			for (i = 0; i < 64; i++) {
				idams[i] = fs_read_uint16_le(fd);
			}
			fread(buf, track_length - 128, 1, fd);
		}
	}
	fclose(fd);
	return disk;
}

static int vdisk_save_dmk(struct vdisk *disk) {
	uint8_t header[16];
	unsigned int track, side;
	FILE *fd;
	if (disk == NULL)
		return -1;
	if (!(fd = fopen(disk->filename, "wb")))
		return -1;
	LOG_DEBUG(2,"Writing DMK virtual disk: %dT %dH (%d-byte)\n", disk->num_tracks, disk->num_sides, disk->track_length);
	memset(header, 0, sizeof(header));
	if (disk->file_write_protect)
		header[0] = 0xff;
	header[1] = disk->num_tracks;
	header[2] = disk->track_length & 0xff;
	header[3] = (disk->track_length >> 8) & 0xff;
	if (disk->num_sides == 1)
		header[4] |= 0x10;
	header[11] = disk->write_protect ? 0xff : 0;
	fwrite(header, 16, 1, fd);
	for (track = 0; track < disk->num_tracks; track++) {
		for (side = 0; side < disk->num_sides; side++) {
			uint16_t *idams = vdisk_track_base(disk, side, track);
			uint8_t *buf = (uint8_t *)idams + 128;
			int i;
			if (idams == NULL) continue;
			for (i = 0; i < 64; i++) {
				fs_write_uint16_le(fd, idams[i]);
			}
			fwrite(buf, disk->track_length - 128, 1, fd);
		}
	}
	fclose(fd);
	return 0;
}

/* Returns void because track data is manipulated in 8-bit and 16-bit
 * chunks. */
void *vdisk_track_base(struct vdisk *disk, int side, int track) {
	if (disk == NULL
			|| side < 0 || (unsigned)side >= disk->num_sides
			|| track < 0 || (unsigned)track >= disk->num_tracks) {
		return NULL;
	}
	return disk->track_data
		+ ((track * disk->num_sides) + side) * disk->track_length;
}

static unsigned int sect_interleave[18] =
	{ 0, 9, 1, 10, 2, 11, 3, 12, 4, 13, 5, 14, 6, 15, 7, 16, 8, 17 };

#define WRITE_BYTE(v) data[offset++] = (v)

#define WRITE_BYTE_CRC(v) do { \
		WRITE_BYTE(v); \
		crc = crc16_byte(crc, v); \
	} while (0)

#define WRITE_CRC() do { \
		data[offset++] = (crc & 0xff00) >> 8; \
		data[offset++] = crc & 0xff; \
	} while (0)

int vdisk_format_disk(struct vdisk *disk, int density,
		int num_sectors, int first_sector, int ssize_code) {
	int ssize = 128 << ssize_code;
	int side, track, sector, i;
	if (disk == NULL) return -1;
	if (density != VDISK_DOUBLE_DENSITY) return -1;
	for (side = 0; (unsigned)side < disk->num_sides; side++) {
		for (track = 0; (unsigned)track < disk->num_tracks; track++) {
			uint16_t *idams = vdisk_track_base(disk, side, track);
			uint8_t *data = (uint8_t *)idams;
			unsigned int offset = 128;
			unsigned int idam = 0;
			for (i = 0; i < 54; i++) WRITE_BYTE(0x4e);
			for (i = 0; i < 9; i++) WRITE_BYTE(0x00);
			for (i = 0; i < 3; i++) WRITE_BYTE(0xc2);
			WRITE_BYTE(0xfc);
			for (i = 0; i < 32; i++) data[offset++] = 0x4e;
			for (sector = 0; sector < num_sectors; sector++) {
				uint16_t crc = CRC16_RESET;
				for (i = 0; i < 8; i++) data[offset++] = 0x00;
				for (i = 0; i < 3; i++) WRITE_BYTE_CRC(0xa1);
				idams[idam++] = offset | density;
				WRITE_BYTE_CRC(0xfe);
				WRITE_BYTE_CRC(track);
				WRITE_BYTE_CRC(side);
				WRITE_BYTE_CRC(sect_interleave[sector] + first_sector);
				WRITE_BYTE_CRC(ssize_code);
				WRITE_CRC();
				for (i = 0; i < 22; i++) WRITE_BYTE(0x4e);
				for (i = 0; i < 12; i++) WRITE_BYTE(0x00);
				crc = CRC16_RESET;
				for (i = 0; i < 3; i++) WRITE_BYTE_CRC(0xa1);
				WRITE_BYTE_CRC(0xfb);
				for (i = 0; i < ssize; i++)
					WRITE_BYTE_CRC(0xe5);
				WRITE_CRC();
				for (i = 0; i < 24; i++) WRITE_BYTE(0x4e);
			}
			while (offset < disk->track_length) {
				WRITE_BYTE(0x4e);
			}
		}
	}
	return 0;
}

int vdisk_update_sector(struct vdisk *disk, int side, int track,
		int sector, int sector_length, uint8_t *buf) {
	uint8_t *data;
	uint16_t *idams;
	unsigned int offset;
	int ssize, i;
	uint16_t crc;
	if (disk == NULL) return -1;
	idams = vdisk_track_base(disk, side, track);
	if (idams == NULL) return -1;
	data = (uint8_t *)idams;
	for (i = 0; i < 64; i++) {
		offset = idams[i] & 0x3fff;
		if (data[offset + 1] == track && data[offset + 2] == side
				&& data[offset + 3] == sector)
			break;
	}
	if (i >= 64) return -1;
	ssize = 128 << data[offset + 4];
	offset += 7;
	offset += 22;
	for (i = 0; i < 12; i++) WRITE_BYTE(0x00);
	crc = CRC16_RESET;
	for (i = 0; i < 3; i++) WRITE_BYTE_CRC(0xa1);
	WRITE_BYTE_CRC(0xfb);
	for (i = 0; i < sector_length; i++) {
		if (i < ssize)
			WRITE_BYTE_CRC(buf[i]);
	}
	for ( ; i < ssize; i++)
		WRITE_BYTE_CRC(0x00);
	WRITE_CRC();
	WRITE_BYTE(0xfe);
	return 0;
}

#define READ_BYTE() data[offset++]

int vdisk_fetch_sector(struct vdisk *disk, int side, int track,
		int sector, int sector_length, uint8_t *buf) {
	uint8_t *data;
	uint16_t *idams;
	unsigned int offset;
	int ssize, i, d;
	if (disk == NULL) return -1;
	idams = vdisk_track_base(disk, side, track);
	if (idams == NULL) return -1;
	data = (uint8_t *)idams;
	for (i = 0; i < 64; i++) {
		offset = idams[i] & 0x3fff;
		if (data[offset + 1] == track && data[offset + 2] == side
				&& data[offset + 3] == sector)
			break;
	}
	if (i >= 64) {
		memset(buf, 0, sector_length);
		return -1;
	}
	ssize = 128 << data[offset + 4];
	if (ssize > sector_length)
		ssize = sector_length;
	offset += 7;
	/* XXX: CRC ignored for now */
	for (i = 0; i < 43; i++) {
		d = READ_BYTE();
		if (d == 0xfb)
			break;
	}
	if (i >= 43)
		return -1;
	for (i = 0; i < ssize; i++) {
		buf[i] = READ_BYTE();
	}
	return 0;
}
