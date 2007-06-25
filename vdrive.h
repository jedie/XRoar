/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2007  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

/* Implements virtual disk in a drive */

#ifndef __VDRIVE_H__
#define __VDRIVE_H__

#include "types.h"
#include "vdisk.h"

#define VDRIVE_MAX_DRIVES (4)

#define VDRIVE_MOTOR_OFF (0)
#define VDRIVE_MOTOR_ON  (1)

#define VDRIVE_WRITE_CRC16 do { \
		uint16_t tmp_write_crc = crc16_value(); \
		vdrive_write(tmp_write_crc >> 8); \
		vdrive_write(tmp_write_crc & 0xff); \
	} while (0)

extern int vdrive_ready;
extern int vdrive_tr00;
extern int vdrive_index_pulse;
extern int vdrive_write_protect;

void vdrive_init(void);
void vdrive_shutdown(void);

int vdrive_insert_disk(int drive, struct vdisk *disk);
int vdrive_eject_disk(int drive);
struct vdisk *vdrive_disk_in_drive(int drive);

unsigned int vdrive_head_pos(void);

/* Lines from controller sent to all drives */
void vdrive_set_direction(int direction);
void vdrive_set_side(int side);
void vdrive_set_density(int density);

/* Drive select */
void vdrive_set_drive(int drive);

/* Drive-specific actions */
void vdrive_step(void);
void vdrive_write(unsigned int data);
void vdrive_skip(void);
unsigned int vdrive_read(void);
void vdrive_write_idam(void);
int vdrive_new_index_pulse(void);  /* Has there been one? */
unsigned int vdrive_time_to_next_byte(void);
unsigned int vdrive_time_to_next_idam(void);
uint8_t *vdrive_next_idam(void);

#endif  /* __VDRIVE_H__ */
