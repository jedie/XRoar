/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2005  Ciaran Anscomb
 *
 *  See COPYING for redistribution conditions. */

/* Implements virtual disk in a drive */

#ifndef __VDRIVE_H__
#define __VDRIVE_H__

#include "types.h"

#define VDRIVE_LENGTH_5_25 (0x1900)
#define VDRIVE_LENGTH_8    (0x2940)

#define VDRIVE_DOUBLE_DENSITY (0x8000)
#define VDRIVE_SINGLE_DENSITY (0x0000)

#define VDRIVE_WRITE_CRC16 do { \
		uint16_t tmp_write_crc = crc16_value(); \
		vdrive_write(tmp_write_crc >> 8); \
		vdrive_write(tmp_write_crc & 0xff); \
	} while (0)

extern int vdrive_ready;
extern int vdrive_tr00;
extern int vdrive_write_protect;

void vdrive_init(void);

/* Managing space for disks */
int vdrive_alloc(unsigned int drive, unsigned int tracks,
		unsigned int track_length, unsigned int sides);

/* Lines from controller sent to all drives */
void vdrive_set_direction(int d);
void vdrive_set_side(unsigned int s);
void vdrive_set_density(unsigned int d);

/* Drive select */
void vdrive_set_drive(unsigned int d);

/* Drive-specific actions */
void vdrive_step(void);
void vdrive_write(unsigned int data);
void vdrive_skip(void);
unsigned int vdrive_read(void);
void vdrive_write_idam(void);
int vdrive_index_pulse(void);  /* Has there been one? */
unsigned int vdrive_time_to_next_idam(void);
uint8_t *vdrive_next_idam(void);

/* Convenience functions */
int vdrive_format_disk(unsigned int drive, unsigned int num_tracks,
		unsigned int num_sides, unsigned int num_sectors,
		unsigned int ssize_code, unsigned int first_sector);
void vdrive_update_sector(unsigned int drive, unsigned int track,
		unsigned int side, unsigned int sector,
		unsigned int ssize_code, uint8_t *data);

#endif  /* __VDRIVE_H__ */
