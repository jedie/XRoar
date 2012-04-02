/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2012  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_SAM_H_
#define XROAR_SAM_H_

#include "types.h"

#define SAM_CPU_SLOW_DIVISOR 16
#define SAM_CPU_FAST_DIVISOR 8

#define sam_init()
void sam_reset(void);
int sam_run(uint16_t A, int RnW, int *S, uint16_t *Z, int *ncycles);
int sam_nvma_cycles(int c);
void sam_vdg_hsync(void);
void sam_vdg_fsync(void);
void sam_vdg_bytes(int nbytes, uint8_t *dest);
void sam_set_register(unsigned int value);
unsigned int sam_get_register(void);

#endif  /* XROAR_SAM_H_ */
