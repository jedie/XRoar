/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2009  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef __SAM_H__
#define __SAM_H__

#include "types.h"

/* Simple macro for use in place of sam_read_byte() when the result isn't
 * required, just appropriate timing.  Side-effects of reads obviously won't
 * happen, but in practice that should almost certainly never matter. */
#ifdef VARIABLE_MPU_RATE
extern unsigned int sam_ram_cycles;
extern unsigned int sam_rom_cycles;
# define sam_peek_byte(a) do { \
		if ((((a)&0xffff) >= 0x8000) && ((((a)&0xffff) < 0xff00) \
					|| (((a)&0xffff) >= 0xff20))) \
			current_cycle += sam_rom_cycles; \
		else \
			current_cycle += sam_ram_cycles; \
	} while(0)
#else
# define sam_ram_cycles CPU_SLOW_DIVISOR
# define sam_rom_cycles CPU_SLOW_DIVISOR
# define sam_peek_byte(a) do { current_cycle += CPU_SLOW_DIVISOR; } while (0)
#endif

extern unsigned int sam_register;
extern unsigned int sam_vdg_mode;
extern unsigned int sam_vdg_address;
extern unsigned int sam_vdg_mod_clear;

static inline void sam_vdg_hsync(void) {
	sam_vdg_address &= sam_vdg_mod_clear;
}

void sam_init(void);
void sam_reset(void);
unsigned int sam_read_byte(unsigned int addr);
void sam_store_byte(unsigned int addr, unsigned int octet);
void sam_vdg_fsync(void);
uint8_t *sam_vdg_bytes(int number);
void sam_set_register(unsigned int value);

#endif  /* __SAM_H__ */
