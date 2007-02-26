/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2006  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef __WD2797_H__
#define __WD2797_H__

#include "types.h"

enum WD279X_type {
	WD2791, WD2793, WD2795, WD2797
};

extern unsigned int wd279x_type;

void wd279x_init(void);
void wd279x_reset(void);
int wd279x_load_disk(char *filename, int drive);
void wd279x_command_write(unsigned int octet);
void wd279x_track_register_write(unsigned int octet);
void wd279x_sector_register_write(unsigned int octet);
void wd279x_data_register_write(unsigned int octet);
unsigned int wd279x_status_read(void);
unsigned int wd279x_track_register_read(void);
unsigned int wd279x_sector_register_read(void);
unsigned int wd279x_data_register_read(void);
void wd279x_ff48_write(unsigned int octet);
void wd279x_ff40_write(unsigned int octet);

#endif  /* __WD2797_H__ */
