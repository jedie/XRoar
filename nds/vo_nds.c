/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2011  Ciaran Anscomb
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdlib.h>
#include <string.h>
#include <nds.h>

#include "types.h"
#include "logging.h"
#include "machine.h"
#include "module.h"
#include "sam.h"
#include "vdg.h"
#include "xroar.h"

static int init(void);
static void shutdown(void);
static void vsync(void);
static void set_mode(unsigned int mode);
static void render_sg4(void);
static void render_cg1(void);
static void render_cg2(void);

VideoModule video_nds_module = {
	{ "nds", "NDS video",
	  init, 0, shutdown },
	NULL, NULL, 0,
	vsync, set_mode,
	NULL, NULL, NULL
};

/* This is now implemented a set of 12 64x64 sprites pulling tiles
 * from a 2D set of 32x24.  As sprites, 4-bit colour can be used. */

/* Pointer into Object Attribute Memory in convenient (libnds-defined) format.
 * OAM should only be written to during blanking. */
#define oam ((volatile OAMTable *)OAM)

/* Sprite memory.  libnds defines SPRITE_GFX as (uint16_t *), but we want
 * to write in 32-bit words. */
#define SPRITE_VRAM_BASE ((uint32_t *)0x6400000)

/* Prerendered data for SG4 and SG6 (2 CSS each) */
static uint32_t vdg_alpha_nds[4][12][256];
static int text_mode = 0;
/* Prerendered pixels for CG1 (2 CSS) */
static uint32_t cg1_pixels[2][16];
/* Prerendered pixels for RG1 (2 CSS) */
static uint32_t rg1_pixels[2][16];
/* Prerendered pixels for CG2 (2 CSS) */
static uint32_t cg2_pixels[2][256];
/* Prerendered pixels for RG6 (2 CSS + 2 Artifact) */
static uint32_t rg6_pixels[4][256];

/* Pointer to current set of prerendered pixels */
static uint32_t *cg_colours;

static void (*render_screen)(void);

static void alloc_colours(void);
static void prerender_bitmaps(void);

#define GREEN    (0)
#define ORANGE   (7)
#define BLACK    (8)
#define D_GREEN  (12)
#define D_ORANGE (13)

/* vdg.c will set nds_update_screen = 1 at end of active area.  Until then,
 * when VCOUNT is triggered, keep setting it back a couple of lines.  This
 * should keep actual screen update in sync with emulated update. */

extern uint8_t scanline_data[6154];

extern int nds_update_screen;
static void vcount_handle(void) {
	if (!nds_update_screen) {
		REG_VCOUNT = 202;
	} else {
		nds_update_screen = 0;
		render_screen();
	}
}

static int init(void) {
	int i, j;
	LOG_DEBUG(2,"Initialising NDS video driver\n");

	videoSetMode(MODE_5_2D | DISPLAY_SPR_ACTIVE | DISPLAY_SPR_2D);
	vramSetBankE(VRAM_E_MAIN_SPRITE);

	while (!(REG_DISPSTAT & DISP_IN_VBLANK));  /* do this during vblank */
	/* Create 12 sprites to cover screen */
	for (j = 0; j < 3; j++) {
		for (i = 0; i < 4; i++) {
			oam->oamBuffer[j*4+i].attribute[0] = OBJ_Y(j*64) | ATTR0_SQUARE;
			oam->oamBuffer[j*4+i].attribute[1] = OBJ_X(i*64) | ATTR1_SIZE_64;
			oam->oamBuffer[j*4+i].attribute[2] = j*256+i*8;
		}
	}
	/* Disable remaining sprites */
	for (i = 12; i < SPRITE_COUNT; i++) {
		oam->oamBuffer[i].attribute[0] = ATTR0_DISABLED;
		oam->oamBuffer[i].attribute[1] = 0;
		oam->oamBuffer[i].attribute[2] = 0;
	}

	alloc_colours();
	prerender_bitmaps();
	set_mode(0);

	/* Set up VCOUNT handler */
	SetYtrigger(204);
	irqSet(IRQ_VCOUNT, vcount_handle);
	return 0;
}

static void shutdown(void) {
	LOG_DEBUG(2,"Shutting down NDS video driver\n");
}

static void vsync(void) {
}

/* Update graphics mode - change current select colour set */
static void set_mode(unsigned int mode) {
	switch ((mode & 0xf0) >> 4) {
		case 0: case 2: case 4: case 6:
		case 1: case 3: case 5: case 7:
			render_screen = render_sg4;
			text_mode = (mode & 0x18) >> 3;
			break;
		case 8:
			render_screen = render_cg1;
			cg_colours = cg1_pixels[(mode & 0x08) >> 3];
			break;
		case 9: case 11: case 13:
			render_screen = render_cg1;
			cg_colours = rg1_pixels[(mode & 0x08) >> 3];
			break;
		case 10: case 12: case 14:
			render_screen = render_cg2;
			cg_colours = cg2_pixels[(mode & 0x08) >> 3];
			break;
		case 15: default:
			render_screen = render_cg2;
			if ((mode & 0x08) && running_config.cross_colour_phase) {
				cg_colours = rg6_pixels[1 + running_config.cross_colour_phase];
			} else {
				cg_colours = rg6_pixels[(mode & 0x08)>>3];
			}
			break;
	}
}

#define MAP_COLOUR(r,g,b) RGB15(r>>3,g>>3,b>>3)

/* Allocate colours */
static void alloc_colours(void) {
	BG_PALETTE[0] = MAP_COLOUR(0x00, 0xff, 0x00);
	SPRITE_PALETTE[0] = MAP_COLOUR(0x00, 0xff, 0x00);
	SPRITE_PALETTE[1] = MAP_COLOUR(0xff, 0xff, 0x00);
	SPRITE_PALETTE[2] = MAP_COLOUR(0x00, 0x00, 0xff);
	SPRITE_PALETTE[3] = MAP_COLOUR(0xff, 0x00, 0x00);
	SPRITE_PALETTE[4] = MAP_COLOUR(0xff, 0xe0, 0xe0);
	SPRITE_PALETTE[5] = MAP_COLOUR(0x00, 0xff, 0xff);
	SPRITE_PALETTE[6] = MAP_COLOUR(0xff, 0x00, 0xff);
	SPRITE_PALETTE[7] = MAP_COLOUR(0xff, 0xa5, 0x00);
	SPRITE_PALETTE[8] = MAP_COLOUR(0x00, 0x00, 0x00);
	SPRITE_PALETTE[9] = MAP_COLOUR(0x00, 0x80, 0xff);
	SPRITE_PALETTE[10] = MAP_COLOUR(0xff, 0x80, 0x00);
	SPRITE_PALETTE[11] = MAP_COLOUR(0xff, 0xff, 0xff);
	SPRITE_PALETTE[12] = MAP_COLOUR(0x00, 0x20, 0x00);
	SPRITE_PALETTE[13] = MAP_COLOUR(0x20, 0x14, 0x00);
}

/**************************************************************************/

/* Renders a line of alphanumeric/semigraphics 4 (mode is selected by data
 * line, so need to be handled together).  Works with prerendered data,
 * so this is good for semigraphics 6 too. */
static void render_sg4(void) {
	uint32_t *pixel = SPRITE_VRAM_BASE;
	uint8_t *vram = scanline_data;
	int t, line, i;
	int subline = 0;
	unsigned int octet;
	for (t = 0; t < 24; t++) {
		for (line = 0; line < 8; line++) {
			for (i = 0; i < 32; i++) {
				octet = *(vram++);
				*(pixel) = vdg_alpha_nds[text_mode][subline][octet];
				pixel += 8;
			}
			pixel -= 255;
			subline++;
			if (subline >= 12)
				subline = 0;
		}
		pixel += 248;
	}
}

/* Render a 16-byte graphics line (CG1, RG1, RG2, & RG3) */
static void render_cg1(void) {
	uint32_t *pixel = SPRITE_VRAM_BASE;
	uint8_t *vram = scanline_data;
	int t, line, i;
	unsigned int octet;
	for (t = 0; t < 24; t++) {
		for (line = 0; line < 8; line++) {
			for (i = 0; i < 16; i++) {
				octet = *(vram++);
				*(pixel) = cg_colours[octet >> 4];
				pixel += 8;
				*(pixel) = cg_colours[octet & 15];
				pixel += 8;
			}
			pixel -= 255;
		}
		pixel += 248;
	}
}

/* Render a 32-byte graphics line (CG2, CG3, CG6 & RG6) */
static void render_cg2(void) {
	uint32_t *pixel = SPRITE_VRAM_BASE;
	uint8_t *vram = scanline_data;
	int t, line, i;
	unsigned int octet;
	for (t = 0; t < 24; t++) {
		for (line = 0; line < 8; line++) {
			for (i = 0; i < 32; i++) {
				octet = *(vram++);
				*(pixel) = cg_colours[octet];
				pixel += 8;
			}
			pixel -= 255;
		}
		pixel += 248;
	}
}

/**************************************************************************/

/* Prerender all the semigraphics image data.  Should probably move this
 * out to generated source file. */

static int rg6a_colours[2][4] = {
	{ 8, 9, 10, 11 },
	{ 8, 10, 9, 11 }
};
static int fg_alpha[2] = { GREEN, ORANGE };
static int bg_alpha[2] = { D_GREEN, D_ORANGE };

static void prerender_bitmaps(void) {
	int screen, c, i;

	/* SG4 */
	for (screen = 0; screen < 2; screen++) {
		for (c = 0; c < 128; c++) {
			int row;
			for (row = 0; row < 12; row++) {
				int b = vdg_alpha[(c&0x3f) * 12 + row];
				if (c & 0x40) b = ~b;
				vdg_alpha_nds[screen][row][c] = (((b & 0x01) ? fg_alpha[screen] : bg_alpha[screen]) << 28)
					| (((b & 0x02) ? fg_alpha[screen] : bg_alpha[screen]) << 24)
					| (((b & 0x04) ? fg_alpha[screen] : bg_alpha[screen]) << 20)
					| (((b & 0x08) ? fg_alpha[screen] : bg_alpha[screen]) << 16)
					| (((b & 0x10) ? fg_alpha[screen] : bg_alpha[screen]) << 12)
					| (((b & 0x20) ? fg_alpha[screen] : bg_alpha[screen]) << 8)
					| (((b & 0x40) ? fg_alpha[screen] : bg_alpha[screen]) << 4)
					| ((b & 0x80) ? fg_alpha[screen] : bg_alpha[screen]);
			}
		}
		for (c = 128; c < 256; c++) {
			uint32_t fg = (c & 0x70) >> 4;
			uint32_t bg = (BLACK << 12) | (BLACK << 8)
				| (BLACK << 4) | BLACK;
			int row;
			fg = (fg << 12) | (fg << 8) | (fg << 4) | fg;
			for (row = 0; row < 6; row++) {
				vdg_alpha_nds[screen][row][c] = (c & 0x04) ? (fg << 16) : (bg << 16);
				vdg_alpha_nds[screen][row][c] |= (c & 0x08) ? fg : bg;
			}
			for (row = 6; row < 12; row++) {
				vdg_alpha_nds[screen][row][c] = (c & 0x01) ? (fg << 16) : (bg << 16);
				vdg_alpha_nds[screen][row][c] |= (c & 0x02) ? fg : bg;
			}
		}
	}

	/* SG6 */
	for (screen = 0; screen < 2; screen++) {
		for (c = 0; c < 128; c++) {
			int row;
			for (row = 0; row < 12; row++) {
				int b = c;
				if (c & 0x40) b = ~b;
				vdg_alpha_nds[screen+2][row][c] = (((b & 0x01) ? fg_alpha[screen] : bg_alpha[screen]) << 28)
					| (((b & 0x02) ? fg_alpha[screen] : bg_alpha[screen]) << 24)
					| (((b & 0x04) ? fg_alpha[screen] : bg_alpha[screen]) << 20)
					| (((b & 0x08) ? fg_alpha[screen] : bg_alpha[screen]) << 16)
					| (((b & 0x10) ? fg_alpha[screen] : bg_alpha[screen]) << 12)
					| (((b & 0x20) ? fg_alpha[screen] : bg_alpha[screen]) << 8)
					| (((b & 0x40) ? fg_alpha[screen] : bg_alpha[screen]) << 4)
					| ((b & 0x80) ? fg_alpha[screen] : bg_alpha[screen]);
			}
		}
		for (c = 128; c < 256; c++) {
			uint32_t fg = (c&0x70)>>4;
			uint32_t bg = (BLACK << 12) | (BLACK << 8)
				| (BLACK << 4) | BLACK;
			int row;
			fg = (fg << 12) | (fg << 8) | (fg << 4) | fg;
			for (row = 0; row < 4; row++) {
				vdg_alpha_nds[screen+2][row][c] = (c & 0x10) ? (fg << 16) : (bg << 16);
				vdg_alpha_nds[screen+2][row][c] |= (c & 0x20) ? fg : bg;
			}
			for (row = 4; row < 8; row++) {
				vdg_alpha_nds[screen+2][row][c] = (c & 0x04) ? (fg << 16) : (bg << 16);
				vdg_alpha_nds[screen+2][row][c] |= (c & 0x08) ? fg : bg;
			}
			for (row = 8; row < 12; row++) {
				vdg_alpha_nds[screen+2][row][c] = (c & 0x01) ? (fg << 16) : (bg << 16);
				vdg_alpha_nds[screen+2][row][c] |= (c & 0x02) ? fg : bg;
			}
		}
	}

	/* CG1 */
	for (screen = 0; screen < 2; screen++) {
		for (i = 0; i < 16; i++) {
			cg1_pixels[screen][i] =
				((((i>>2)&3)+screen*4) << 0)
				| ((((i>>2)&3)+screen*4) << 4)
				| ((((i>>2)&3)+screen*4) << 8)
				| ((((i>>2)&3)+screen*4) << 12)
				| (((i&3)+screen*4) << 16)
				| (((i&3)+screen*4) << 20)
				| (((i&3)+screen*4) << 24)
				| (((i&3)+screen*4) << 28);
		}
	}

	/* CG2 */
	for (screen = 0; screen < 2; screen++) {
		for (i = 0; i < 256; i++) {
			cg2_pixels[screen][i] =
				((((i>>6)&3)+screen*4) << 0)
				| ((((i>>6)&3)+screen*4) << 4)
				| ((((i>>4)&3)+screen*4) << 8)
				| ((((i>>4)&3)+screen*4) << 12)
				| ((((i>>2)&3)+screen*4) << 16)
				| ((((i>>2)&3)+screen*4) << 20)
				| (((i&3)+screen*4) << 24)
				| (((i&3)+screen*4) << 28);
		}
	}

	/* RG1 */
	for (i = 0; i < 16; i++) {
		rg1_pixels[0][i] =
			((i&8) ? (0<<0) : (8<<0))
			| ((i&8) ? (0<<4) : (8<<4))
			| ((i&4) ? (0<<8) : (8<<8))
			| ((i&4) ? (0<<12) : (8<<12))
			| ((i&2) ? (0<<16) : (8<<16))
			| ((i&2) ? (0<<20) : (8<<20))
			| ((i&1) ? (0<<24) : (8<<24))
			| ((i&1) ? (0<<28) : (8<<28));
		rg1_pixels[1][i] =
			((i&8) ? (4<<0) : (8<<0))
			| ((i&8) ? (4<<4) : (8<<4))
			| ((i&4) ? (4<<8) : (8<<8))
			| ((i&4) ? (4<<12) : (8<<12))
			| ((i&2) ? (4<<16) : (8<<16))
			| ((i&2) ? (4<<20) : (8<<20))
			| ((i&1) ? (4<<24) : (8<<24))
			| ((i&1) ? (4<<28) : (8<<28));
	}

	/* RG6 */
	for (screen = 0; screen < 2; screen++) {
		for (i = 0; i < 256; i++) {
			rg6_pixels[screen][i] =
				((i&128) ? ((screen*4)<<0) : (8<<0))
				| ((i&64) ? ((screen*4)<<4) : (8<<4))
				| ((i&32) ? ((screen*4)<<8) : (8<<8))
				| ((i&16) ? ((screen*4)<<12) : (8<<12))
				| ((i&8) ? ((screen*4)<<16) : (8<<16))
				| ((i&4) ? ((screen*4)<<20) : (8<<20))
				| ((i&2) ? ((screen*4)<<24) : (8<<24))
				| ((i&1) ? ((screen*4)<<28) : (8<<28));
		}
	}

	/* RG6 artifacted */
	for (screen = 0; screen < 2; screen++) {
		for (i = 0; i < 256; i++) {
			rg6_pixels[screen+2][i] =
				(rg6a_colours[screen][(i>>6)&3])
				| (rg6a_colours[screen][(i>>6)&3] << 4)
				| (rg6a_colours[screen][(i>>4)&3] << 8)
				| (rg6a_colours[screen][(i>>4)&3] << 12)
				| (rg6a_colours[screen][(i>>2)&3] << 16)
				| (rg6a_colours[screen][(i>>2)&3] << 20)
				| (rg6a_colours[screen][i&3] << 24)
				| (rg6a_colours[screen][i&3] << 28);
		}
	}
}
