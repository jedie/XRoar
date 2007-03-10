/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2007  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef __SAM_H__
#define __SAM_H__

#include "types.h"

#define sam_vram_ptr(a) (a<0x8000 ? &addrptr_low[a] : &addrptr_high[a-0x8000])

/* Simple macro for use in place of sam_read_byte() when the result isn't
 * required, just appropriate timing.  Side-effects of reads obviously won't
 * happen, but in practice that should almost certainly never matter. */
#ifdef HAVE_GP32
# define sam_peek_byte(a) do { current_cycle += CPU_SLOW_DIVISOR; } while (0)
#else
#define sam_peek_byte(a) do { \
		if ((((a)&0xffff) >= 0x8000) && ((((a)&0xffff) < 0xff00) \
					|| (((a)&0xffff) >= 0xff20))) \
			current_cycle += sam_topaddr_cycles; \
		else \
			current_cycle += CPU_SLOW_DIVISOR; \
	} while(0)
#endif

extern uint8_t *addrptr_low;
extern uint8_t *addrptr_high;
extern uint_least16_t sam_register;
extern uint_least16_t sam_vdg_base;
extern uint_least16_t sam_vdg_line_base;
extern unsigned int  sam_vdg_mode;
extern uint_least16_t sam_vdg_address;
extern unsigned int  sam_vdg_mod_xdiv;
extern unsigned int  sam_vdg_mod_ydiv;
extern uint_least16_t sam_vdg_mod_clear;
extern unsigned int  sam_vdg_xcount;
extern unsigned int  sam_vdg_ycount;
#ifdef HAVE_GP32
# define sam_topaddr_cycles CPU_SLOW_DIVISOR
#else
extern unsigned int sam_topaddr_cycles;
#endif

static inline void sam_vdg_xstep(int b) {
	sam_vdg_address += b;
	sam_vdg_xcount++;
	if (sam_vdg_xcount >= sam_vdg_mod_xdiv) {
		sam_vdg_xcount = 0;
	} else {
		if (sam_vdg_mode != 7) {
			if (sam_vdg_address & 8) {
				sam_vdg_address += 6;
			} else {
				sam_vdg_address -= 6;
			}
			sam_vdg_address &= ~15;
		}
	}
}

static inline void sam_vdg_hsync(int a) {
	sam_vdg_address += a;
	sam_vdg_ycount++;
	if (sam_vdg_ycount >= sam_vdg_mod_ydiv) {
		sam_vdg_ycount = 0;
	} else {
		sam_vdg_address = sam_vdg_line_base;
	}
	sam_vdg_address &= sam_vdg_mod_clear;
	sam_vdg_line_base = sam_vdg_address;
}

static inline void sam_vdg_fsync(void) {
	sam_vdg_line_base = sam_vdg_address = sam_vdg_base;
	sam_vdg_xcount = 0;
	sam_vdg_ycount = 0;
}

void sam_init(void);
void sam_reset(void);
unsigned int sam_read_byte(uint_least16_t addr);
void sam_store_byte(uint_least16_t addr, unsigned int octet);
void sam_update_from_register(void);

#endif  /* __SAM_H__ */
