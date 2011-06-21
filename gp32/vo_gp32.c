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

#include <string.h>
#include <gpgraphic.h>
#include "gp32/gpgfx.h"

#include "types.h"
#include "machine.h"
#include "module.h"
#include "sam.h"
#include "xroar.h"

static int init(void);
static void shutdown(void);
static void vdg_vsync(void);
static void vdg_set_mode(unsigned int mode);
static void render_sg4(void);
static void render_cg1(void);
static void render_rg1(void);
static void render_cg2(void);
static void render_rg6(void);

VideoModule video_gp32_module = {
	{ "gp32", "GP32 video driver",
	  init, 0, shutdown },
	NULL, NULL, 0,
	vdg_vsync, vdg_set_mode,
	NULL, NULL
};

#define MAPCOLOUR GPGFX_MAPCOLOUR
#define NEXTLINE -76801
#define VIDEO_VIEWPORT_YOFFSET (-7)
#define LOCK_SURFACE
#define UNLOCK_SURFACE

static unsigned int subline;
static uint32_t *pixel;
static uint32_t darkgreen, black;
static uint8_t bg_colour;
static uint8_t fg_colour;
static uint8_t vdg_colour[16];
static uint8_t *cg_colours;
static uint32_t border_colour;

static unsigned int text_mode;

extern const uint8_t vdg_alpha_gp32[4][3][8192];

static int init(void) {
	vdg_colour[0] = MAPCOLOUR(0x00, 0xff, 0x00);
	vdg_colour[1] = MAPCOLOUR(0xff, 0xff, 0x00);
	vdg_colour[2] = MAPCOLOUR(0x00, 0x00, 0xff);
	vdg_colour[3] = MAPCOLOUR(0xff, 0x00, 0x00);
	vdg_colour[4] = MAPCOLOUR(0xff, 0xff, 0xff);
	vdg_colour[5] = MAPCOLOUR(0x00, 0xff, 0xff);
	vdg_colour[6] = MAPCOLOUR(0xff, 0x00, 0xff);
	vdg_colour[7] = MAPCOLOUR(0xff, 0xa5, 0x00);
	vdg_colour[8] = MAPCOLOUR(0x00, 0x00, 0x00);
	vdg_colour[9] = MAPCOLOUR(0x00, 0x00, 0xff);
	vdg_colour[10] = MAPCOLOUR(0xff, 0x00, 0x00);
	vdg_colour[11] = MAPCOLOUR(0xff, 0xff, 0xff);
	vdg_colour[12] = MAPCOLOUR(0x00, 0x00, 0x00);
	vdg_colour[13] = MAPCOLOUR(0xff, 0x00, 0x00);
	vdg_colour[14] = MAPCOLOUR(0x00, 0x00, 0xff);
	vdg_colour[15] = MAPCOLOUR(0xff, 0xff, 0xff);
	black = MAPCOLOUR(0x00, 0x00, 0x00);
	black = (black<<24) | (black<<16) | (black<<8) | black;
	darkgreen = MAPCOLOUR(0x00, 0x20, 0x00);
	darkgreen = (darkgreen<<24) | (darkgreen<<16) | (darkgreen<<8) | darkgreen;
	return 0;
}

static void shutdown(void) {
}

static void vdg_vsync(void) {
	/* gp_screen.ptbuffer always aligned correctly: */
	void *ptbuffer = gp_screen.ptbuffer;
	pixel = (uint32_t *)ptbuffer + 58;
	subline = 0;
}

static void vdg_set_mode(unsigned int mode) {
	switch ((mode & 0xf0) >> 4) {
		case 0: case 2: case 4: case 6:
		case 1: case 3: case 5: case 7:
			video_gp32_module.render_scanline = render_sg4;
			border_colour = black;
			text_mode = (mode & 0x18) >> 3;
			break;
		case 8:
			video_gp32_module.render_scanline = render_cg1;
			cg_colours = &vdg_colour[(mode & 0x08) >> 1];
			border_colour = (cg_colours[0]<<24)|(cg_colours[0]<<16)|(cg_colours[0]<<8)|cg_colours[0];
			break;
		case 9: case 11: case 13:
			video_gp32_module.render_scanline = render_rg1;
			fg_colour = vdg_colour[(mode & 0x08) >> 1];
			bg_colour = black;
			border_colour = (fg_colour<<24)|(fg_colour<<16)|(fg_colour<<8)|fg_colour;
			break;
		case 10: case 12: case 14:
			video_gp32_module.render_scanline = render_cg2;
			cg_colours = &vdg_colour[(mode & 0x08) >> 1];
			border_colour = (cg_colours[0]<<24)|(cg_colours[0]<<16)|(cg_colours[0]<<8)|cg_colours[0];
			break;
		case 15: default:
			if ((mode & 0x08) && running_config.cross_colour_phase) {
				video_gp32_module.render_scanline = render_cg2;
				cg_colours = &vdg_colour[4 + running_config.cross_colour_phase * 4];
				border_colour = (vdg_colour[4]<<24)|(vdg_colour[4]<<16)|(vdg_colour[4]<<8)|vdg_colour[4];
			} else {
				video_gp32_module.render_scanline = render_rg6;
				fg_colour = vdg_colour[(mode & 0x08) >> 1];
				border_colour = (fg_colour<<24)|(fg_colour<<16)|(fg_colour<<8)|fg_colour;
			}
			bg_colour = black;
			break;
	}
}

static inline void vram_ptrs_16(uint8_t **ptrs) {
	int i;
	for (i = 4; i; i--) {
		*(ptrs++) = sam_vdg_bytes(16);
		(void)sam_vdg_bytes(6);
		sam_vdg_hsync();
	}
}

static inline void vram_ptrs_32(uint8_t **ptrs) {
	int i;
	for (i = 4; i; i--) {
		*(ptrs++) = sam_vdg_bytes(16);
		*(ptrs++) = sam_vdg_bytes(16);
		(void)sam_vdg_bytes(10);
		sam_vdg_hsync();
	}
}

static void render_sg4(void) {
	uint8_t *vram0, *vram1, *vram2, *vram3;
	uint32_t in4, in5, in6, in7, out, i, j, k;
	uint32_t *dest;
	const uint8_t *charset;
	uint8_t *vram_ptrs[8];
	dest = pixel--;
	for (i = 32; i; i--) {
		*dest = border_colour;
		dest += 60;
	}
	charset = vdg_alpha_gp32[text_mode][subline];
	vram_ptrs_32(vram_ptrs);
	for (k = 0; k < 2; k++) {
		vram0 = vram_ptrs[k]; vram1 = vram_ptrs[2+k];
		vram2 = vram_ptrs[4+k]; vram3 = vram_ptrs[6+k];
		for (j = 16; j; j--) {
			in4 = (*(vram0++) << 2);
			in5 = (*(vram1++) << 2) + 1;
			in6 = (*(vram2++) << 2) + 2;
			in7 = (*(vram3++) << 2) + 3;
			for (i = 8; i; i--) {
				out = *(charset + (in4<<3)) << 24;
				out |= *(charset + (in5<<3)) << 16;
				out |= *(charset + (in6<<3)) << 8;
				out |= *(charset + (in7<<3));
				*dest = out;
				dest += 60;
				charset++;
			}
			charset -= 8;
		}
	}
	for (i = 32; i; i--) {
		*dest = border_colour;
		dest += 60;
	}
	subline++;
	if (subline > 2) subline = 0;
}

static void render_cg1(void) {
	uint8_t *vram0, *vram1, *vram2, *vram3;
	uint32_t in4, in5, in6, in7, out, i, j;
	uint32_t *dest;
	uint8_t *vram_ptrs[4];
	dest = pixel--;
	for (i = 32; i; i--) {
		*dest = border_colour;
		dest += 60;
	}
	vram_ptrs_16(vram_ptrs);
	vram0 = vram_ptrs[0]; vram1 = vram_ptrs[1];
	vram2 = vram_ptrs[2]; vram3 = vram_ptrs[3];
	for (j = 16; j; j--) {
		in4 = *(vram0++);
		in5 = *(vram1++);
		in6 = *(vram2++);
		in7 = *(vram3++);
		out = *(cg_colours + ((in4&0xc0)>>6)) << 24;
		out |= *(cg_colours + ((in5&0xc0)>>6)) << 16;
		out |= *(cg_colours + ((in6&0xc0)>>6)) << 8;
		out |= *(cg_colours + ((in7&0xc0)>>6));
		*dest = out; dest += 60;
		*dest = out; dest += 60;
		*dest = out; dest += 60;
		*dest = out; dest += 60;
		out = *(cg_colours + ((in4&0x30)>>4)) << 24;
		out |= *(cg_colours + ((in5&0x30)>>4)) << 16;
		out |= *(cg_colours + ((in6&0x30)>>4)) << 8;
		out |= *(cg_colours + ((in7&0x30)>>4));
		*dest = out; dest += 60;
		*dest = out; dest += 60;
		*dest = out; dest += 60;
		*dest = out; dest += 60;
		out = *(cg_colours + ((in4&0x0c)>>2)) << 24;
		out |= *(cg_colours + ((in5&0x0c)>>2)) << 16;
		out |= *(cg_colours + ((in6&0x0c)>>2)) << 8;
		out |= *(cg_colours + ((in7&0x0c)>>2));
		*dest = out; dest += 60;
		*dest = out; dest += 60;
		*dest = out; dest += 60;
		*dest = out; dest += 60;
		out = *(cg_colours + (in4&0x03)) << 24;
		out |= *(cg_colours + (in5&0x03)) << 16;
		out |= *(cg_colours + (in6&0x03)) << 8;
		out |= *(cg_colours + (in7&0x03));
		*dest = out; dest += 60;
		*dest = out; dest += 60;
		*dest = out; dest += 60;
		*dest = out; dest += 60;
	}
	for (i = 32; i; i--) {
		*dest = border_colour;
		dest += 60;
	}
	subline++;
	if (subline > 2) subline = 0;
}

static void render_rg1(void) {
	uint8_t *vram0, *vram1, *vram2, *vram3;
	uint32_t in4, in5, in6, in7, out, i, j;
	uint32_t *dest;
	uint8_t *vram_ptrs[4];
	dest = pixel--;
	for (i = 32; i; i--) {
		*dest = border_colour;
		dest += 60;
	}
	vram_ptrs_16(vram_ptrs);
	vram0 = vram_ptrs[0]; vram1 = vram_ptrs[1];
	vram2 = vram_ptrs[2]; vram3 = vram_ptrs[3];
	for (j = 16; j; j--) {
		in4 = *(vram0++);
		in5 = *(vram1++);
		in6 = *(vram2++);
		in7 = *(vram3++);
		out = ((in4 & 0x80)?fg_colour:bg_colour) << 24;
		out |= ((in5 & 0x80)?fg_colour:bg_colour) << 16;
		out |= ((in6 & 0x80)?fg_colour:bg_colour) << 8;
		out |= ((in7 & 0x80)?fg_colour:bg_colour);
		*dest = out; dest += 60;
		*dest = out; dest += 60;
		out = ((in4 & 0x40)?fg_colour:bg_colour) << 24;
		out |= ((in5 & 0x40)?fg_colour:bg_colour) << 16;
		out |= ((in6 & 0x40)?fg_colour:bg_colour) << 8;
		out |= ((in7 & 0x40)?fg_colour:bg_colour);
		*dest = out; dest += 60;
		*dest = out; dest += 60;
		out = ((in4 & 0x20)?fg_colour:bg_colour) << 24;
		out |= ((in5 & 0x20)?fg_colour:bg_colour) << 16;
		out |= ((in6 & 0x20)?fg_colour:bg_colour) << 8;
		out |= ((in7 & 0x20)?fg_colour:bg_colour);
		*dest = out; dest += 60;
		*dest = out; dest += 60;
		out = ((in4 & 0x10)?fg_colour:bg_colour) << 24;
		out |= ((in5 & 0x10)?fg_colour:bg_colour) << 16;
		out |= ((in6 & 0x10)?fg_colour:bg_colour) << 8;
		out |= ((in7 & 0x10)?fg_colour:bg_colour);
		*dest = out; dest += 60;
		*dest = out; dest += 60;
		out = ((in4 & 0x08)?fg_colour:bg_colour) << 24;
		out |= ((in5 & 0x08)?fg_colour:bg_colour) << 16;
		out |= ((in6 & 0x08)?fg_colour:bg_colour) << 8;
		out |= ((in7 & 0x08)?fg_colour:bg_colour);
		*dest = out; dest += 60;
		*dest = out; dest += 60;
		out = ((in4 & 0x04)?fg_colour:bg_colour) << 24;
		out |= ((in5 & 0x04)?fg_colour:bg_colour) << 16;
		out |= ((in6 & 0x04)?fg_colour:bg_colour) << 8;
		out |= ((in7 & 0x04)?fg_colour:bg_colour);
		*dest = out; dest += 60;
		*dest = out; dest += 60;
		out = ((in4 & 0x02)?fg_colour:bg_colour) << 24;
		out |= ((in5 & 0x02)?fg_colour:bg_colour) << 16;
		out |= ((in6 & 0x02)?fg_colour:bg_colour) << 8;
		out |= ((in7 & 0x02)?fg_colour:bg_colour);
		*dest = out; dest += 60;
		*dest = out; dest += 60;
		out = ((in4 & 0x01)?fg_colour:bg_colour) << 24;
		out |= ((in5 & 0x01)?fg_colour:bg_colour) << 16;
		out |= ((in6 & 0x01)?fg_colour:bg_colour) << 8;
		out |= ((in7 & 0x01)?fg_colour:bg_colour);
		*dest = out; dest += 60;
		*dest = out; dest += 60;
	}
	for (i = 32; i; i--) {
		*dest = border_colour;
		dest += 60;
	}
	subline++;
	if (subline > 2) subline = 0;
}

static void render_cg2(void) {
	uint8_t *vram0, *vram1, *vram2, *vram3;
	uint32_t in4, in5, in6, in7, out, i, j, k;
	uint32_t *dest;
	uint8_t *vram_ptrs[8];
	dest = pixel--;
	for (i = 32; i; i--) {
		*dest = border_colour;
		dest += 60;
	}
	vram_ptrs_32(vram_ptrs);
	for (k = 0; k < 2; k++) {
		vram0 = vram_ptrs[k]; vram1 = vram_ptrs[2+k];
		vram2 = vram_ptrs[4+k]; vram3 = vram_ptrs[6+k];
		for (j = 16; j; j--) {
			in4 = *(vram0++);
			in5 = *(vram1++);
			in6 = *(vram2++);
			in7 = *(vram3++);
			out = *(cg_colours + ((in4&0xc0)>>6)) << 24;
			out |= *(cg_colours + ((in5&0xc0)>>6)) << 16;
			out |= *(cg_colours + ((in6&0xc0)>>6)) << 8;
			out |= *(cg_colours + ((in7&0xc0)>>6));
			*dest = out;
			dest += 60;
			*dest = out;
			dest += 60;
			out = *(cg_colours + ((in4&0x30)>>4)) << 24;
			out |= *(cg_colours + ((in5&0x30)>>4)) << 16;
			out |= *(cg_colours + ((in6&0x30)>>4)) << 8;
			out |= *(cg_colours + ((in7&0x30)>>4));
			*dest = out;
			dest += 60;
			*dest = out;
			dest += 60;
			out = *(cg_colours + ((in4&0x0c)>>2)) << 24;
			out |= *(cg_colours + ((in5&0x0c)>>2)) << 16;
			out |= *(cg_colours + ((in6&0x0c)>>2)) << 8;
			out |= *(cg_colours + ((in7&0x0c)>>2));
			*dest = out;
			dest += 60;
			*dest = out;
			dest += 60;
			out = *(cg_colours + (in4&0x03)) << 24;
			out |= *(cg_colours + (in5&0x03)) << 16;
			out |= *(cg_colours + (in6&0x03)) << 8;
			out |= *(cg_colours + (in7&0x03));
			*dest = out;
			dest += 60;
			*dest = out;
			dest += 60;
		}
	}
	for (i = 32; i; i--) {
		*dest = border_colour;
		dest += 60;
	}
	subline++;
	if (subline > 2) subline = 0;
}

static void render_rg6(void) {
	uint8_t *vram0, *vram1, *vram2, *vram3;
	uint32_t in4, in5, in6, in7, out, i, j, k;
	uint32_t *dest;
	uint8_t *vram_ptrs[8];
	dest = pixel--;
	for (i = 32; i; i--) {
		*dest = border_colour;
		dest += 60;
	}
	vram_ptrs_32(vram_ptrs);
	for (k = 0; k < 2; k++) {
		vram0 = vram_ptrs[k]; vram1 = vram_ptrs[2+k];
		vram2 = vram_ptrs[4+k]; vram3 = vram_ptrs[6+k];
		for (j = 16; j; j--) {
			in4 = *(vram0++);
			in5 = *(vram1++);
			in6 = *(vram2++);
			in7 = *(vram3++);
			out = ((in4 & 0x80)?fg_colour:bg_colour) << 24;
			out |= ((in5 & 0x80)?fg_colour:bg_colour) << 16;
			out |= ((in6 & 0x80)?fg_colour:bg_colour) << 8;
			out |= ((in7 & 0x80)?fg_colour:bg_colour);
			*dest = out; dest += 60;
			out = ((in4 & 0x40)?fg_colour:bg_colour) << 24;
			out |= ((in5 & 0x40)?fg_colour:bg_colour) << 16;
			out |= ((in6 & 0x40)?fg_colour:bg_colour) << 8;
			out |= ((in7 & 0x40)?fg_colour:bg_colour);
			*dest = out; dest += 60;
			out = ((in4 & 0x20)?fg_colour:bg_colour) << 24;
			out |= ((in5 & 0x20)?fg_colour:bg_colour) << 16;
			out |= ((in6 & 0x20)?fg_colour:bg_colour) << 8;
			out |= ((in7 & 0x20)?fg_colour:bg_colour);
			*dest = out; dest += 60;
			out = ((in4 & 0x10)?fg_colour:bg_colour) << 24;
			out |= ((in5 & 0x10)?fg_colour:bg_colour) << 16;
			out |= ((in6 & 0x10)?fg_colour:bg_colour) << 8;
			out |= ((in7 & 0x10)?fg_colour:bg_colour);
			*dest = out; dest += 60;
			out = ((in4 & 0x08)?fg_colour:bg_colour) << 24;
			out |= ((in5 & 0x08)?fg_colour:bg_colour) << 16;
			out |= ((in6 & 0x08)?fg_colour:bg_colour) << 8;
			out |= ((in7 & 0x08)?fg_colour:bg_colour);
			*dest = out; dest += 60;
			out = ((in4 & 0x04)?fg_colour:bg_colour) << 24;
			out |= ((in5 & 0x04)?fg_colour:bg_colour) << 16;
			out |= ((in6 & 0x04)?fg_colour:bg_colour) << 8;
			out |= ((in7 & 0x04)?fg_colour:bg_colour);
			*dest = out; dest += 60;
			out = ((in4 & 0x02)?fg_colour:bg_colour) << 24;
			out |= ((in5 & 0x02)?fg_colour:bg_colour) << 16;
			out |= ((in6 & 0x02)?fg_colour:bg_colour) << 8;
			out |= ((in7 & 0x02)?fg_colour:bg_colour);
			*dest = out; dest += 60;
			out = ((in4 & 0x01)?fg_colour:bg_colour) << 24;
			out |= ((in5 & 0x01)?fg_colour:bg_colour) << 16;
			out |= ((in6 & 0x01)?fg_colour:bg_colour) << 8;
			out |= ((in7 & 0x01)?fg_colour:bg_colour);
			*dest = out; dest += 60;
		}
	}
	for (i = 32; i; i--) {
		*dest = border_colour;
		dest += 60;
	}
	subline++;
	if (subline > 2) subline = 0;
}
