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

#include "config.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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
	_Bool disk_present;
	struct vdisk *disk;
	unsigned current_cyl;
};

_Bool vdrive_ready = 0;
_Bool vdrive_tr00 = 1;
_Bool vdrive_index_pulse = 0;
_Bool vdrive_write_protect = 0;
void (*vdrive_update_drive_cyl_head)(unsigned drive, unsigned cyl, unsigned head) = NULL;

static struct drive_data drives[MAX_DRIVES];
static struct drive_data *current_drive = &drives[0];
static int cur_direction = 1;
static unsigned cur_drive_number = 0;
static unsigned cur_head = 0;
static unsigned cur_density = VDISK_SINGLE_DENSITY;
static unsigned head_incr = 2;  // bytes per write - 2 in SD, 1 in DD
static uint8_t *track_base = NULL;  // updated to point to base addr of cur track
static uint16_t *idamptr = NULL;  // likewise, but different data size
static unsigned head_pos = 0;  // index into current track for read/write

static void update_signals(void);

static event_ticks last_update_cycle;
static event_ticks track_start_cycle;
static struct event index_pulse_event;
static struct event reset_index_pulse_event;
static void do_index_pulse(void *);
static void do_reset_index_pulse(void *);

void vdrive_init(void) {
	for (unsigned i = 0; i < MAX_DRIVES; i++) {
		drives[i].disk = NULL;
		drives[i].disk_present = 0;
		drives[i].current_cyl = 0;
	}
	vdrive_set_dden(1);
	vdrive_set_drive(0);
	event_init(&index_pulse_event, do_index_pulse, NULL);
	event_init(&reset_index_pulse_event, do_reset_index_pulse, NULL);
}

void vdrive_shutdown(void) {
	for (unsigned i = 0; i < MAX_DRIVES; i++) {
		if (drives[i].disk != NULL && drives[i].disk_present) {
			vdrive_eject_disk(i);
		}
	}
}

void vdrive_insert_disk(unsigned drive, struct vdisk *disk) {
	assert(drive < MAX_DRIVES);
	if (drives[drive].disk_present) {
		vdrive_eject_disk(drive);
	}
	if (disk == NULL)
		return;
	drives[drive].disk = disk;
	drives[drive].disk_present = 1;
	update_signals();
}

void vdrive_eject_disk(unsigned drive) {
	assert(drive < MAX_DRIVES);
	if (!drives[drive].disk_present)
		return;
	vdisk_save(drives[drive].disk, 0);
	vdisk_destroy(drives[drive].disk);
	drives[drive].disk = NULL;
	drives[drive].disk_present = 0;
	update_signals();
}

struct vdisk *vdrive_disk_in_drive(unsigned drive) {
	assert(drive < MAX_DRIVES);
	if (!drives[drive].disk_present)
		return NULL;
	return drives[drive].disk;
}

_Bool vdrive_set_write_enable(unsigned drive, int action) {
	assert(drive < MAX_DRIVES);
	struct vdisk *disk = vdrive_disk_in_drive(drive);
	if (!disk) return -1;
	_Bool new_we = !disk->write_protect;
	if (action < 0) {
		new_we = !new_we;
	} else {
		new_we = action ? 1 : 0;
	}
	disk->write_protect = !new_we;
	return new_we;
}

_Bool vdrive_set_write_back(unsigned drive, int action) {
	assert(drive < MAX_DRIVES);
	struct vdisk *disk = vdrive_disk_in_drive(drive);
	if (!disk) return -1;
	_Bool new_wb = disk->write_back;
	if (action < 0) {
		new_wb = !new_wb;
	} else {
		new_wb = action ? 1 : 0;
	}
	disk->write_back = new_wb;
	return new_wb;
}

unsigned vdrive_head_pos(void) {
	return head_pos;
}

/* Lines from controller sent to all drives */
void vdrive_set_direction(int direction) {
	cur_direction = (direction > 0) ? 1 : -1;
}

void vdrive_set_head(unsigned head) {
	if (head >= MAX_SIDES) return;
	cur_head = head;
	update_signals();
}

void vdrive_set_dden(_Bool dden) {
	cur_density = dden ? VDISK_DOUBLE_DENSITY : VDISK_SINGLE_DENSITY;
	head_incr = dden ? 1 : 2;
}

static void update_signals(void) {
	vdrive_ready = current_drive->disk_present;
	vdrive_tr00 = (current_drive->current_cyl == 0);
	if (vdrive_update_drive_cyl_head) {
		vdrive_update_drive_cyl_head(cur_drive_number, current_drive->current_cyl, cur_head);
	}
	if (!vdrive_ready) {
		vdrive_write_protect = 0;
		track_base = NULL;
		idamptr = NULL;
		return;
	}
	vdrive_write_protect = current_drive->disk->write_protect;
	if (cur_head < current_drive->disk->num_heads) {
		idamptr = vdisk_track_base(current_drive->disk, current_drive->current_cyl, cur_head);
	} else {
		idamptr = NULL;
	}
	track_base = (uint8_t *)idamptr;
	if (!index_pulse_event.queued) {
		head_pos = 128;
		track_start_cycle = event_current_tick;
		index_pulse_event.at_tick = track_start_cycle + (current_drive->disk->track_length - 128) * BYTE_TIME;
		event_queue(&MACHINE_EVENT_LIST, &index_pulse_event);
	}
}

/* Drive select */
void vdrive_set_drive(unsigned drive) {
	if (drive >= MAX_DRIVES) return;
	cur_drive_number = drive;
	current_drive = &drives[drive];
	update_signals();
}

/* Drive-specific actions */
void vdrive_step(void) {
	if (vdrive_ready) {
		if (cur_direction > 0 || current_drive->current_cyl > 0)
			current_drive->current_cyl += cur_direction;
		if (current_drive->current_cyl >= MAX_TRACKS)
			current_drive->current_cyl = MAX_TRACKS - 1;
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

void vdrive_write(uint8_t data) {
	if (!vdrive_ready) return;
	if (!track_base) {
		idamptr = vdisk_extend_disk(current_drive->disk, current_drive->current_cyl, cur_head);
		track_base = (uint8_t *)idamptr;
	}
	for (unsigned i = head_incr; i; i--) {
		if (track_base && head_pos < current_drive->disk->track_length) {
			track_base[head_pos] = data;
			for (unsigned j = 0; j < 64; j++) {
				if (head_pos == (idamptr[j] & 0x3fff)) {
					idamptr[j] = 0;
					qsort(idamptr, 64, sizeof(uint16_t), compar_idams);
				}
			}
		}
		head_pos++;
	}
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

uint8_t vdrive_read(void) {
	uint8_t ret = 0;
	if (!vdrive_ready) return 0;
	if (track_base && head_pos < current_drive->disk->track_length) {
		ret = track_base[head_pos] & 0xff;
	}
	head_pos += head_incr;
	if (head_pos >= current_drive->disk->track_length) {
		vdrive_index_pulse = 1;
	}
	return ret;
}

#define IDAM(i) ((unsigned)(idamptr[i] & 0x3fff))

void vdrive_write_idam(void) {
	if (!track_base) {
		idamptr = vdisk_extend_disk(current_drive->disk, current_drive->current_cyl, cur_head);
		track_base = (uint8_t *)idamptr;
	}
	if (track_base && (head_pos+head_incr) < current_drive->disk->track_length) {
		/* Write 0xfe and remove old IDAM ptr if it exists */
		for (unsigned i = 0; i < 64; i++) {
			for (unsigned j = 0; j < head_incr; j++) {
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
	if (head_pos >= current_drive->disk->track_length) {
		vdrive_index_pulse = 1;
	}
}

unsigned vdrive_time_to_next_byte(void) {
	event_ticks next_cycle = track_start_cycle + (head_pos - 128) * BYTE_TIME;
	unsigned to_time = next_cycle - event_current_tick;
	if (to_time > (UINT_MAX/2)) {
		LOG_DEBUG(3, "Negative time to next byte!\n");
		return 1;
	}
	return to_time + 1;
}

/* Calculates the number of cycles it would take to get from the current head
 * position to the next IDAM or next index pulse, whichever comes first. */

unsigned vdrive_time_to_next_idam(void) {
	event_ticks next_cycle;
	if (!vdrive_ready) return OSCILLATOR_RATE / 5;
	/* Update head_pos based on time elapsed since track start */
	head_pos = 128 + ((event_current_tick - track_start_cycle) / BYTE_TIME);
	(void)vdrive_new_index_pulse();
	unsigned next_head_pos = current_drive->disk->track_length;
	if (idamptr) {
		for (unsigned i = 0; i < 64; i++) {
			if ((unsigned)(idamptr[i] & 0x8000) == cur_density) {
				unsigned tmp = idamptr[i] & 0x3fff;
				if (head_pos < tmp && tmp < next_head_pos)
					next_head_pos = tmp;
			}
		}
	}
	if (next_head_pos >= current_drive->disk->track_length)
		return (index_pulse_event.at_tick - event_current_tick) + 1;
	next_cycle = track_start_cycle + (next_head_pos - 128) * BYTE_TIME;
	unsigned to_time = next_cycle - event_current_tick;
	if (to_time > (UINT_MAX/2)) {
		LOG_DEBUG(3, "Negative time to next IDAM!\n");
		return 1;
	}
	return to_time + 1;
}

/* Updates head_pos to next IDAM and returns a pointer to it.  If no valid
 * IDAMs are present, an index pulse is generated and the head left at the
 * beginning of the track. */

uint8_t *vdrive_next_idam(void) {
	unsigned next_head_pos;
	if (!vdrive_ready) return NULL;
	next_head_pos = current_drive->disk->track_length;
	if (idamptr) {
		for (unsigned i = 0; i < 64; i++) {
			if ((idamptr[i] & 0x8000) == cur_density) {
				unsigned tmp = idamptr[i] & 0x3fff;
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
_Bool vdrive_new_index_pulse(void) {
	static _Bool last_index_pulse = 0;
	_Bool last = last_index_pulse;
	last_index_pulse = vdrive_index_pulse;
	return (!last && vdrive_index_pulse);
}

static void do_index_pulse(void *data) {
	(void)data;
	if (!vdrive_ready) {
		vdrive_index_pulse = 0;
		return;
	}
	vdrive_index_pulse = 1;
	head_pos = 128;
	last_update_cycle = index_pulse_event.at_tick;
	track_start_cycle = index_pulse_event.at_tick;
	index_pulse_event.at_tick = track_start_cycle + (current_drive->disk->track_length - 128) * BYTE_TIME;
	event_queue(&MACHINE_EVENT_LIST, &index_pulse_event);
	reset_index_pulse_event.at_tick = track_start_cycle + ((current_drive->disk->track_length - 128)/100) * BYTE_TIME;
	event_queue(&MACHINE_EVENT_LIST, &reset_index_pulse_event);
}

static void do_reset_index_pulse(void *data) {
	(void)data;
	vdrive_index_pulse = 0;
	/* reset latch */
	(void)vdrive_new_index_pulse();
}
