/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2014  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

/* Implements virtual disk in a drive */

#ifndef XROAR_VDRIVE_H_
#define XROAR_VDRIVE_H_

#include <stdint.h>

#include "delegate.h"

struct vdisk;

#define VDRIVE_MAX_DRIVES (4)

extern _Bool vdrive_ready;
extern _Bool vdrive_tr00;
DELEGATE_T1(void,bool) vdrive_index_pulse;
extern _Bool vdrive_write_protect;
extern void (*vdrive_update_drive_cyl_head)(unsigned drive, unsigned cyl, unsigned head);

void vdrive_init(void);
void vdrive_shutdown(void);

void vdrive_insert_disk(unsigned drive, struct vdisk *disk);
void vdrive_eject_disk(unsigned drive);
struct vdisk *vdrive_disk_in_drive(unsigned drive);
_Bool vdrive_set_write_enable(unsigned drive, int action);
_Bool vdrive_set_write_back(unsigned drive, int action);

unsigned vdrive_head_pos(void);

/* Lines from controller sent to all drives */
void vdrive_set_dirc(void *, int);
void vdrive_set_dden(void *, _Bool);
void vdrive_set_sso(void *, unsigned);

/* Drive select */
void vdrive_set_drive(unsigned drive);

/* Drive-specific actions */
void vdrive_step(void);
void vdrive_write(uint8_t data);
void vdrive_skip(void);
uint8_t vdrive_read(void);
void vdrive_write_idam(void);
unsigned vdrive_time_to_next_byte(void);
unsigned vdrive_time_to_next_idam(void);
uint8_t *vdrive_next_idam(void);

#endif  /* XROAR_VDRIVE_H_ */
