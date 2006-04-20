/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2006  Ciaran Anscomb
 *
 *  See COPYING for redistribution conditions. */

#ifndef __WD2797_H__
#define __WD2797_H__

#include "types.h"

void wd2797_init(void);
void wd2797_reset(void);
int wd2797_load_disk(char *filename, int drive);
void wd2797_command_write(unsigned int octet);
void wd2797_track_register_write(unsigned int octet);
void wd2797_sector_register_write(unsigned int octet);
void wd2797_data_register_write(unsigned int octet);
unsigned int wd2797_status_read(void);
unsigned int wd2797_track_register_read(void);
unsigned int wd2797_sector_register_read(void);
unsigned int wd2797_data_register_read(void);
void wd2797_ff48_write(unsigned int octet);
void wd2797_ff40_write(unsigned int octet);

#endif  /* __WD2797_H__ */
