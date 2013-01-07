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

/* This file contains generic scanline rendering routines.  It is included
 * into various video module source files and makes use of macros defined in
 * those files (eg, LOCK_SURFACE and XSTEP) */

#include "config.h"

#include "machine.h"
#include "module.h"
#include "vdg_palette.h"

static Pixel *pixel;
static Pixel vdg_colour[12];
static Pixel artifact_5bit[2][32];
static Pixel artifact_simple[2][4];
static int phase = 0;

/* Map VDG palette entry */
static Pixel map_palette_entry(int i) {
	float R, G, B;
	_Bool is_pal = (xroar_machine_config->tv_standard == TV_PAL);
	vdg_palette_RGB(xroar_vdg_palette, is_pal, i, &R, &G, &B);
	R *= 255.0;
	G *= 255.0;
	B *= 255.0;
	return MAPCOLOUR((int)R, (int)G, (int)B);
}

/* Allocate colours */
static void alloc_colours(void) {
	int i;
#ifdef RESET_PALETTE
	RESET_PALETTE();
#endif
	for (i = 0; i < 12; i++) {
		vdg_colour[i] = map_palette_entry(i);
	}

	artifact_simple[0][0] = MAPCOLOUR(0x00, 0x00, 0x00);
	artifact_simple[0][1] = MAPCOLOUR(0x00, 0x80, 0xff);
	artifact_simple[0][2] = MAPCOLOUR(0xff, 0x80, 0x00);
	artifact_simple[0][3] = MAPCOLOUR(0xff, 0xff, 0xff);
	artifact_simple[1][0] = MAPCOLOUR(0x00, 0x00, 0x00);
	artifact_simple[1][1] = MAPCOLOUR(0xff, 0x80, 0x00);
	artifact_simple[1][2] = MAPCOLOUR(0x00, 0x80, 0xff);
	artifact_simple[1][3] = MAPCOLOUR(0xff, 0xff, 0xff);

	artifact_5bit[0][0x00] = MAPCOLOUR(0x00, 0x00, 0x00);
	artifact_5bit[0][0x01] = MAPCOLOUR(0x00, 0x00, 0x00);
	artifact_5bit[0][0x02] = MAPCOLOUR(0x00, 0x32, 0x78);
	artifact_5bit[0][0x03] = MAPCOLOUR(0x00, 0x28, 0x00);
	artifact_5bit[0][0x04] = MAPCOLOUR(0xff, 0x8c, 0x64);
	artifact_5bit[0][0x05] = MAPCOLOUR(0xff, 0x8c, 0x64);
	artifact_5bit[0][0x06] = MAPCOLOUR(0xff, 0xd2, 0xff);
	artifact_5bit[0][0x07] = MAPCOLOUR(0xff, 0xf0, 0xc8);
	artifact_5bit[0][0x08] = MAPCOLOUR(0x00, 0x32, 0x78);
	artifact_5bit[0][0x09] = MAPCOLOUR(0x00, 0x00, 0x3c);
	artifact_5bit[0][0x0a] = MAPCOLOUR(0x00, 0x80, 0xff);
	artifact_5bit[0][0x0b] = MAPCOLOUR(0x00, 0x80, 0xff);
	artifact_5bit[0][0x0c] = MAPCOLOUR(0xd2, 0xff, 0xd2);
	artifact_5bit[0][0x0d] = MAPCOLOUR(0xff, 0xff, 0xff);
	artifact_5bit[0][0x0e] = MAPCOLOUR(0x64, 0xf0, 0xff);
	artifact_5bit[0][0x0f] = MAPCOLOUR(0xff, 0xff, 0xff);
	artifact_5bit[0][0x10] = MAPCOLOUR(0x3c, 0x00, 0x00);
	artifact_5bit[0][0x11] = MAPCOLOUR(0x3c, 0x00, 0x00);
	artifact_5bit[0][0x12] = MAPCOLOUR(0x00, 0x00, 0x00);
	artifact_5bit[0][0x13] = MAPCOLOUR(0x00, 0x28, 0x00);
	artifact_5bit[0][0x14] = MAPCOLOUR(0xff, 0x80, 0x00);
	artifact_5bit[0][0x15] = MAPCOLOUR(0xff, 0x80, 0x00);
	artifact_5bit[0][0x16] = MAPCOLOUR(0xff, 0xff, 0xff);
	artifact_5bit[0][0x17] = MAPCOLOUR(0xff, 0xf0, 0xc8);
	artifact_5bit[0][0x18] = MAPCOLOUR(0x28, 0x00, 0x28);
	artifact_5bit[0][0x19] = MAPCOLOUR(0x28, 0x00, 0x28);
	artifact_5bit[0][0x1a] = MAPCOLOUR(0x00, 0x80, 0xff);
	artifact_5bit[0][0x1b] = MAPCOLOUR(0x00, 0x80, 0xff);
	artifact_5bit[0][0x1c] = MAPCOLOUR(0xff, 0xf0, 0xc8);
	artifact_5bit[0][0x1d] = MAPCOLOUR(0xff, 0xf0, 0xc8);
	artifact_5bit[0][0x1e] = MAPCOLOUR(0xff, 0xff, 0xff);
	artifact_5bit[0][0x1f] = MAPCOLOUR(0xff, 0xff, 0xff);

	artifact_5bit[1][0x00] = MAPCOLOUR(0x00, 0x00, 0x00);
	artifact_5bit[1][0x01] = MAPCOLOUR(0x00, 0x00, 0x00);
	artifact_5bit[1][0x02] = MAPCOLOUR(0xb4, 0x3c, 0x1e);
	artifact_5bit[1][0x03] = MAPCOLOUR(0x28, 0x00, 0x28);
	artifact_5bit[1][0x04] = MAPCOLOUR(0x46, 0xc8, 0xff);
	artifact_5bit[1][0x05] = MAPCOLOUR(0x46, 0xc8, 0xff);
	artifact_5bit[1][0x06] = MAPCOLOUR(0xd2, 0xff, 0xd2);
	artifact_5bit[1][0x07] = MAPCOLOUR(0x64, 0xf0, 0xff);
	artifact_5bit[1][0x08] = MAPCOLOUR(0xb4, 0x3c, 0x1e);
	artifact_5bit[1][0x09] = MAPCOLOUR(0x3c, 0x00, 0x00);
	artifact_5bit[1][0x0a] = MAPCOLOUR(0xff, 0x80, 0x00);
	artifact_5bit[1][0x0b] = MAPCOLOUR(0xff, 0x80, 0x00);
	artifact_5bit[1][0x0c] = MAPCOLOUR(0xff, 0xd2, 0xff);
	artifact_5bit[1][0x0d] = MAPCOLOUR(0xff, 0xff, 0xff);
	artifact_5bit[1][0x0e] = MAPCOLOUR(0xff, 0xf0, 0xc8);
	artifact_5bit[1][0x0f] = MAPCOLOUR(0xff, 0xff, 0xff);
	artifact_5bit[1][0x10] = MAPCOLOUR(0x00, 0x00, 0x3c);
	artifact_5bit[1][0x11] = MAPCOLOUR(0x00, 0x00, 0x3c);
	artifact_5bit[1][0x12] = MAPCOLOUR(0x00, 0x00, 0x00);
	artifact_5bit[1][0x13] = MAPCOLOUR(0x28, 0x00, 0x28);
	artifact_5bit[1][0x14] = MAPCOLOUR(0x00, 0x80, 0xff);
	artifact_5bit[1][0x15] = MAPCOLOUR(0x00, 0x80, 0xff);
	artifact_5bit[1][0x16] = MAPCOLOUR(0xff, 0xff, 0xff);
	artifact_5bit[1][0x17] = MAPCOLOUR(0x64, 0xf0, 0xff);
	artifact_5bit[1][0x18] = MAPCOLOUR(0x00, 0x28, 0x00);
	artifact_5bit[1][0x19] = MAPCOLOUR(0x00, 0x28, 0x00);
	artifact_5bit[1][0x1a] = MAPCOLOUR(0xff, 0x80, 0x00);
	artifact_5bit[1][0x1b] = MAPCOLOUR(0xff, 0x80, 0x00);
	artifact_5bit[1][0x1c] = MAPCOLOUR(0x64, 0xf0, 0xff);
	artifact_5bit[1][0x1d] = MAPCOLOUR(0x64, 0xf0, 0xff);
	artifact_5bit[1][0x1e] = MAPCOLOUR(0xff, 0xff, 0xff);
	artifact_5bit[1][0x1f] = MAPCOLOUR(0xff, 0xff, 0xff);
}

/* Render colour line using palette */
static void render_scanline(uint8_t *scanline_data) {
	scanline_data += ((VDG_tLB / 2) - 32);
	LOCK_SURFACE;
	for (int i = 320; i; i--) {
		*pixel = vdg_colour[*(scanline_data++)];
		pixel += XSTEP;
	}
	UNLOCK_SURFACE;
	pixel += NEXTLINE;
}

/* Render artifacted colours - simple 4-colour lookup */
static void render_ccr_simple(uint8_t *scanline_data) {
	int phase = xroar_machine_config->cross_colour_phase - 1;
	scanline_data += ((VDG_tLB / 2) - 32);
	LOCK_SURFACE;
	for (int i = 160; i; i--) {
		uint8_t c0 = *(scanline_data++);
		uint8_t c1 = *(scanline_data++);
		if (c0 == VDG_BLACK || c0 == VDG_WHITE) {
			int aindex = ((c0 != VDG_BLACK) ? 2 : 0)
			             | ((c1 != VDG_BLACK) ? 1 : 0);
			*pixel = *(pixel + 1) = artifact_simple[phase][aindex];
		} else {
			*pixel = vdg_colour[c0];
			*(pixel + 1) = vdg_colour[c1];
		}
		pixel += 2 * XSTEP;
	}
	UNLOCK_SURFACE;
	pixel += NEXTLINE;
}

/* Render artifacted colours - 5-bit lookup table */
static void render_ccr_5bit(uint8_t *scanline_data) {
	scanline_data += ((VDG_tLB / 2) - 32);
	LOCK_SURFACE;
	int aindex = 0;
	for (int i = -2; i < 2; i++) {
		aindex = (aindex << 1) & 31;
		if (*(scanline_data + i) != VDG_BLACK)
			aindex |= 1;
	}
	for (int i = 320; i; i--) {
		aindex = (aindex << 1) & 31;
		if (*(scanline_data + 2) != VDG_BLACK)
			aindex |= 1;
		uint8_t c = *(scanline_data++);
		if (c == VDG_BLACK || c == VDG_WHITE) {
			*pixel = artifact_5bit[phase][aindex];
		} else {
			*pixel = vdg_colour[c];
		}
		pixel += XSTEP;
		phase ^= 1;
	}
	UNLOCK_SURFACE;
	pixel += NEXTLINE;
}

static void update_cross_colour_phase(void) {
	if (xroar_machine_config->cross_colour_phase == CROSS_COLOUR_OFF) {
		phase = 0;
		video_module->render_scanline = render_scanline;
	} else {
		phase = xroar_machine_config->cross_colour_phase - 1;
		if (xroar_opt_ccr == CROSS_COLOUR_SIMPLE) {
			video_module->render_scanline = render_ccr_simple;
		} else {
			video_module->render_scanline = render_ccr_5bit;
		}
	}
}
