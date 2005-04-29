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

#include <stdlib.h>
#include <string.h>

#include "xroar.h"
#include "wd2797.h"
#include "m6809.h"
#include "pia.h"
#include "fs.h"
#include "logging.h"
#include "types.h"

#define MODE_NORMAL (0)
#define MODE_TYPE_1 (1)
#define MODE_TYPE_2 (2)
#define MODE_TYPE_3 (3)

#define STATUS_NOT_READY     (1<<7)
#define STATUS_WRITE_PROTECT (1<<6)
#define STATUS_HEAD_LOADED   (1<<5)
#define STATUS_RECORD_TYPE   (1<<5)
#define STATUS_SEEK_ERROR    (1<<4)
#define STATUS_RNF           (1<<4)
#define STATUS_CRC_ERROR     (1<<3)
#define STATUS_TRACK_0       (1<<2)
#define STATUS_LOST_DATA     (1<<2)
#define STATUS_INDEX_PULSE   (1<<1)
#define STATUS_DRQ           (1<<1)
#define STATUS_BUSY          (1<<0)

#define INTERRUPT_DISABLE (0)
#define INTERRUPT_DRQ     (1)
#define INTERRUPT_INTRQ   (2)

#define SCHEDULE_INTERRUPT(i,c) enable_disk_interrupt = (i); next_disk_interrupt = current_cycle + (c); if (!(c)) { wd2797_generate_interrupt(); }
#define WD2797_INTRQ if (ic1_nmi_enable) nmi = 1

/* Disks are implemented as an array of tracks, each track being a pointer to a
 * linked lists of disk sectors.  This is because technically, a track can have
 * sectors on it that have completely different track ids.  It's all down to
 * how you format them.  This is probably almost never going to be useful. */

typedef struct Sector Sector;
struct Sector {
	unsigned int size;
	unsigned int track_number;
	unsigned int sector_number;
	unsigned int side_number;
	uint8_t *data;
	Sector *next;
};

typedef struct {
	char *disk_name;
	unsigned int num_tracks;
	int head_position;
	unsigned int head_loaded;
	Sector **track;
} Disk;

static Disk *current_disk;
static Disk disk[4];

static unsigned int ic1_drive_select;
static unsigned int ic1_motor_enable;
static unsigned int ic1_precomp_enable;
static unsigned int ic1_density;
static unsigned int ic1_nmi_enable;

static unsigned int status_register;
static unsigned int track_register;
static unsigned int sector_register;
static unsigned int data_register;
static unsigned int mode;
static int step_direction;

static unsigned int data_left;
static uint8_t *data_ptr;

void wd2797_init(void) {
	memset(disk, 0, sizeof(disk));
}

void wd2797_reset(void) {
	ic1_drive_select = 0;
	current_disk = &disk[ic1_drive_select];
	ic1_motor_enable = 0;
	ic1_precomp_enable = 0;
	ic1_density = 0;
	ic1_nmi_enable = 0;
	status_register = 0;
	track_register = 0;
	sector_register = 0;
	data_register = 0;
	mode = 0;
	step_direction = -1;
}

static void wd2797_remove_disk(int drive) {
	Sector *sector, *cur;
	unsigned int i;
	drive &= 3;
	if (disk[drive].disk_name == NULL)
		return;
	free(disk[drive].disk_name);
	disk[drive].disk_name = NULL;
	for (i = 0; i < disk[drive].num_tracks; i++) {
		LOG_DEBUG(5,"Freeing track %d ", i);
		sector = disk[drive].track[i];
		while (sector != NULL) {
			LOG_DEBUG(5,".");
			cur = sector;
			sector = cur->next;
			free(cur->data);
			free(cur);
		}
		LOG_DEBUG(5,"\n");
		free(disk[drive].track[i]);
	}
	free(disk[drive].track);
}

int wd2797_load_disk(char *filename, int drive) {
	FS_FILE fd;
	Sector *track, **sector;
	unsigned int skip;
	int it, is;
	uint8_t junk[64];
	int tracks, sectors;
	drive &= 3;
	wd2797_remove_disk(drive);
	if ((fd = fs_open(filename, FS_READ)) == -1)
		return -1;
	fs_read(fd, junk, 12);
	tracks = junk[8];
	sectors = 18;
	skip = (junk[3]<<8 | junk[2]) - 12;
	while (skip > sizeof(junk)) {
		fs_read(fd, junk, sizeof(junk));
		skip -= sizeof(junk);
	}
	if (skip) fs_read(fd, junk, skip);
	disk[drive].num_tracks = tracks;
	disk[drive].track = malloc(tracks * sizeof(Sector *));
	for (it = 0; it < tracks; it++) {
		sector = &track;
		for (is = 1; is <= sectors; is++) {
			*sector = malloc(sizeof(Sector));
			(*sector)->size = 256;
			(*sector)->track_number = it;
			(*sector)->sector_number = is;
			(*sector)->side_number = 0;
			(*sector)->data = malloc(256);
			fs_read(fd, (*sector)->data, 256);
			(*sector)->next = NULL;
			sector = &(*sector)->next;
		}
		disk[drive].track[it] = track;
	}
	fs_close(fd);
	return 0;
}

static void wd2797_step(unsigned int flags) {
	current_disk->head_position += step_direction;
	if (flags & 0x10)
		track_register += step_direction;
	status_register = 0;
	if (current_disk->head_position <= 0) {
		status_register |= STATUS_TRACK_0;
		current_disk->head_position = 0;
		if (flags & 0x10)
			track_register = 0;
	}
	if ((flags & 0x04) && ((unsigned int)current_disk->head_position != track_register)) {
		status_register |= STATUS_SEEK_ERROR;
	}
	LOG_DEBUG(4, "WD2797: HEAD POS = %02x, TRACK REG = %02x\n", current_disk->head_position, track_register);
}

void wd2797_command_write(unsigned int octet) {
	/* RESTORE */
	if ((octet & 0xf0) == 0) {
		LOG_DEBUG(4, "WD2797: CMD: Restore to track 00\n");
		mode = MODE_TYPE_1;
		status_register = STATUS_BUSY;
		step_direction = -1;
		track_register = 0;
		current_disk->head_position = 0;
		/* XXX: should technically verify here if v flag set */
		SCHEDULE_INTERRUPT(INTERRUPT_INTRQ, 2000);
		return;
	}
	/* SEEK */
	if ((octet & 0xf0) == 0x10) {
		LOG_DEBUG(4, "WD2797: CMD: Seek to track %02x\n", data_register);
		mode = MODE_TYPE_1;
		status_register = STATUS_BUSY;
		current_disk->head_loaded = octet & 0x10;
		if (data_register < track_register)

			step_direction = -1;
		if (data_register > track_register)
			step_direction = 1;
		while (track_register != data_register) {
			track_register += step_direction;
			current_disk->head_position += step_direction;
			if (current_disk->head_position < 0) {
				current_disk->head_position = 0;
			}
		}
		SCHEDULE_INTERRUPT(INTERRUPT_INTRQ, 2000);
		return;
	}
	/* STEP */
	if ((octet & 0xe0) == 0x20) {
		LOG_DEBUG(4, "WD2797: CMD: Step (%d)\n", step_direction);
		wd2797_step(octet & 0x1c);
		SCHEDULE_INTERRUPT(INTERRUPT_INTRQ, 2000);
		return;
	}
	/* STEP-IN */
	if ((octet & 0xe0) == 0x40) {
		LOG_DEBUG(4, "WD2797: CMD: Step-in\n");
		step_direction = 1;
		wd2797_step(octet & 0x1c);
		SCHEDULE_INTERRUPT(INTERRUPT_INTRQ, 2000);
		return;
	}
	/* STEP-OUT */
	if ((octet & 0xe0) == 0x60) {
		LOG_DEBUG(4, "WD2797: CMD: Step-out\n");
		step_direction = -1;
		wd2797_step(octet & 0x1c);
		SCHEDULE_INTERRUPT(INTERRUPT_INTRQ, 2000);
		return;
	}
	/* READ SECTOR */
	if ((octet & 0xe1) == 0x80) {
		Sector *sector;
		LOG_DEBUG(4, "WD2797: CMD: Read sector, Track = %02x, Sector = %02x\n",
		           track_register, sector_register);
		if ((unsigned int)current_disk->head_position > current_disk->num_tracks) {
			LOG_DEBUG(4, "WD2797: HEAD POS > NUM TRACKS\n");
			status_register = STATUS_RNF;
			SCHEDULE_INTERRUPT(INTERRUPT_INTRQ, 2000);
			return;
		}
		if (octet & 0x10) {
			LOG_WARN("WD2797: M FLAG UNSUPPORTED\n");
			status_register = STATUS_RNF;
			SCHEDULE_INTERRUPT(INTERRUPT_INTRQ, 2000);
			return;
		}
		if ((unsigned int)current_disk->head_position < current_disk->num_tracks) {
			sector = current_disk->track[current_disk->head_position];
			while (sector != NULL) {
				if ((sector_register == sector->sector_number) &&
				    (track_register == sector->track_number)) {
					mode = MODE_TYPE_2;
					data_left = sector->size;
					data_ptr = sector->data;
					status_register = STATUS_BUSY;
					SCHEDULE_INTERRUPT(INTERRUPT_DRQ, 300);
					return;
				}
				sector = sector->next;
			}
		}
		LOG_DEBUG(4, "WD2797: RECORD NOT FOUND\n");
		status_register = STATUS_RNF;
		SCHEDULE_INTERRUPT(INTERRUPT_INTRQ, 2000);
		return;
	}
	/* FORCE INTERRUPT */
	if ((octet & 0xf0) == 0xd0) {
		LOG_DEBUG(4, "WD2797: CMD: Force interrupt (%01x) (%04x bytes left)\n",
		           octet & 0x0f, data_left);
		status_register = 0;
		if (mode == MODE_TYPE_1) {
			if (current_disk->head_position == 0)
				status_register |= STATUS_TRACK_0;
		}
		mode = MODE_NORMAL;
		if (octet & 0x08) {
			SCHEDULE_INTERRUPT(INTERRUPT_INTRQ, 1);
		}
		return;
	}
	LOG_WARN("WD2797: CMD: Unknown command %02x\n", octet);
	return;
}

void wd2797_track_register_write(unsigned int octet) {
	LOG_DEBUG(4, "WD2797: Set track_register = %02x\n", octet);
	track_register = octet;
}

void wd2797_sector_register_write(unsigned int octet) {
	LOG_DEBUG(4, "WD2797: Set sector_register = %02x\n", octet);
	sector_register = octet;
}

void wd2797_data_register_write(unsigned int octet) {
	data_register = octet;
	if (mode == MODE_TYPE_2) {
		if (data_left) {
			*(data_ptr++) = octet;
			data_left--;
		}
		if (data_left) {
			SCHEDULE_INTERRUPT(INTERRUPT_DRQ, 300);
			status_register = STATUS_BUSY;
		} else {
			SCHEDULE_INTERRUPT(INTERRUPT_INTRQ, 2000);
			status_register = 0;
		}
		return;
	}
	if (mode == MODE_TYPE_3) {
		/* just see what gets written for now... */
		LOG_DEBUG(5, "%02x  ", data_register);
	}
}

unsigned int wd2797_status_read(void) {
	LOG_DEBUG(4, "WD2797: Read %02x from status_register\n", status_register);
	return status_register;
}

unsigned int wd2797_track_register_read(void) {
	LOG_DEBUG(4, "WD2797: Read %02x from track_register\n", track_register);
	return track_register;
}

unsigned int wd2797_sector_register_read(void) {
	LOG_DEBUG(4, "WD2797: Read %02x from sector_register\n", sector_register);
	return sector_register;
}

unsigned int wd2797_data_register_read(void) {
	if (mode == MODE_TYPE_2) {
		if (data_left) {
			data_register = *(data_ptr++);
			//LOG_DEBUG(2, "%02x ", data_register);
			data_left--;
			if (data_left) {
				/* DRQs have to be sufficiently far apart to
				 * allow the ROM to reach a wait-for-interrupt
				 * instruction.  This is an artifact of
				 * providing the data "on demand", which is
				 * unrealistic. */
				SCHEDULE_INTERRUPT(INTERRUPT_DRQ, 300);
				status_register = STATUS_BUSY;
			} else {
				LOG_DEBUG(4, "WD2797: FINISHED READ SECTOR\n");
				status_register = STATUS_BUSY;
				mode = MODE_NORMAL;
				SCHEDULE_INTERRUPT(INTERRUPT_INTRQ, 2000);
			}
		} else {
			mode = MODE_NORMAL;
		}
	}
	return data_register;
}

void wd2797_generate_interrupt(void) {
	if (enable_disk_interrupt == INTERRUPT_DRQ) {
		status_register = STATUS_BUSY | STATUS_DRQ;
		PIA_SET_P1CB1;
	}
	if (enable_disk_interrupt == INTERRUPT_INTRQ) {
		status_register = 0;
		if (mode == MODE_TYPE_1) {
			if (current_disk->head_position == 0)
				status_register |= STATUS_TRACK_0;
		}
		WD2797_INTRQ;
		mode = MODE_NORMAL;
	}
	enable_disk_interrupt = INTERRUPT_DISABLE;
}

/* Not strictly WD2797 accesses here, this is DOS cartridge circuitry */
void wd2797_ff48_write(unsigned int octet) {
	LOG_DEBUG(4, "WD2797: Write to FF48: ");
	if ((octet & 0x03) != ic1_drive_select)
		LOG_DEBUG(4, "DRIVE SELECT %01d, ", octet & 0x03);
	if ((octet & 0x04) != ic1_motor_enable)
		LOG_DEBUG(4, "MOTOR %s, ", (octet & 0x04)?"ON":"OFF");
	if ((octet & 0x08) != ic1_precomp_enable)
		LOG_DEBUG(4, "PRECOMP %s, ", (octet & 0x08)?"ON":"OFF");
	if ((octet & 0x10) != ic1_density)
		LOG_DEBUG(4, "DENSITY %s, ", (octet & 0x10)?"DOUBLE":"SINGLE");
	if ((octet & 0x20) != ic1_nmi_enable)
		LOG_DEBUG(4, "NMI %s, ", (octet & 0x20)?"ENABLED":"DISABLED");
	LOG_DEBUG(4, "\n");
	ic1_drive_select = octet & 0x03;
	current_disk = &disk[ic1_drive_select];
	ic1_motor_enable = octet & 0x04;
	ic1_precomp_enable = octet & 0x08;
	ic1_density = octet & 0x10;
	ic1_nmi_enable = octet & 0x20;
}

unsigned int wd2797_ff48_read(void) {
	return ic1_nmi_enable | ic1_precomp_enable | ic1_motor_enable |
	       ic1_drive_select;
}
