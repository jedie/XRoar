/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2004  Ciaran Anscomb
 *
 *  See COPYING for redistribution conditions. */

#ifndef __SAM_H__
#define __SAM_H__

#include "types.h"

#define sam_read_byte(a) _sam_read_byte(a)
#define sam_store_byte(a,v) _sam_store_byte(a,v)
/*
#define sam_read_byte(a) (((a)&0xffff) < 0x8000 ? addrptr_low[((a)&0xffff)] : ( ((a)&0xffff) < 0xff00 ? addrptr_high[((a)&0xffff)-0x8000] : _sam_read_byte(((a)&0xffff))))
#define sam_store_byte(a,v) \
	{ if (((a)&0xffff) < 0x8000) { addrptr_low[((a)&0xffff)] = v; } \
	else { \
		if (((a)&0xffff) < 0xff00) { \
			if (mapped_ram) \
				addrptr_high[((a)&0xffff)-0x8000] = v; \
		} else { \
			_sam_store_byte(((a)&0xffff),v); \
		} \
	} }
*/

#define sam_read_word(a) (sam_read_byte(a)<<8 | sam_read_byte(a+1))

#define sam_vdg_xstep(b) { \
		sam_vdg_xcount++; \
		if (sam_vdg_xcount >= sam_vdg_mod_xdiv) { \
			sam_vdg_xcount = 0; \
			sam_vdg_address += b; \
		} \
	}

#define sam_vdg_hsync(bpl,a,p) { \
		sam_vdg_ycount++; \
		if (sam_vdg_ycount >= sam_vdg_mod_ydiv) { \
			sam_vdg_ycount = 0; \
		} else { \
			sam_vdg_address -= bpl; \
		} \
		sam_vdg_xstep(sam_vdg_mode == 7 ? a : p); \
		sam_vdg_address &= sam_vdg_mod_clear; \
	}

#define sam_vdg_fsync() { \
		sam_vdg_address = sam_vdg_base; \
		sam_vdg_xcount = 0; \
		sam_vdg_ycount = 0; \
	}

#define sam_vram_ptr(a) (a<0x8000 ? &addrptr_low[a] : &addrptr_high[a-0x8000])

extern uint8_t *addrptr_low;
extern uint8_t *addrptr_high;
extern uint_fast16_t mapped_ram;
extern uint_fast16_t sam_register;
extern uint_fast16_t sam_vdg_base;
extern uint_fast8_t  sam_vdg_mode;
extern uint_fast16_t sam_vdg_address;
extern uint_fast8_t  sam_vdg_mod_xdiv;
extern uint_fast8_t  sam_vdg_mod_ydiv;
extern uint_fast16_t sam_vdg_mod_clear;
extern uint_fast8_t  sam_vdg_xcount;
extern uint_fast8_t  sam_vdg_ycount;

void sam_init(void);
void sam_reset(void);
uint_fast8_t _sam_read_byte(uint_fast16_t addr);
void _sam_store_byte(uint_fast16_t addr, uint_fast8_t octet);
void sam_update_from_register(void);

#endif  /* __SAM_H__ */
