/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2011  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_SAM_H_
#define XROAR_SAM_H_

#include "types.h"

extern uint8_t *sam_mapped_rom;
extern unsigned int sam_vdg_address;
extern unsigned int sam_vdg_mod_clear;

static inline void sam_vdg_hsync(void) {
	sam_vdg_address &= sam_vdg_mod_clear;
}

void sam_init(void);
void sam_reset(void);
void sam_run(int cycles);
unsigned int sam_read_byte(unsigned int addr);
void sam_store_byte(unsigned int addr, unsigned int octet);
void sam_nvma_cycles(int cycles);
void sam_vdg_fsync(void);
void sam_vdg_bytes(int nbytes, uint8_t *dest);
void sam_set_register(unsigned int value);
unsigned int sam_get_register(void);

#endif  /* XROAR_SAM_H_ */
