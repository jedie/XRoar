/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2007  Ciaran Anscomb
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdlib.h>
#include <string.h>
#include <nds.h>

#include "types.h"
#include "logging.h"
#include "module.h"
#include "sam.h"
#include "vdg.h"
#include "xroar.h"

static int init(int argc, char **argv);
static void shutdown(void);
static void vsync(void);
static void set_mode(unsigned int mode);
static void render_sg4(void);
static void render_cg1(void);
static void render_rg1(void);
static void render_cg2(void);

VideoModule video_nds_module = {
	{ "nds", "NDS video",
	  init, 0, shutdown, NULL },
	NULL, NULL, 0,
	vsync, set_mode,
	render_sg4, render_sg4, render_cg1,
	render_rg1, render_cg2, render_cg2,
	NULL
};

static uint16_t darkgreen, black;
static uint16_t bg_colour;
static uint16_t fg_colour;
static uint16_t vdg_colour[16];
static uint16_t *cg_colours;

static int map_colour(int r, int g, int b);
static void alloc_colours(void);

/* Prerendered data for SG4 and SG6 (2 CSS each) */
/* Indexed by: text_mode, char, subline, x */
static uint16_t vdg_alpha_nds[4][256][12][4];
static int text_mode = 0;
/* Prerendered pixel pairs for CG2 (2 CSS) */
static uint16_t cg2_pixels[2][4];
/* Prerendered pixel pairs for RG6 (2 CSS + 2 Artifact) */
static uint16_t rg6_pixels[4][4];

static void prerender_bitmaps(void);

static unsigned int subline;
static uint16_t *pixel;

static int init(int argc, char **argv) {
	(void)argc;
	(void)argv;
	LOG_DEBUG(2,"Initialising NDS video driver\n");
	videoSetMode(MODE_5_2D | DISPLAY_BG2_ACTIVE);
	vramSetBankA(VRAM_A_MAIN_BG_0x06000000);
	BG2_CR = (BG_BMP16_256x256 | BIT(7)) & ~BIT(2);
	BG2_XDX = 1 << 8;
	BG2_XDY = 0;
	BG2_YDX = 0;
	BG2_YDY = 1 << 8;
	BG2_CX = 0;
	BG2_CY = 0;
	alloc_colours();
	prerender_bitmaps();
	return 0;
}

static void shutdown(void) {
	LOG_DEBUG(2,"Shutting down NDS video driver\n");
}

static void vsync(void) {
	pixel = VRAM_0;
	subline = 0;
}

/* Update graphics mode - change current select colour set */
static void set_mode(unsigned int mode) {
	if (mode & 0x80) {
		/* Graphics modes */
		if ((mode & 0x70) == 0x70) {
			if (mode & 0x08) {
				cg_colours = rg6_pixels[1 + video_artifact_mode];
			} else {
				cg_colours = rg6_pixels[0];
			}
		} else {
			cg_colours = cg2_pixels[(mode & 0x08) >> 1];
			fg_colour = cg_colours[0];
			bg_colour = vdg_colour[8] | (vdg_colour[8] << 8);
		}
	} else {
		text_mode = (mode & 0x18) >> 3;
	}
}

/* DS backgrounds can be palletised 8-bit, but writes still need to be
 * 16 bits wide, so all these routines deal in pairs of pixels. */

/* Renders a line of alphanumeric/semigraphics 4 (mode is selected by data
 * line, so need to be handled together).  Works with prerendered data,
 * so this is good for semigraphics 6 too. */
static void render_sg4(void) {
	uint16_t *bitmap;
	uint8_t *vram;
	int line, i, j;
	unsigned int octet;
	for (line = 0; line < 4; line++) {
		for (i = 2; i; i--) {
			vram = (uint8_t *)sam_vram_ptr(sam_vdg_address);
			for (j = 16; j; j--) {
				octet = *(vram++);
				bitmap = vdg_alpha_nds[text_mode][octet][subline];
				*(pixel++) = *(bitmap++);
				*(pixel++) = *(bitmap++);
				*(pixel++) = *(bitmap++);
				*(pixel++) = *(bitmap++);
			}
			sam_vdg_xstep(16);
		}
		sam_vdg_hsync(10);
		subline++;
	}
	if (subline > 11)
		subline = 0;
}

/* Render a 16-byte colour graphics line (CG1) */
static void render_cg1(void) {
	uint8_t *vram = (uint8_t *)sam_vram_ptr(sam_vdg_address);
	int line, i;
	unsigned int octet;
	for (line = 0; line < 4; line++) {
		for (i = 16; i; i--) {
			octet = *(vram++);
			*(pixel++) = cg_colours[octet >> 6];
			*(pixel++) = cg_colours[octet >> 6];
			*(pixel++) = cg_colours[(octet >> 4) & 0x03];
			*(pixel++) = cg_colours[(octet >> 4) & 0x03];
			*(pixel++) = cg_colours[(octet >> 2) & 0x03];
			*(pixel++) = cg_colours[(octet >> 2) & 0x03];
			*(pixel++) = cg_colours[octet & 0x03];
			*(pixel++) = cg_colours[octet & 0x03];
		}
		sam_vdg_xstep(16);
		sam_vdg_hsync(6);
	}
	subline += 4;
	if (subline > 11)
		subline = 0;
}

/* Render a 16-byte resolution graphics line (RG1,RG2,RG3) */
static void render_rg1(void) {
	uint8_t *vram = (uint8_t *)sam_vram_ptr(sam_vdg_address);
	int line, i;
	unsigned int octet;
	for (line = 0; line < 4; line++) {
		for (i = 16; i; i--) {
			octet = *(vram++);
			*(pixel++) = (octet & 0x80) ? fg_colour : bg_colour;
			*(pixel++) = (octet & 0x80) ? fg_colour : bg_colour;
			*(pixel++) = (octet & 0x40) ? fg_colour : bg_colour;
			*(pixel++) = (octet & 0x40) ? fg_colour : bg_colour;
			*(pixel++) = (octet & 0x20) ? fg_colour : bg_colour;
			*(pixel++) = (octet & 0x20) ? fg_colour : bg_colour;
			*(pixel++) = (octet & 0x10) ? fg_colour : bg_colour;
			*(pixel++) = (octet & 0x10) ? fg_colour : bg_colour;
			*(pixel++) = (octet & 0x08) ? fg_colour : bg_colour;
			*(pixel++) = (octet & 0x08) ? fg_colour : bg_colour;
			*(pixel++) = (octet & 0x04) ? fg_colour : bg_colour;
			*(pixel++) = (octet & 0x04) ? fg_colour : bg_colour;
			*(pixel++) = (octet & 0x02) ? fg_colour : bg_colour;
			*(pixel++) = (octet & 0x02) ? fg_colour : bg_colour;
			*(pixel++) = (octet & 0x01) ? fg_colour : bg_colour;
			*(pixel++) = (octet & 0x01) ? fg_colour : bg_colour;
		}
		sam_vdg_xstep(16);
		sam_vdg_hsync(6);
	}
	subline += 4;
	if (subline > 11)
		subline = 0; 
}

/* Render a 32-byte colour graphics line (CG2,CG3,CG6) */
/* Draws pre-computed "pixel pairs", so this gets used for RG6 too. */
static void render_cg2(void) {
	uint8_t *vram;
	int line, i, j;
	unsigned int octet;
	for (line = 0; line < 4; line++) {
		for (i = 2; i; i--) {
			vram = (uint8_t *)sam_vram_ptr(sam_vdg_address);
			for (j = 16; j; j--) {
				octet = *(vram++);
				*(pixel++) = cg_colours[octet >> 6];
				*(pixel++) = cg_colours[(octet >> 4) & 0x03];
				*(pixel++) = cg_colours[(octet >> 2) & 0x03];
				*(pixel++) = cg_colours[octet & 0x03];
			}
			sam_vdg_xstep(16);
		}
		sam_vdg_hsync(10);
	}
	subline += 4;
	if (subline > 11)
		subline = 0;
}

/**************************************************************************/

static int map_colour(int r, int g, int b) {
	static int last = 1;
	int i;
	uint16_t c = RGB15(r>>3,g>>3,b>>3);
	for (i = 1; i < last; i++) {
		if (BG_PALETTE[i] == c) return i;
	}
	last++;
	BG_PALETTE[i] = c;
	return i;
}

/* Allocate colours */
static void alloc_colours(void) {
	vdg_colour[0] = map_colour(0x00, 0xff, 0x00);
	vdg_colour[1] = map_colour(0xff, 0xff, 0x00);
	vdg_colour[2] = map_colour(0x00, 0x00, 0xff);
	vdg_colour[3] = map_colour(0xff, 0x00, 0x00);
	vdg_colour[4] = map_colour(0xff, 0xe0, 0xe0);
	vdg_colour[5] = map_colour(0x00, 0xff, 0xff);
	vdg_colour[6] = map_colour(0xff, 0x00, 0xff);
	vdg_colour[7] = map_colour(0xff, 0xa5, 0x00);
	vdg_colour[8] = map_colour(0x00, 0x00, 0x00);
	vdg_colour[9] = map_colour(0x00, 0x80, 0xff);
	vdg_colour[10] = map_colour(0xff, 0x80, 0x00);
	vdg_colour[11] = map_colour(0xff, 0xff, 0xff);
	vdg_colour[12] = map_colour(0x00, 0x00, 0x00);
	vdg_colour[13] = map_colour(0xff, 0x80, 0x00);
	vdg_colour[14] = map_colour(0x00, 0x80, 0xff);
	vdg_colour[15] = map_colour(0xff, 0xff, 0xff);
	black = map_colour(0x00, 0x00, 0x00);
	darkgreen = map_colour(0x00, 0x20, 0x00);
}

/* Prerender all the semigraphics image data.  Should probably move this
 * out to generated source file. */
static void prerender_bitmaps(void) {
	uint8_t fgcolour[2] = { vdg_colour[0], vdg_colour[7] };
	uint8_t bgcolour[2] = { darkgreen, darkgreen };
	int screen;
	/* SG4 */
	for (screen = 0; screen < 2; screen++) {
		int c;
		for (c = 0; c < 128; c++) {
			uint16_t *dest = vdg_alpha_nds[screen][c][0];
			int row;
			for (row = 0; row < 12; row++) {
				int b = vdg_alpha[(c&0x3f) * 12 + row];
				int i;
				if (c & 0x40) b = ~b;
				for (i = 0; i < 4; i++) {
					*(dest++) = (((b & 0x40) ? fgcolour[screen] : bgcolour[screen]) << 8)
						| ((b & 0x80) ? fgcolour[screen] : bgcolour[screen]);
					b <<= 2;
				}
			}
		}
		for (c = 128; c < 256; c++) {
			uint16_t *dest = vdg_alpha_nds[screen][c][0];
			uint16_t fg = vdg_colour[(c&0x70)>>4];
			uint16_t bg = (black << 8) | black;
			int row;
			fg = (fg<<8) | fg;
			for (row = 0; row < 6; row++) {
				*(dest++) = (c & 0x08) ? fg: bg;
				*(dest++) = (c & 0x08) ? fg: bg;
				*(dest++) = (c & 0x04) ? fg: bg;
				*(dest++) = (c & 0x04) ? fg: bg;
			}
			for (row = 6; row < 12; row++) {
				*(dest++) = (c & 0x02) ? fg: bg;
				*(dest++) = (c & 0x02) ? fg: bg;
				*(dest++) = (c & 0x01) ? fg: bg;
				*(dest++) = (c & 0x01) ? fg: bg;
			}
		}
	}
	/* SG6 */
	for (screen = 0; screen < 2; screen++) {
		int c;
		for (c = 0; c < 128; c++) {
			uint16_t *dest = vdg_alpha_nds[screen+2][c][0];
			int row;
			for (row = 0; row < 12; row++) {
				int b = c;
				int i;
				if (c & 0x40) b = ~b;
				for (i = 0; i < 4; i++) {
					*(dest++) = (((b & 0x40) ? fgcolour[screen] : bgcolour[screen]) << 8)
						| ((b & 0x80) ? fgcolour[screen] : bgcolour[screen]);
					b <<= 2;
				}
			}
		}
		for (c = 128; c < 256; c++) {
			uint16_t *dest = vdg_alpha_nds[screen+2][c][0];
			uint16_t fg = vdg_colour[(c&0x70)>>4];
			uint16_t bg = (black << 8) | black;
			int row;
			fg = (fg<<8) | fg;
			for (row = 0; row < 4; row++) {
				*(dest++) = (c & 0x20) ? fg: bg;
				*(dest++) = (c & 0x20) ? fg: bg;
				*(dest++) = (c & 0x10) ? fg: bg;
				*(dest++) = (c & 0x10) ? fg: bg;
			}
			for (row = 4; row < 8; row++) {
				*(dest++) = (c & 0x08) ? fg: bg;
				*(dest++) = (c & 0x08) ? fg: bg;
				*(dest++) = (c & 0x04) ? fg: bg;
				*(dest++) = (c & 0x04) ? fg: bg;
			}
			for (row = 8; row < 12; row++) {
				*(dest++) = (c & 0x02) ? fg: bg;
				*(dest++) = (c & 0x02) ? fg: bg;
				*(dest++) = (c & 0x01) ? fg: bg;
				*(dest++) = (c & 0x01) ? fg: bg;
			}
		}
	}
	/* CG2 */
	cg2_pixels[0][0] = vdg_colour[0] | (vdg_colour[0] << 8);
	cg2_pixels[0][1] = vdg_colour[1] | (vdg_colour[1] << 8);
	cg2_pixels[0][2] = vdg_colour[2] | (vdg_colour[2] << 8);
	cg2_pixels[0][3] = vdg_colour[3] | (vdg_colour[3] << 8);
	cg2_pixels[1][0] = vdg_colour[4] | (vdg_colour[4] << 8);
	cg2_pixels[1][1] = vdg_colour[5] | (vdg_colour[5] << 8);
	cg2_pixels[1][2] = vdg_colour[6] | (vdg_colour[6] << 8);
	cg2_pixels[1][3] = vdg_colour[7] | (vdg_colour[7] << 8);
	/* RG6 */
	rg6_pixels[0][0] = black | (black << 8);
	rg6_pixels[0][1] = black | (vdg_colour[0] << 8);
	rg6_pixels[0][2] = vdg_colour[0] | (black << 8);
	rg6_pixels[0][3] = vdg_colour[0] | (vdg_colour[0] << 8);
	rg6_pixels[1][0] = black | (black << 8);
	rg6_pixels[1][1] = black | (vdg_colour[4] << 8);
	rg6_pixels[1][2] = vdg_colour[4] | (black << 8);
	rg6_pixels[1][3] = vdg_colour[4] | (vdg_colour[4] << 8);
	/* RG6 artifacted */
	rg6_pixels[2][0] = vdg_colour[8] | (vdg_colour[8] << 8);
	rg6_pixels[2][1] = vdg_colour[9] | (vdg_colour[9] << 8);
	rg6_pixels[2][2] = vdg_colour[10] | (vdg_colour[10] << 8);
	rg6_pixels[2][3] = vdg_colour[11] | (vdg_colour[11] << 8);
	rg6_pixels[3][0] = vdg_colour[12] | (vdg_colour[12] << 8);
	rg6_pixels[3][1] = vdg_colour[13] | (vdg_colour[13] << 8);
	rg6_pixels[3][2] = vdg_colour[14] | (vdg_colour[14] << 8);
	rg6_pixels[3][3] = vdg_colour[15] | (vdg_colour[15] << 8);
}
