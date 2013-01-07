/*  Copyright 2003-2013 Ciaran Anscomb
 *
 *  This file is part of XRoar.
 *
 *  XRoar is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  XRoar is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XRoar.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <inttypes.h>
#include <stdlib.h>

#include "sam.h"

/* Constants for tracking VDG address counter */
static int vdg_mod_xdivs[8] = { 1, 3, 1, 2, 1, 1, 1, 1 };
static int vdg_mod_ydivs[8] = { 12, 1, 3, 1, 2, 1, 1, 1 };
static int vdg_mod_adds[8] = { 16, 8, 16, 8, 16, 8, 16, 0 };
static uint16_t vdg_mod_clears[8] = { ~30, ~14, ~30, ~14, ~30, ~14, ~30, ~0 };

/* Constants for address multiplexer
 * SAM Data Sheet,
 *   Figure 6 - Signal routing for address multiplexer */
static uint16_t ram_row_masks[4] = { 0x007f, 0x007f, 0x00ff, 0x00ff };
static int ram_col_shifts[4] = { 2, 1, 0, 0 };
static uint16_t ram_col_masks[4] = { 0x3f00, 0x7f00, 0xff00, 0xff00 };
static uint16_t ram_ras1_bits[4] = { 0x1000, 0x4000, 0, 0 };

/* VDG address counter */
static uint16_t vdg_base;
static uint16_t vdg_address;
static int vdg_mod_xdiv;
static int vdg_mod_ydiv;
static int vdg_mod_add;
static uint16_t vdg_mod_clear;
static int vdg_xcount;
static int vdg_ycount;

/* Address multiplexer */
static uint16_t ram_row_mask;
static int ram_col_shift;
static uint16_t ram_col_mask;
static uint16_t ram_ras1_bit;
static uint16_t ram_ras1;
static uint16_t ram_page_bit;

/* Address decode */
static _Bool map_type_1;

/* SAM control register */
static uint_fast16_t sam_register;

/* MPU rate */
static _Bool mpu_rate_fast;
static _Bool mpu_rate_ad;
static _Bool running_fast = 0;

static void update_from_register(void);

void sam_reset(void) {
	sam_set_register(0);
	sam_vdg_fsync();
}

#define VRAM_TRANSLATE(a) ( \
		((a << ram_col_shift) & ram_col_mask) \
		| (a & ram_row_mask) \
		| (!(a & ram_ras1_bit) ? ram_ras1 : 0) \
	)

#define RAM_TRANSLATE(a) (VRAM_TRANSLATE(a) | ram_page_bit)

/* The primary function of the SAM: translates an address (A) plus Read/!Write
 * flag (RnW) into an S value and RAM address (Z).  Writes to the SAM control
 * register will update the configuration.  The number of (SAM) cycles the CPU
 * clock would be use for this access is written to ncycles.  Returns 1 when
 * the access is to a RAM area, 0 otherwise. */

_Bool sam_run(uint16_t A, _Bool RnW, int *S, uint16_t *Z, int *ncycles) {
	_Bool is_ram_access;
	_Bool fast_cycle;
	if (A < 0x8000 || (map_type_1 && A < 0xff00)) {
		*Z = RAM_TRANSLATE(A);
		is_ram_access = 1;
		fast_cycle = mpu_rate_fast;
	} else {
		fast_cycle = mpu_rate_fast || mpu_rate_ad;
		is_ram_access = 0;
	}
	if (A < 0x8000) {
		*S = RnW ? 0 : 7;
	} else if (map_type_1 && RnW && A < 0xff00) {
		*S = 0;
	} else if (A < 0xa000) {
		*S = 1;
	} else if (A < 0xc000) {
		*S = 2;
	} else if (A < 0xff00) {
		*S = 3;
	} else if (A < 0xff20) {
		*S = 4;
		fast_cycle = mpu_rate_fast;
	} else if (A < 0xff40) {
		*S = 5;
	} else if (A < 0xff60) {
		*S = 6;
	} else if (A < 0xffe0) {
		*S = 7;
		if (!RnW && A >= 0xffc0) {
			uint_fast16_t b = 1 << ((A >> 1) & 0x0f);
			if (A & 1) {
				sam_register |= b;
			} else {
				sam_register &= ~b;
			}
			update_from_register();
		}
	} else {
		*S = 2;
	}
	*ncycles = fast_cycle ? SAM_CPU_FAST_DIVISOR : SAM_CPU_SLOW_DIVISOR;
	if (fast_cycle != running_fast) {
		if (fast_cycle)
			*ncycles += 2 * (SAM_CPU_FAST_DIVISOR >> 1);
		else
			*ncycles += (SAM_CPU_FAST_DIVISOR >> 1);
		running_fast = fast_cycle;
	}
	return is_ram_access;
}

static void vdg_address_add(int n) {
	uint16_t new_B = vdg_address + n;
	if ((vdg_address ^ new_B) & 0x10) {
		vdg_xcount = (vdg_xcount + 1) % vdg_mod_xdiv;
		if (vdg_xcount != 0) {
			new_B -= 0x10;
		} else {
			if ((vdg_address ^ new_B) & 0x20) {
				vdg_ycount = (vdg_ycount + 1) % vdg_mod_ydiv;
				if (vdg_ycount != 0) {
					new_B -= 0x20;
				}
			}
		}
	}
	vdg_address = new_B;
}

void sam_vdg_hsync(void) {
	/* The top cleared bit will, if a transition to low occurs, increment
	 * the bits above it.  This dummy fetch will achieve the same effective
	 * result. */
	vdg_address_add(vdg_mod_add);
	vdg_address &= vdg_mod_clear;
}

void sam_vdg_fsync(void) {
	vdg_address = vdg_base;
	vdg_xcount = 0;
	vdg_ycount = 0;
}

/* Called with the number of bytes of video data required, this implements the
 * divide-by-X and divide-by-Y parts of the SAM video address counter.  Sets
 * 'V' to the base address of available data and returns the actual number of
 * bytes available.  As the next byte may not be sequential, continue calling
 * until all required data is fetched. */

int sam_vdg_bytes(int nbytes, uint16_t *V, _Bool *valid) {
	uint16_t b3_0 = vdg_address & 0xf;
	_Bool is_valid = !mpu_rate_fast;
	if (valid) *valid = is_valid;
	if (is_valid && V)
		*V = VRAM_TRANSLATE(vdg_address);
	if ((b3_0 + nbytes) < 16) {
		vdg_address += nbytes;
		return nbytes;
	}
	nbytes = 16 - b3_0;
	vdg_address_add(nbytes);
	return nbytes;
}

void sam_set_register(unsigned int value) {
	sam_register = value;
	update_from_register();
}

unsigned int sam_get_register(void) {
	return sam_register;
}

static void update_from_register(void) {
	int vdg_mode = sam_register & 7;
	vdg_base = (sam_register & 0x03f8) << 6;
	vdg_mod_xdiv = vdg_mod_xdivs[vdg_mode];
	vdg_mod_ydiv = vdg_mod_ydivs[vdg_mode];
	vdg_mod_add = vdg_mod_adds[vdg_mode];
	vdg_mod_clear = vdg_mod_clears[vdg_mode];

	int memory_size = (sam_register >> 13) & 3;
	ram_row_mask = ram_row_masks[memory_size];
	ram_col_shift = ram_col_shifts[memory_size];
	ram_col_mask = ram_col_masks[memory_size];
	ram_ras1_bit = ram_ras1_bits[memory_size];
	switch (memory_size) {
		case 0: /* 4K */
		case 1: /* 16K */
			ram_page_bit = 0;
			ram_ras1 = 0x8080;
			break;
		default:
		case 2:
		case 3: /* 64K */
			ram_page_bit = (sam_register & 0x0400) << 5;
			ram_ras1 = 0;
			break;
	}

	map_type_1 = ((sam_register & 0x8000) != 0);
	mpu_rate_fast = sam_register & 0x1000;
	mpu_rate_ad = !map_type_1 && (sam_register & 0x800);
}
