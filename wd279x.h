/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2007  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef __WD279X_H__
#define __WD279X_H__

#include "types.h"

enum WD279X_type {
	WD2791, WD2793, WD2795, WD2797
};

extern unsigned int wd279x_type;
extern void (*wd279x_set_drq_handler)(void);
extern void (*wd279x_reset_drq_handler)(void);
extern void (*wd279x_set_intrq_handler)(void);
extern void (*wd279x_reset_intrq_handler)(void);

void wd279x_init(void);
void wd279x_reset(void);
void wd279x_set_density(unsigned int d);  /* 0 = Double, else Single */
void wd279x_command_write(unsigned int octet);
void wd279x_track_register_write(unsigned int octet);
void wd279x_sector_register_write(unsigned int octet);
void wd279x_data_register_write(unsigned int octet);
unsigned int wd279x_status_read(void);
unsigned int wd279x_track_register_read(void);
unsigned int wd279x_sector_register_read(void);
unsigned int wd279x_data_register_read(void);

#endif  /* __WD279X_H__ */
