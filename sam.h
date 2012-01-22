/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2012  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_SAM_H_
#define XROAR_SAM_H_

#include "types.h"

#define SAM_CPU_SLOW_DIVISOR 16
#define SAM_CPU_FAST_DIVISOR 8

void sam_init(void);
void sam_reset(void);
void sam_run(int cycles);
uint8_t sam_read_byte(uint16_t addr);
void sam_store_byte(uint16_t addr, uint8_t octet);
void sam_nvma_cycles(int cycles);
void sam_vdg_hsync(void);
void sam_vdg_fsync(void);
void sam_vdg_bytes(int nbytes, uint8_t *dest);
void sam_set_register(unsigned int value);
unsigned int sam_get_register(void);

#endif  /* XROAR_SAM_H_ */
