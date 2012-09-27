/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2012  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_WD279X_H_
#define XROAR_WD279X_H_

#include "types.h"

enum WD279X_type {
	WD2791, WD2793, WD2795, WD2797
};

extern int wd279x_type;
extern void (*wd279x_set_drq_handler)(void);
extern void (*wd279x_reset_drq_handler)(void);
extern void (*wd279x_set_intrq_handler)(void);
extern void (*wd279x_reset_intrq_handler)(void);

void wd279x_init(void);
void wd279x_reset(void);
void wd279x_set_dden(_Bool dden);  /* 1 = Double density, 0 = Single */
uint8_t wd279x_read(uint16_t A);
void wd279x_write(uint16_t A, uint8_t D);

#endif  /* XROAR_WD279X_H_ */
