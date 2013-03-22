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

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "portalib/glib.h"

#include "crc16.h"
#include "fs.h"
#include "logging.h"
#include "machine.h"
#include "module.h"
#include "vdisk.h"
#include "xroar.h"

#define MAX_SIDES (2)
#define MAX_TRACKS (256)

static struct vdisk *vdisk_load_vdk(const char *filename);
static struct vdisk *vdisk_load_jvc(const char *filename);
static struct vdisk *vdisk_load_dmk(const char *filename);
static _Bool vdisk_save_vdk(struct vdisk *disk);
static _Bool vdisk_save_jvc(struct vdisk *disk);
static _Bool vdisk_save_dmk(struct vdisk *disk);

static struct {
	int filetype;
	struct vdisk *(*load_func)(const char *);
	_Bool (*save_func)(struct vdisk *);
} dispatch[] = {
	{ FILETYPE_VDK, vdisk_load_vdk, vdisk_save_vdk },
	{ FILETYPE_JVC, vdisk_load_jvc, vdisk_save_jvc },
	{ FILETYPE_DMK, vdisk_load_dmk, vdisk_save_dmk },
	{ -1, NULL, NULL }
};

struct vdisk *vdisk_blank_disk(unsigned num_sides, unsigned num_tracks,
		unsigned track_length) {
	struct vdisk *disk;
	uint8_t **side_data;
	/* Ensure multiples of track_length will stay 16-bit aligned */
	if ((track_length % 2) != 0)
		track_length++;
	if (num_sides < 1 || num_sides > MAX_SIDES
			|| num_tracks < 1 || num_tracks > MAX_TRACKS
			|| track_length < 129 || track_length > 0x2940) {
		return NULL;
	}
	if (!(disk = g_try_malloc(sizeof(*disk))))
		return NULL;
	if (!(side_data = g_try_malloc0(num_sides * sizeof(*side_data))))
		goto cleanup;
	unsigned side_length = num_tracks * track_length;
	for (unsigned i = 0; i < num_sides; i++) {
		if (!(side_data[i] = g_try_malloc0(side_length)))
			goto cleanup_sides;
	}
	disk->filetype = FILETYPE_DMK;
	disk->filename = NULL;
	disk->file_write_protect = !xroar_opt_disk_write_back;
	disk->write_protect = 0;
	disk->num_sides = num_sides;
	disk->num_tracks = num_tracks;
	disk->track_length = track_length;
	disk->side_data = side_data;
	return disk;
cleanup_sides:
	for (unsigned i = 0; i < num_sides; i++) {
		if (side_data[i])
			g_free(side_data[i]);
	}
	g_free(side_data);
cleanup:
	g_free(disk);
	return NULL;
}

void vdisk_destroy(struct vdisk *disk) {
	if (disk == NULL) return;
	if (disk->filename) {
		g_free(disk->filename);
		disk->filename = NULL;
	}
	for (unsigned i = 0; i < disk->num_sides; i++) {
		if (disk->side_data[i])
			g_free(disk->side_data[i]);
	}
	g_free(disk->side_data);
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

_Bool vdisk_save(struct vdisk *disk, _Bool force) {
	char *backup_filename;
	int i;
	if (disk == NULL) return 0;
	if (!force && disk->file_write_protect) {
		LOG_DEBUG(2, "Not saving disk file: write-back is disabled.\n");
		// This is the requested behaviour, so success:
		return 1;
	}
	if (disk->filename == NULL) {
		disk->filename = filereq_module->save_filename(NULL);
		if (disk->filename == NULL) {
			 LOG_WARN("No filename given: not writing disk file.\n");
			 return 0;
		}
		disk->filetype = xroar_filetype_by_ext(disk->filename);
	}
	for (i = 0; dispatch[i].filetype >= 0 && dispatch[i].filetype != disk->filetype; i++);
	if (dispatch[i].save_func == NULL) {
		LOG_WARN("No writer for virtual disk file type.\n");
		return 0;
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
	unsigned header_size;
	unsigned num_tracks;
	unsigned num_sides = 1;
	unsigned num_sectors = 18;
	unsigned ssize_code = 1, ssize;
	unsigned track, sector, side;
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
	if (!vdisk_format_disk(disk, VDISK_DOUBLE_DENSITY, num_sectors, 1, ssize_code)) {
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
static _Bool vdisk_save_vdk(struct vdisk *disk) {
	unsigned track, sector, side;
	uint8_t buf[1024];
	FILE *fd;
	if (disk == NULL)
		return 0;
	if (!(fd = fopen(disk->filename, "wb")))
		return 0;
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
	return 1;
}

static struct vdisk *vdisk_load_jvc(const char *filename) {
	struct vdisk *disk;
	ssize_t file_size;
	unsigned header_size;
	unsigned num_tracks;
	unsigned num_sides = 1;
	unsigned num_sectors = 18;
	unsigned ssize_code = 1, ssize;
	unsigned first_sector = 1;
	unsigned sector_attr = 0;
	unsigned track, sector, side;
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
	if (!vdisk_format_disk(disk, VDISK_DOUBLE_DENSITY, num_sectors, first_sector, ssize_code)) {
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

static _Bool vdisk_save_jvc(struct vdisk *disk) {
	unsigned track, sector, side;
	uint8_t buf[1024];
	FILE *fd;
	if (disk == NULL)
		return 0;
	if (!(fd = fopen(disk->filename, "wb")))
		return 0;
	LOG_DEBUG(2,"Writing JVC virtual disk: %dT %dH (%d-byte)\n", disk->num_tracks, disk->num_sides, disk->track_length);
	/* XXX: assume 18 tracks per sector */
	if (disk->num_sides != 1) {
		fs_write_uint8(fd, 18);
		fs_write_uint8(fd, disk->num_sides);
		fs_write_uint8(fd, 1);  /* size code = 1 (XXX: assume 256 bytes) */
		fs_write_uint8(fd, 1);  /* first sector = 1 */
	}
	for (track = 0; track < disk->num_tracks; track++) {
		for (side = 0; side < disk->num_sides; side++) {
			for (sector = 0; sector < 18; sector++) {
				vdisk_fetch_sector(disk, side, track, sector + 1, 256, buf);
				fwrite(buf, 256, 1, fd);
			}
		}
	}
	fclose(fd);
	return 1;
}

/* XRoar extension to DMK: byte 11 of header contains actual virtual
 * disk write protect status, as value in header[0] is interpreted as the
 * write protect status of the file. */

static struct vdisk *vdisk_load_dmk(const char *filename) {
	struct vdisk *disk;
	uint8_t header[16];
	ssize_t file_size;
	unsigned num_sides;
	unsigned num_tracks;
	unsigned track_length;
	unsigned track, side;
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
	if (header[11] == 0 || header[11] == 0xff) {
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

static _Bool vdisk_save_dmk(struct vdisk *disk) {
	uint8_t header[16];
	unsigned track, side;
	FILE *fd;
	if (disk == NULL)
		return 0;
	if (!(fd = fopen(disk->filename, "wb")))
		return 0;
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
	return 1;
}

// Returns void because track data is manipulated in 8-bit and 16-bit chunks.
void *vdisk_track_base(struct vdisk *disk, unsigned side, unsigned track) {
	if (disk == NULL || side >= disk->num_sides || track >= disk->num_tracks) {
		return NULL;
	}
	return disk->side_data[side] + track * disk->track_length;
}

// Write routines call this to increase disk size.  Returns same as above.
void *vdisk_extend_disk(struct vdisk *disk, unsigned side, unsigned track) {
	if (!disk)
		return NULL;
	if (track >= MAX_TRACKS)
		return NULL;
	uint8_t **side_data = disk->side_data;
	unsigned nsides = disk->num_sides;
	unsigned ntracks = disk->num_tracks;
	unsigned tlength = disk->track_length;
	if (side >= nsides) {
		nsides = side + 1;
	}
	if (track >= ntracks) {
		// Round amount of tracks up to the next nearest standard size.
		if (IS_COCO) {
			// Standard CoCo disk.
			if (ntracks < 35 && track >= ntracks)
				ntracks = 35;
		}
		// Try for exactly 40 or 80 track, but allow 3 tracks more.
		if (ntracks < 40 && track >= ntracks)
			ntracks = 40;
		if (ntracks < 43 && track >= ntracks)
			ntracks = 43;
		if (ntracks < 80 && track >= ntracks)
			ntracks = 80;
		if (ntracks < 83 && track >= ntracks)
			ntracks = 83;
		// If that's still not enough, just increase to new size.
		if (track >= ntracks)
			ntracks = track + 1;
	}
	if (nsides > disk->num_sides || ntracks > disk->num_tracks) {
		if (ntracks > disk->num_tracks) {
			// Allocate and clear new tracks
			for (unsigned s = 0; s < disk->num_sides; s++) {
				uint8_t *new_track = g_realloc(side_data[s], ntracks * tlength);
				if (!new_track)
					return NULL;
				side_data[s] = new_track;
				for (unsigned t = disk->num_tracks; t < ntracks; t++) {
					uint8_t *dest = new_track + t * tlength;
					memset(dest, 0, tlength);
				}
			}
			disk->num_tracks = ntracks;
		}
		if (nsides > disk->num_sides) {
			// Increase size of side_data array
			side_data = g_realloc(side_data, nsides * sizeof(*side_data));
			if (!side_data)
				return NULL;
			disk->side_data = side_data;
			// Allocate new empty side data
			for (unsigned s = disk->num_sides; s < nsides; s++) {
				side_data[s] = g_try_malloc0(ntracks * tlength);
				if (!side_data[s])
					return NULL;
			}
			disk->num_sides = nsides;
		}
	}
	return side_data[side] + track * tlength;
}

static unsigned sect_interleave[18] =
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

_Bool vdisk_format_disk(struct vdisk *disk, unsigned density,
		unsigned num_sectors, unsigned first_sector, unsigned ssize_code) {
	unsigned ssize = 128 << ssize_code;
	if (disk == NULL) return 0;
	if (density != VDISK_DOUBLE_DENSITY)
		return 0;
	for (unsigned side = 0; side < disk->num_sides; side++) {
		for (unsigned track = 0; track < disk->num_tracks; track++) {
			uint16_t *idams = vdisk_track_base(disk, side, track);
			uint8_t *data = (uint8_t *)idams;
			unsigned offset = 128;
			unsigned idam = 0;
			for (unsigned i = 0; i < 54; i++) WRITE_BYTE(0x4e);
			for (unsigned i = 0; i < 9; i++) WRITE_BYTE(0x00);
			for (unsigned i = 0; i < 3; i++) WRITE_BYTE(0xc2);
			WRITE_BYTE(0xfc);
			for (unsigned i = 0; i < 32; i++) data[offset++] = 0x4e;
			for (unsigned sector = 0; sector < num_sectors; sector++) {
				uint16_t crc = CRC16_RESET;
				for (unsigned i = 0; i < 8; i++) data[offset++] = 0x00;
				for (unsigned i = 0; i < 3; i++) WRITE_BYTE_CRC(0xa1);
				idams[idam++] = offset | density;
				WRITE_BYTE_CRC(0xfe);
				WRITE_BYTE_CRC(track);
				WRITE_BYTE_CRC(side);
				WRITE_BYTE_CRC(sect_interleave[sector] + first_sector);
				WRITE_BYTE_CRC(ssize_code);
				WRITE_CRC();
				for (unsigned i = 0; i < 22; i++) WRITE_BYTE(0x4e);
				for (unsigned i = 0; i < 12; i++) WRITE_BYTE(0x00);
				crc = CRC16_RESET;
				for (unsigned i = 0; i < 3; i++) WRITE_BYTE_CRC(0xa1);
				WRITE_BYTE_CRC(0xfb);
				for (unsigned i = 0; i < ssize; i++)
					WRITE_BYTE_CRC(0xe5);
				WRITE_CRC();
				for (unsigned i = 0; i < 24; i++) WRITE_BYTE(0x4e);
			}
			while (offset < disk->track_length) {
				WRITE_BYTE(0x4e);
			}
		}
	}
	return 1;
}

_Bool vdisk_update_sector(struct vdisk *disk, unsigned side, unsigned track,
		unsigned sector, unsigned sector_length, uint8_t *buf) {
	uint8_t *data;
	uint16_t *idams;
	unsigned offset;
	unsigned ssize, i;
	uint16_t crc;
	if (disk == NULL) return 0;
	idams = vdisk_track_base(disk, side, track);
	if (idams == NULL) return 0;
	data = (uint8_t *)idams;
	for (i = 0; i < 64; i++) {
		offset = idams[i] & 0x3fff;
		if (data[offset + 1] == track && data[offset + 2] == side
				&& data[offset + 3] == sector)
			break;
	}
	if (i >= 64) return 0;
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
	return 1;
}

#define READ_BYTE() data[offset++]

_Bool vdisk_fetch_sector(struct vdisk *disk, unsigned side, unsigned track,
		unsigned sector, unsigned sector_length, uint8_t *buf) {
	uint8_t *data;
	uint16_t *idams;
	unsigned offset;
	unsigned ssize, i;
	if (disk == NULL) return 0;
	idams = vdisk_track_base(disk, side, track);
	if (idams == NULL) return 0;
	data = (uint8_t *)idams;
	for (i = 0; i < 64; i++) {
		offset = idams[i] & 0x3fff;
		if (data[offset + 1] == track && data[offset + 2] == side
				&& data[offset + 3] == sector)
			break;
	}
	if (i >= 64) {
		memset(buf, 0, sector_length);
		return 0;
	}
	ssize = 128 << data[offset + 4];
	if (ssize > sector_length)
		ssize = sector_length;
	offset += 7;
	/* XXX: CRC ignored for now */
	for (i = 0; i < 43; i++) {
		uint8_t d = READ_BYTE();
		if (d == 0xfb)
			break;
	}
	if (i >= 43)
		return 0;
	for (i = 0; i < ssize; i++) {
		buf[i] = READ_BYTE();
	}
	return 1;
}
