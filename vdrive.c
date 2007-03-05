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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "crc16.h"
#include "logging.h"
#include "machine.h"
#include "vdrive.h"

#define BYTE_TIME (OSCILLATOR_RATE / 31250)
#define MAX_DRIVES (4)
#define MAX_SIDES (2)
#define MAX_TRACKS (256)

struct drive_data {
	int disk_present;
	int write_protect;
	unsigned int tracks;
	unsigned int track_length;
	unsigned int sides;
	uint8_t *track_data;
	unsigned int current_track;
	unsigned int max_alloc;
};

int vdrive_ready;
int vdrive_tr00;
int vdrive_write_protect;

static struct drive_data drives[MAX_DRIVES], *current_drive;
static int direction;
static unsigned int cur_side;
static unsigned int density;
static uint8_t *track_base;  /* updated to point to base addr of cur track */
static uint16_t *idamptr;  /* likewise, but different data size */
static unsigned int head_pos;  /* index into current track for read/write */
static unsigned int index_pulse;

static void update_signals(void);

void vdrive_init(void) {
	int i;
	for (i = 0; i < MAX_DRIVES; i++) {
		drives[i].disk_present = 0;
		drives[i].track_length = VDRIVE_LENGTH_5_25;
		drives[i].max_alloc = 0;
		drives[i].current_track = 0;
		drives[i].tracks = 43;
	}
	vdrive_set_drive(0);
}

/* Allocate space for a new virtual disk within the appropriate drive struct.
 * Disk is kept internally in 'DMK' format minus the header (all useful
 * info in the vdrive struct) which should be exportable to any other
 * format.  More on 'DMK' format here:
 * http://discover-net.net/~dmkeil/coco/cocotech.htm#Technical-DMK-disks
 */

int vdrive_alloc(unsigned int drive, unsigned int num_tracks,
		unsigned int track_length, unsigned int num_sides) {
	unsigned int new_alloc;
	uint8_t *new_data;
	if (drive >= MAX_DRIVES) return -1;
	if (num_tracks < 1 || num_tracks > MAX_TRACKS
			|| track_length < 129 || track_length > 0x2940
			|| num_sides < 1 || num_sides > MAX_SIDES) {
		return -1;
	}
	new_alloc = num_tracks * num_sides * track_length;
	if (drives[drive].track_data == NULL) {
		new_data = malloc(new_alloc);
		if (new_data == NULL) return -1;
		drives[drive].track_data = new_data;
		drives[drive].max_alloc = new_alloc;
	} else if (new_alloc > drives[drive].max_alloc) {
		new_data = realloc(drives[drive].track_data, new_alloc);
		if (new_data == NULL) return -1;
		drives[drive].track_data = new_data;
		drives[drive].max_alloc = new_alloc;
	}
	drives[drive].write_protect = 0;
	drives[drive].tracks = num_tracks;
	drives[drive].track_length = track_length;
	drives[drive].sides = num_sides;
	memset(drives[drive].track_data, 0, drives[drive].max_alloc);
	drives[drive].disk_present = 1;
	return 0;
}

/* Lines from controller sent to all drives */
void vdrive_set_direction(int d) {
	direction = (d > 0) ? 1 : -1;
}

void vdrive_set_side(unsigned int s) {
	if (s >= MAX_SIDES) return;
	cur_side = s;
	update_signals();
}

void vdrive_set_density(unsigned int d) {
	density = d ? VDRIVE_DOUBLE_DENSITY : VDRIVE_SINGLE_DENSITY;
}

static void update_signals(void) {
	assert(current_drive);
	vdrive_ready = current_drive->disk_present;
	if (current_drive->current_track >= current_drive->tracks)
		current_drive->current_track = current_drive->tracks - 1;
	vdrive_tr00 = (current_drive->current_track == 0) ? 1 : 0;
	vdrive_write_protect = current_drive->write_protect;
	if (vdrive_ready && cur_side < current_drive->sides) {
		track_base = current_drive->track_data
			+ (current_drive->current_track * current_drive->sides * current_drive->track_length)
			+ (((current_drive->sides > 1) ? cur_side : 0) * current_drive->track_length);
		idamptr = (uint16_t *)track_base;
	} else {
		track_base = NULL;
		idamptr = NULL;
	}
}

/* Drive select */
void vdrive_set_drive(unsigned int d) {
	if (d >= MAX_DRIVES) return;
	current_drive = &drives[d];
	update_signals();
}

/* Drive-specific actions */
void vdrive_step(void) {
	if (current_drive->disk_present) {
		if (direction > 0 || current_drive->current_track > 0)
			current_drive->current_track += direction;
	}
	update_signals();
}

void vdrive_write(unsigned int data) {
	if (!current_drive->disk_present)
		return;
	track_base[head_pos++] = data & 0xff;
	crc16_byte(data & 0xff);
	if (head_pos >= current_drive->track_length) {
		index_pulse = 1;
		head_pos = 128;
	}
}

void vdrive_skip(void) {
	if (!current_drive->disk_present)
		return;
	head_pos++;
	if (head_pos >= current_drive->track_length) {
		index_pulse = 1;
		head_pos = 128;
	}
}

unsigned int vdrive_read(void) {
	unsigned int ret;
	if (!current_drive->disk_present)
		return 0;
	ret = track_base[head_pos++] & 0xff;
	crc16_byte(ret);
	if (head_pos >= current_drive->track_length) {
		index_pulse = 1;
		head_pos = 128;
	}
	return ret;
}

void vdrive_write_idam(void) {
	unsigned int i;
	for (i = 0; i < 64; i++) {
		if ((idamptr[i] & 0x3fff) < 128) {
			idamptr[i] = head_pos | density;
			vdrive_write(0xfe);
			return;
		}
	}
	LOG_WARN("Too many IDAMs\n");
}

/* Calculates the number of cycles it would take to get from the current
 * head position to the end of the next IDAM or the next index hole,
 * whichever comes first. */
unsigned int vdrive_time_to_next_idam(void) {
	unsigned int next_offset;
	unsigned int i, tmp;
	if (!current_drive->disk_present) {
		return (OSCILLATOR_RATE / 5);
	}
	next_offset = current_drive->track_length;
	for (i = 0; i < 64; i++) {
		tmp = idamptr[i] & 0x3fff;
		if (head_pos < tmp && tmp < next_offset)
			next_offset = tmp + 7;
	}
	if (next_offset > current_drive->track_length)
		next_offset = current_drive->track_length;
	return BYTE_TIME * (next_offset - head_pos);
}

/* Returns a pointer to the next IDAM.  Sets index_pulse if appropriate.
 * Updates head_pos to point to just after IDAM.  If no valid IDAMs are
 * present, an index pulse is generated and the head left at the beginning
 * of the track. */
uint8_t *vdrive_next_idam(void) {
	unsigned int next_offset;
	unsigned int i, tmp;
	if (!current_drive->disk_present) {
		index_pulse = 1;
		return NULL;
	}
	next_offset = current_drive->track_length;
	for (i = 0; i < 64; i++) {
		tmp = idamptr[i] & 0x3fff;
		if (head_pos < tmp && tmp < next_offset)
			next_offset = tmp;
	}
	if ((next_offset + 7) >= current_drive->track_length) {
		index_pulse = 1;
		head_pos = 128;
		return NULL;
	}
	head_pos = next_offset + 7;
	crc16_reset();
	crc16_block(track_base + next_offset/* + 1*/, 5);
	return track_base + next_offset;
}

/* Samples index_pulse, clearing it if set */
int vdrive_index_pulse(void) {
	if (index_pulse) {
		index_pulse = 0;
		return 1;
	}
	return 0;
}

static unsigned int sect_interleave[18] =
	{ 0, 9, 1, 10, 2, 11, 3, 12, 4, 13, 5, 14, 6, 15, 7, 16, 8, 17 };

int vdrive_format_disk(unsigned int drive, unsigned int num_tracks,
		unsigned int num_sides, unsigned int num_sectors,
		unsigned int ssize_code, unsigned int first_sector) {
	unsigned int ssize = 128 << ssize_code;
	unsigned int track, sector, side, i;
	if (vdrive_alloc(drive, num_tracks, VDRIVE_LENGTH_5_25, num_sides) < 0)
		return -1;
	vdrive_set_drive(drive);
	vdrive_set_density(VDRIVE_DOUBLE_DENSITY);
	vdrive_set_direction(-1);
	while (!vdrive_tr00) vdrive_step();
	vdrive_set_direction(1);
	for (track = 0; track < num_tracks; track++) {
		for (side = 0; side < num_sides; side++) {
			vdrive_set_side(side);
			do {
				(void)vdrive_next_idam();
			} while (!vdrive_index_pulse());
			for (i = 0; i < 54; i++) vdrive_write(0x4e);
			for (i = 0; i < 9; i++) vdrive_write(0x00);
			for (i = 0; i < 3; i++) vdrive_write(0xc2);
			vdrive_write(0xfc);
			for (i = 0; i < 32; i++) vdrive_write(0x4e);
			for (sector = 0; sector < num_sectors; sector++) {
				for (i = 0; i < 8; i++) vdrive_write(0x00);
				for (i = 0; i < 3; i++) vdrive_write(0xa1);
				crc16_reset();
				vdrive_write_idam();
				vdrive_write(track);
				vdrive_write(side);
				vdrive_write(sect_interleave[sector] + first_sector);
				vdrive_write(ssize_code);
				VDRIVE_WRITE_CRC16;
				for (i = 0; i < 22; i++) vdrive_write(0x4e);
				for (i = 0; i < 12; i++) vdrive_write(0x00);
				for (i = 0; i < 3; i++) vdrive_write(0xa1);
				crc16_reset();
				vdrive_write(0xfb);
				for (i = 0; i < ssize; i++) {
					vdrive_write(0xe5);
				}
				VDRIVE_WRITE_CRC16;
				for (i = 0; i < 24; i++) vdrive_write(0x4e);
			}
			while (!vdrive_index_pulse()) {
				vdrive_write(0x4e);
			}
		}
		vdrive_step();
	}
	return 0;
}

void vdrive_update_sector(unsigned int drive, unsigned int track,
		unsigned int side, unsigned int sector,
		unsigned int ssize_code, uint8_t *data) {
	unsigned int ssize = 128 << ssize_code;
	unsigned int i;
	uint8_t *idam = NULL;
	vdrive_set_drive(drive);
	vdrive_set_density(VDRIVE_DOUBLE_DENSITY);
	vdrive_set_side(side);
	current_drive->current_track = track;
	update_signals();
	do {
		(void)vdrive_next_idam();
	} while (!vdrive_index_pulse());
	while (!vdrive_index_pulse()) {
		idam = vdrive_next_idam();
		if (idam == NULL) continue;
		if (track == idam[1] && side == idam[2] && sector == idam[3]) {
			for (i = 0; i < 22; i++) vdrive_skip();
			for (i = 0; i < 12; i++) vdrive_write(0x00);
			crc16_reset();
			vdrive_write(0xfb);
			for (i = 0; i < ssize; i++) {
				vdrive_write(data[i]);
			}
			VDRIVE_WRITE_CRC16;
			vdrive_write(0xfe);
			return;
		}
	}
}
