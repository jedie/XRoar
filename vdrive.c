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
#include "events.h"
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
	unsigned int num_sides;
	unsigned int num_tracks;
	unsigned int track_length;
	uint8_t *track_data;
	unsigned int current_track;
	unsigned int max_alloc;
};

int vdrive_ready;
int vdrive_tr00;
int vdrive_index_pulse;
int vdrive_write_protect;

static struct drive_data drives[MAX_DRIVES], *current_drive;
static int cur_direction;
static unsigned int cur_side;
static unsigned int cur_density;
static int head_incr;  /* bytes per write - 2 in SD, 1 in DD */
static uint8_t *track_base;  /* updated to point to base addr of cur track */
static uint16_t *idamptr;  /* likewise, but different data size */
static unsigned int head_pos;  /* index into current track for read/write */

static void update_signals(void);

static Cycle last_update_cycle;
static Cycle track_start_cycle;
static event_t *index_pulse_event;
static event_t *reset_index_pulse_event;
static void do_index_pulse(void *context);
static void do_reset_index_pulse(void *context);

void vdrive_init(void) {
	int i;
	for (i = 0; i < MAX_DRIVES; i++) {
		drives[i].disk_present = 0;
		drives[i].track_length = VDRIVE_LENGTH_5_25;
		drives[i].max_alloc = 0;
		drives[i].current_track = 0;
		drives[i].num_tracks = 43;
	}
	vdrive_set_density(VDRIVE_DOUBLE_DENSITY);
	vdrive_set_drive(0);
	index_pulse_event = event_new();
	index_pulse_event->dispatch = do_index_pulse;
	reset_index_pulse_event = event_new();
	reset_index_pulse_event->dispatch = do_reset_index_pulse;
	track_start_cycle = current_cycle;
	index_pulse_event->at_cycle = track_start_cycle + (current_drive->track_length - 128) * BYTE_TIME;
	event_queue(index_pulse_event);
}

static void do_index_pulse(void *context) {
	(void)context;
	if (vdrive_ready) {
		vdrive_index_pulse = 1;
		head_pos = 128;
		last_update_cycle = index_pulse_event->at_cycle;
	}
	track_start_cycle = index_pulse_event->at_cycle;
	index_pulse_event->at_cycle = track_start_cycle + (current_drive->track_length - 128) * BYTE_TIME;
	event_queue(index_pulse_event);
	reset_index_pulse_event->at_cycle = track_start_cycle + ((current_drive->track_length - 128)/100) * BYTE_TIME;
	event_queue(reset_index_pulse_event);
}

static void do_reset_index_pulse(void *context) {
	(void)context;
	vdrive_index_pulse = 0;
	/* reset latch */
	(void)vdrive_new_index_pulse();
}

unsigned int vdrive_head_pos(void) {
	return head_pos;
}

/* Allocate space for a new virtual disk within the appropriate drive struct.
 * Disk is kept internally in 'DMK' format minus the header (all useful
 * info in the vdrive struct) which should be exportable to any other
 * format.  More on 'DMK' format here:
 * http://discover-net.net/~dmkeil/coco/cocotech.htm#Technical-DMK-disks
 */

int vdrive_blank_disk(int drive, int num_sides,
		int num_tracks, int track_length) {
	unsigned int new_alloc;
	uint8_t *new_data;
	if (drive < 0 || drive >= MAX_DRIVES
			|| num_sides < 1 || num_sides > MAX_SIDES
			|| num_tracks < 1 || num_tracks > MAX_TRACKS
			|| track_length < 129 || track_length > 0x2940) {
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
	if (drives[drive].track_data == NULL) return -1;
	drives[drive].write_protect = 0;
	drives[drive].num_sides = num_sides;
	drives[drive].num_tracks = num_tracks;
	drives[drive].track_length = track_length;
	memset(drives[drive].track_data, 0, drives[drive].max_alloc);
	drives[drive].disk_present = 1;
	update_signals();
	return 0;
}

/* Return pointer to current drive's raw data area - for loading DMK images */
uint8_t *vdrive_track_data(int drive) {
	if (drive < 0 || drive >= MAX_DRIVES) return NULL;
	if (!drives[drive].disk_present) return NULL;
	return drives[drive].track_data;
}

void vdrive_set_write_protect(int drive, int write_protect) {
	if (drive < 0 || drive >= MAX_DRIVES) return;
	if (!drives[drive].disk_present) return;
	current_drive->write_protect = write_protect ? VDRIVE_WRITE_PROTECT : VDRIVE_WRITE_ENABLE;
	update_signals();
}

/* Lines from controller sent to all drives */
void vdrive_set_direction(int direction) {
	cur_direction = (direction > 0) ? 1 : -1;
}

void vdrive_set_side(int side) {
	if (side < 0 || side >= MAX_SIDES) return;
	cur_side = side;
	update_signals();
}

void vdrive_set_density(int density) {
	if (density != VDRIVE_SINGLE_DENSITY
			&& density != VDRIVE_DOUBLE_DENSITY)
		return;
	cur_density = density;
	head_incr = (density == VDRIVE_SINGLE_DENSITY) ? 2 : 1;
}

static void update_signals(void) {
	assert(current_drive);
	vdrive_ready = current_drive->disk_present;
	if (current_drive->current_track >= current_drive->num_tracks)
		current_drive->current_track = current_drive->num_tracks - 1;
	vdrive_tr00 = (current_drive->current_track == 0) ? 1 : 0;
	vdrive_write_protect = current_drive->write_protect;
	if (vdrive_ready && cur_side < current_drive->num_sides) {
		track_base = current_drive->track_data
			+ (current_drive->current_track * current_drive->num_sides * current_drive->track_length)
			+ (((current_drive->num_sides > 1) ? cur_side : 0) * current_drive->track_length);
		idamptr = (uint16_t *)track_base;
	} else {
		track_base = NULL;
		idamptr = NULL;
	}
}

/* Drive select */
void vdrive_set_drive(int drive) {
	if (drive < 0 || drive >= MAX_DRIVES) return;
	current_drive = &drives[drive];
	update_signals();
}

/* Drive-specific actions */
void vdrive_step(void) {
	if (vdrive_ready) {
		if (cur_direction > 0 || current_drive->current_track > 0)
			current_drive->current_track += cur_direction;
	}
	update_signals();
}

/* Compare IDAM pointers - normal int comparison with 0 being a special case
 * to come after everything else */
static int compar_idams(const void *aa, const void *bb) {
	uint16_t a = *((const uint16_t *)aa) & 0x3fff;
	uint16_t b = *((const uint16_t *)bb) & 0x3fff;
	if (a == b) return 0;
	if (a == 0) return 1;
	if (b == 0) return -1;
	if (a < b) return -1;
	return 1;
}

void vdrive_write(unsigned int data) {
	int i;
	if (!vdrive_ready) return;
	data &= 0xff;
	for (i = head_incr; i; i--) {
		int j;
		if (head_pos < current_drive->track_length) {
			track_base[head_pos] = data;
			for (j = 0; j < 64; j++) {
				if (head_pos == (unsigned)(idamptr[j] & 0x3fff)) {
					idamptr[j] = 0;
					qsort(idamptr, 64, sizeof(uint16_t), compar_idams);
				}
			}
		}
		head_pos++;
	}
	crc16_byte(data);
	if (head_pos >= current_drive->track_length) {
		vdrive_index_pulse = 1;
	}
}

void vdrive_skip(void) {
	if (!vdrive_ready) return;
	head_pos += head_incr;
	if (head_pos >= current_drive->track_length) {
		vdrive_index_pulse = 1;
	}
}

unsigned int vdrive_read(void) {
	unsigned int ret = 0;
	if (!vdrive_ready) return 0;
	if (head_pos < current_drive->track_length) {
		ret = track_base[head_pos] & 0xff;
	}
	head_pos += head_incr;
	crc16_byte(ret);
	if (head_pos >= current_drive->track_length) {
		vdrive_index_pulse = 1;
	}
	return ret;
}

#define IDAM(i) ((unsigned)(idamptr[i] & 0x3fff))

void vdrive_write_idam(void) {
	int i;
	if ((head_pos+head_incr) < current_drive->track_length) {
		/* Write 0xfe and remove old IDAM ptr if it exists */
		for (i = 0; i < 64; i++) {
			int j;
			for (j = 0; j < head_incr; j++) {
				track_base[head_pos + j] = 0xfe;
				if ((head_pos + j) == IDAM(j)) {
					idamptr[i] = 0;
				}
			}
		}
		/* Add to end of idam list and sort */
		idamptr[63] = head_pos | cur_density;
		qsort(idamptr, 64, sizeof(uint16_t), compar_idams);
	}
	head_pos += head_incr;
	crc16_byte(0xfe);
	if (head_pos >= current_drive->track_length) {
		vdrive_index_pulse = 1;
	}
}

unsigned int vdrive_time_to_next_byte(void) {
	Cycle next_cycle = track_start_cycle + (head_pos - 128) * BYTE_TIME;
	int to_time = next_cycle - current_cycle;
	if (to_time < 0) {
		LOG_DEBUG(4,"Negative time to next byte!\n");
		return 1;
	}
	return to_time + 1;
}

/* Calculates the number of cycles it would take to get from the current head
 * position to the next IDAM or next index pulse, whichever comes first. */

unsigned int vdrive_time_to_next_idam(void) {
	unsigned int next_head_pos;
	unsigned int i, tmp;
	Cycle next_cycle;
	int to_time;
	if (!vdrive_ready) return OSCILLATOR_RATE / 5;
	/* Update head_pos based on time elapsed since track start */
	head_pos = 128 + ((current_cycle - track_start_cycle) / BYTE_TIME);
	(void)vdrive_new_index_pulse();
	next_head_pos = current_drive->track_length;
	for (i = 0; i < 64; i++) {
		if ((unsigned)(idamptr[i] & 0x8000) == cur_density) {
			tmp = idamptr[i] & 0x3fff;
			if (head_pos < tmp && tmp < next_head_pos)
				next_head_pos = tmp;
		}
	}
	if (next_head_pos >= current_drive->track_length)
		return (index_pulse_event->at_cycle - current_cycle) + 1;
	next_cycle = track_start_cycle + (next_head_pos - 128) * BYTE_TIME;
	to_time = next_cycle - current_cycle;
	if (to_time < 0) {
		LOG_DEBUG(4,"Negative time to next IDAM!\n");
		return 1;
	}
	return to_time + 1;
}

/* Updates head_pos to next IDAM and returns a pointer to it.  If no valid
 * IDAMs are present, an index pulse is generated and the head left at the
 * beginning of the track. */

uint8_t *vdrive_next_idam(void) {
	unsigned int next_head_pos;
	unsigned int i, tmp;
	if (!vdrive_ready) return NULL;
	next_head_pos = current_drive->track_length;
	for (i = 0; i < 64; i++) {
		if ((unsigned)(idamptr[i] & 0x8000) == cur_density) {
			tmp = idamptr[i] & 0x3fff;
			if (head_pos < tmp && tmp < next_head_pos)
				next_head_pos = tmp;
		}
	}
	if (next_head_pos >= current_drive->track_length) {
		vdrive_index_pulse = 1;
		return NULL;
	}
	head_pos = next_head_pos;
	return track_base + next_head_pos;
}

/* Returns 1 on active transition of index pulse */
int vdrive_new_index_pulse(void) {
	static int last_index_pulse = 0;
	int last = last_index_pulse;
	last_index_pulse = vdrive_index_pulse;
	if (!last && vdrive_index_pulse)
		return 1;
	return 0;
}

static unsigned int sect_interleave[18] =
	{ 0, 9, 1, 10, 2, 11, 3, 12, 4, 13, 5, 14, 6, 15, 7, 16, 8, 17 };

static void format_track_dd(int side, int track,
		int num_sectors, int first_sector, int ssize_code) {
	int ssize = 128 << ssize_code;
	int sector;
	int i;
	head_pos = 128;
	for (i = 0; i < 54; i++) vdrive_write(0x4e);
	for (i = 0; i < 9; i++) vdrive_write(0x00);
	for (i = 0; i < 3; i++) vdrive_write(0xc2);
	vdrive_write(0xfc);
	for (i = 0; i < 32; i++) vdrive_write(0x4e);
	for (sector = 0; sector < num_sectors; sector++) {
		for (i = 0; i < 8; i++) vdrive_write(0x00);
		crc16_reset();
		for (i = 0; i < 3; i++) vdrive_write(0xa1);
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
	while (!vdrive_index_pulse) {
		vdrive_write(0x4e);
	}
}

int vdrive_format_disk(int drive, int density,
		int num_sectors, int first_sector, int ssize_code) {
	unsigned int track, side;
	int old_density;
	if (drive < 0 || drive >= MAX_DRIVES) return -1;
	if (density != VDRIVE_DOUBLE_DENSITY) return -1;
	vdrive_set_drive(drive);
	if (!vdrive_ready) return -1;
	old_density = cur_density;
	vdrive_set_density(density);
	vdrive_set_direction(-1);
	while (!vdrive_tr00) vdrive_step();
	vdrive_set_direction(1);
	for (track = 0; track < current_drive->num_tracks; track++) {
		for (side = 0; side < current_drive->num_sides; side++) {
			vdrive_set_side(side);
			if (density == VDRIVE_DOUBLE_DENSITY)
				format_track_dd(side, track, num_sectors, first_sector, ssize_code);
		}
		vdrive_step();
	}
	vdrive_set_density(old_density);
	return 0;
}

int vdrive_update_sector(int drive, int side, int track,
		int sector, int sector_length, uint8_t *data) {
	unsigned int old_density;
	uint8_t *idam;
	if (drive >= MAX_DRIVES) return -1;
	old_density = cur_density;
	vdrive_set_density(VDRIVE_DOUBLE_DENSITY);
	vdrive_set_drive(drive);
	if (!vdrive_ready) return -1;
	vdrive_set_side(side);
	current_drive->current_track = track;
	update_signals();
	head_pos = 128;
	while ( (idam = vdrive_next_idam()) ) {
		int ssize, i;
		vdrive_skip();
		if ((unsigned)track != vdrive_read()) continue;
		if ((unsigned)side != vdrive_read()) continue;
		if ((unsigned)sector != vdrive_read()) continue;
		ssize = 128 << vdrive_read();
		vdrive_skip();
		vdrive_skip();
		for (i = 0; i < 22; i++) vdrive_skip();
		for (i = 0; i < 12; i++) vdrive_write(0x00);
		crc16_reset();
		if (cur_density == VDRIVE_DOUBLE_DENSITY) {
			crc16_byte(0xa1);
			crc16_byte(0xa1);
			crc16_byte(0xa1);
		}
		vdrive_write(0xfb);
		for (i = 0; i < sector_length; i++) {
			if (i < ssize)
				vdrive_write(data[i]);
		}
		for ( ; i < ssize; i++)
			vdrive_write(0);
		VDRIVE_WRITE_CRC16;
		vdrive_write(0xfe);
		vdrive_set_density(old_density);
		return 0;
	}
	vdrive_set_density(old_density);
	return -1;
}
