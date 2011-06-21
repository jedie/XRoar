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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "crc16.h"
#include "events.h"
#include "logging.h"
#include "machine.h"
#include "vdisk.h"
#include "vdrive.h"
#include "xroar.h"

#define BYTE_TIME (OSCILLATOR_RATE / 31250)
#define MAX_DRIVES VDRIVE_MAX_DRIVES
#define MAX_SIDES (2)
#define MAX_TRACKS (256)

struct drive_data {
	int disk_present;
	struct vdisk *disk;
	unsigned int current_track;
};

int vdrive_ready;
int vdrive_tr00;
int vdrive_index_pulse;
int vdrive_write_protect;
void (*vdrive_update_drive_cyl_head)(int drive, int cyl, int head) = NULL;

static struct drive_data drives[MAX_DRIVES], *current_drive;
static int cur_direction;
static int cur_drive_number;
static unsigned int cur_side;
static unsigned int cur_density;
static int head_incr;  /* bytes per write - 2 in SD, 1 in DD */
static uint8_t *track_base;  /* updated to point to base addr of cur track */
static uint16_t *idamptr;  /* likewise, but different data size */
static unsigned int head_pos;  /* index into current track for read/write */

static void update_signals(void);

static Cycle last_update_cycle;
static Cycle track_start_cycle;
static event_t index_pulse_event;
static event_t reset_index_pulse_event;
static void do_index_pulse(void);
static void do_reset_index_pulse(void);

void vdrive_init(void) {
	int i;
	for (i = 0; i < MAX_DRIVES; i++) {
		drives[i].disk_present = 0;
		drives[i].disk = NULL;
		drives[i].current_track = 0;
	}
	vdrive_set_density(VDISK_DOUBLE_DENSITY);
	vdrive_set_drive(0);
	event_init(&index_pulse_event);
	index_pulse_event.dispatch = do_index_pulse;
	event_init(&reset_index_pulse_event);
	reset_index_pulse_event.dispatch = do_reset_index_pulse;
}

void vdrive_shutdown(void) {
	int i;
	for (i = 0; i < MAX_DRIVES; i++) {
		if (drives[i].disk != NULL && drives[i].disk_present) {
			vdrive_eject_disk(i);
		}
	}
}

int vdrive_insert_disk(int drive, struct vdisk *disk) {
	if (drive < 0 || drive >= MAX_DRIVES)
		return -1;
	if (drives[drive].disk_present) {
		vdrive_eject_disk(drive);
	}
	if (disk == NULL)
		return -1;
	drives[drive].disk = disk;
	drives[drive].disk_present = 1;
	update_signals();
	return 0;
}

int vdrive_eject_disk(int drive) {
	if (drive < 0 || drive >= MAX_DRIVES)
		return -1;
	if (!drives[drive].disk_present)
		return -1;
	vdisk_save(drives[drive].disk, 0);
	vdisk_destroy(drives[drive].disk);
	drives[drive].disk_present = 0;
	update_signals();
	return 0;
}

struct vdisk *vdrive_disk_in_drive(int drive) {
	if (drive < 0 || drive >= MAX_DRIVES)
		return NULL;
	if (!drives[drive].disk_present)
		return NULL;
	return drives[drive].disk;
}

int vdrive_set_write_enable(int drive, int action) {
	struct vdisk *disk = vdrive_disk_in_drive(drive);
	if (!disk) return -1;
	int new_we = (disk->write_protect == VDISK_WRITE_ENABLE);
	if (action < 0) {
		new_we = !new_we;
	} else {
		new_we = action ? 1 : 0;
	}
	disk->write_protect = new_we ? VDISK_WRITE_ENABLE : VDISK_WRITE_PROTECT;
	return new_we;
}

int vdrive_set_write_back(int drive, int action) {
	struct vdisk *disk = vdrive_disk_in_drive(drive);
	if (!disk) return -1;
	int new_wb = (disk->file_write_protect == VDISK_WRITE_ENABLE);
	if (action < 0) {
		new_wb = !new_wb;
	} else {
		new_wb = action ? 1 : 0;
	}
	disk->file_write_protect = new_wb ? VDISK_WRITE_ENABLE : VDISK_WRITE_PROTECT;
	return new_wb;
}

unsigned int vdrive_head_pos(void) {
	return head_pos;
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
	if (density != VDISK_SINGLE_DENSITY
			&& density != VDISK_DOUBLE_DENSITY)
		return;
	cur_density = density;
	head_incr = (density == VDISK_SINGLE_DENSITY) ? 2 : 1;
}

static void update_signals(void) {
	assert(current_drive);
	vdrive_ready = current_drive->disk_present;
	vdrive_tr00 = (current_drive->current_track == 0) ? 1 : 0;
	if (vdrive_update_drive_cyl_head) {
		vdrive_update_drive_cyl_head(cur_drive_number, current_drive->current_track, cur_side);
	}
	if (!vdrive_ready) {
		vdrive_write_protect = VDISK_WRITE_ENABLE;
		track_base = NULL;
		idamptr = NULL;
		return;
	}
	vdrive_write_protect = current_drive->disk->write_protect;
	if (cur_side < current_drive->disk->num_sides) {
		idamptr = vdisk_track_base(current_drive->disk, cur_side, current_drive->current_track);
	} else {
		idamptr = NULL;
	}
	track_base = (uint8_t *)idamptr;
	if (!index_pulse_event.queued) {
		head_pos = 128;
		track_start_cycle = current_cycle;
		index_pulse_event.at_cycle = track_start_cycle + (current_drive->disk->track_length - 128) * BYTE_TIME;
		event_queue(&MACHINE_EVENT_LIST, &index_pulse_event);
	}
}

/* Drive select */
void vdrive_set_drive(int drive) {
	if (drive < 0 || drive >= MAX_DRIVES) return;
	cur_drive_number = drive;
	current_drive = &drives[drive];
	update_signals();
}

/* Drive-specific actions */
void vdrive_step(void) {
	if (vdrive_ready) {
		if (cur_direction > 0 || current_drive->current_track > 0)
			current_drive->current_track += cur_direction;
		if (current_drive->current_track >= MAX_TRACKS)
			current_drive->current_track = MAX_TRACKS - 1;
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
		if (track_base && head_pos < current_drive->disk->track_length) {
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
	if (head_pos >= current_drive->disk->track_length) {
		vdrive_index_pulse = 1;
	}
}

void vdrive_skip(void) {
	if (!vdrive_ready) return;
	head_pos += head_incr;
	if (head_pos >= current_drive->disk->track_length) {
		vdrive_index_pulse = 1;
	}
}

unsigned int vdrive_read(void) {
	unsigned int ret = 0;
	if (!vdrive_ready) return 0;
	if (track_base && head_pos < current_drive->disk->track_length) {
		ret = track_base[head_pos] & 0xff;
	}
	head_pos += head_incr;
	crc16_byte(ret);
	if (head_pos >= current_drive->disk->track_length) {
		vdrive_index_pulse = 1;
	}
	return ret;
}

#define IDAM(i) ((unsigned)(idamptr[i] & 0x3fff))

void vdrive_write_idam(void) {
	int i;
	if (track_base && (head_pos+head_incr) < current_drive->disk->track_length) {
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
	if (head_pos >= current_drive->disk->track_length) {
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
	next_head_pos = current_drive->disk->track_length;
	if (idamptr) {
		for (i = 0; i < 64; i++) {
			if ((unsigned)(idamptr[i] & 0x8000) == cur_density) {
				tmp = idamptr[i] & 0x3fff;
				if (head_pos < tmp && tmp < next_head_pos)
					next_head_pos = tmp;
			}
		}
	}
	if (next_head_pos >= current_drive->disk->track_length)
		return (index_pulse_event.at_cycle - current_cycle) + 1;
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
	next_head_pos = current_drive->disk->track_length;
	if (idamptr) {
		for (i = 0; i < 64; i++) {
			if ((unsigned)(idamptr[i] & 0x8000) == cur_density) {
				tmp = idamptr[i] & 0x3fff;
				if (head_pos < tmp && tmp < next_head_pos)
					next_head_pos = tmp;
			}
		}
	}
	if (next_head_pos >= current_drive->disk->track_length) {
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

static void do_index_pulse(void) {
	if (!vdrive_ready) {
		vdrive_index_pulse = 0;
		return;
	}
	vdrive_index_pulse = 1;
	head_pos = 128;
	last_update_cycle = index_pulse_event.at_cycle;
	track_start_cycle = index_pulse_event.at_cycle;
	index_pulse_event.at_cycle = track_start_cycle + (current_drive->disk->track_length - 128) * BYTE_TIME;
	event_queue(&MACHINE_EVENT_LIST, &index_pulse_event);
	reset_index_pulse_event.at_cycle = track_start_cycle + ((current_drive->disk->track_length - 128)/100) * BYTE_TIME;
	event_queue(&MACHINE_EVENT_LIST, &reset_index_pulse_event);
}

static void do_reset_index_pulse(void) {
	vdrive_index_pulse = 0;
	/* reset latch */
	(void)vdrive_new_index_pulse();
}
