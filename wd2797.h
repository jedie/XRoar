/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2004  Ciaran Anscomb
 *
 *  See COPYING for redistribution conditions. */

#ifndef __WD2797_H__
#define __WD2797_H__

#include "types.h"

void wd2797_init(void);
void wd2797_reset(void);
int wd2797_load_disk(char *filename, int drive);
void wd2797_command_write(uint_fast8_t octet);
void wd2797_track_register_write(uint_fast8_t octet);
void wd2797_sector_register_write(uint_fast8_t octet);
void wd2797_data_register_write(uint_fast8_t octet);
uint_fast8_t wd2797_status_read(void);
uint_fast8_t wd2797_track_register_read(void);
uint_fast8_t wd2797_sector_register_read(void);
uint_fast8_t wd2797_data_register_read(void);
void wd2797_generate_interrupt(void);
void wd2797_ff48_write(uint_fast8_t octet);
uint_fast8_t wd2797_ff48_read(void);

#endif  /* __WD2797_H__ */
