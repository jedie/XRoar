/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2006  Ciaran Anscomb
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

#ifdef HAVE_GP32

#include <string.h>
#include <gpgraphic.h>

#include "sam.h"
#include "video.h"
#include "keyboard.h"
#include "joystick.h"
#include "types.h"

static int init(void);
static void shutdown(void);
static void fillrect(uint_least16_t x, uint_least16_t y,
		uint_least16_t w, uint_least16_t h, uint32_t colour);
static void blit(uint_least16_t x, uint_least16_t y, Sprite *src);
static void backup(void);
static void restore(void);
static void vdg_reset(void);
static void vdg_vsync(void);
static void vdg_set_mode(unsigned int mode);
static void vdg_render_sg4(void);
/* static void vdg_render_sg6(void); */
static void vdg_render_cg1(void);
static void vdg_render_rg1(void);
static void vdg_render_cg2(void);
static void vdg_render_rg6(void);

extern KeyboardModule keyboard_gp32_module;
extern JoystickModule joystick_gp32_module;

VideoModule video_gp32_module = {
	"gp32",
	"GP32 video driver",
	init, shutdown,
	fillrect, blit,
	backup, restore, NULL, NULL,
	vdg_reset, vdg_vsync, vdg_set_mode,
	vdg_render_sg4, vdg_render_sg4 /* 6 */, vdg_render_cg1,
	vdg_render_rg1, vdg_render_cg2, vdg_render_rg6,
	NULL
};

#define MAPCOLOUR(r,g,b) ((r & 0xc0) | (g & 0xe0) >> 2 | (b & 0xe0) >> 5)
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
//uint32_t border_colour;  /* TESTING */

static uint8_t screen_backup[320*240];
GPDRAWSURFACE screen;
static uint8_t *rendered_alpha;

#include "vdg_bitmaps_gp32.c"

static int init(void) {
	GpGraphicModeSet(GPC_DFLAG_8BPP, NULL);
	GpLcdSurfaceGet(&screen, 0);
	GpSurfaceSet(&screen);
	GpLcdEnable();
	swi_mmu_change((unsigned char *)((uint32_t)screen.ptbuffer & ~4095), (unsigned char *)((uint32_t)(screen.ptbuffer + (320 * 240) + 4095) & ~4095), MMU_NCNB);
	fillrect(0, 0, 320, 196, 0);
	fillrect(0, 196, 320, 44, 0xffffff00);
	vdg_colour[0] = MAPCOLOUR(0x00, 0xff, 0x00);
	vdg_colour[1] = MAPCOLOUR(0xff, 0xff, 0x00);
	vdg_colour[2] = MAPCOLOUR(0x00, 0x00, 0xff);
	vdg_colour[3] = MAPCOLOUR(0xff, 0x00, 0x00);
	vdg_colour[4] = MAPCOLOUR(0xff, 0xe0, 0xe0);
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
	/* Set preferred keyboard driver */
	keyboard_module = &keyboard_gp32_module;
	return 0;
}

static void shutdown(void) {
}

static void fillrect(uint_least16_t x, uint_least16_t y, uint_least16_t w, uint_least16_t h, uint32_t colour) {
	uint8_t *d = (uint8_t *)screen.ptbuffer + (x*240) + (240-y) - h;
	uint8_t c = MAPCOLOUR(colour >> 24, (colour >> 16) & 0xff,
			(colour >> 8) & 0xff);
	uint_least16_t skip = 240 - h;
	uint_least16_t j;
	for (; w; w--) {
		for (j = h; j; j--) {
			*(d++) = c;
		}
		d += skip;
	}
}

static void blit(uint_least16_t x, uint_least16_t y, Sprite *src) {
	uint8_t *s = src->data;
	uint8_t *d = (uint8_t *)screen.ptbuffer + (x*240) + (240-y) - src->h;
	uint_least16_t skip = 240 - src->h;
	uint_least16_t j, w = src->w;
	for (; w; w--) {
		for (j = src->h; j; j--) {
			*(d++) = *(s++);
		}
		d += skip;
	}
}

static void backup(void) {
	memcpy(screen_backup, screen.ptbuffer, 320*240*sizeof(uint8_t));
}

static void restore(void) {
	memcpy(screen.ptbuffer, screen_backup, 320*240*sizeof(uint8_t));
}

static void vdg_reset(void) {
	pixel = (uint32_t *)screen.ptbuffer + 58;
	subline = 0;
}

static void vdg_vsync(void) {
	/* VIDEO_UPDATE; */
	pixel = (uint32_t *)screen.ptbuffer + 58;
	subline = 0;
}

static inline void vram_ptrs_16(uint8_t **ptrs) {
	int i;
	for (i = 4; i; i--) {
		*(ptrs++) = (uint8_t *)sam_vram_ptr(sam_vdg_address);
		sam_vdg_xstep(16);
		sam_vdg_hsync(16,6,0);
	}
}

static inline void vram_ptrs_32(uint8_t **ptrs) {
	int i, j;
	for (j = 4; j; j--) {
		for (i = 2; i; i--) {
			*(ptrs++) = (uint8_t *)sam_vram_ptr(sam_vdg_address);
			sam_vdg_xstep(16);
		}
		sam_vdg_hsync(32,10,16);
	}
}

static void vdg_render_sg4(void) {
	uint8_t *vram0, *vram1, *vram2, *vram3;
	uint32_t in4, in5, in6, in7, out, i, j, k;
	uint32_t *dest;
	uint8_t *charset;
	uint8_t *vram_ptrs[8];
	dest = pixel--;
	for (i = 32; i; i--) {
		*dest = border_colour;
		dest += 60;
	}
	charset = rendered_alpha + subline * 8192;
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

static void vdg_render_cg1(void) {
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

static void vdg_render_rg1(void) {
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

static void vdg_render_cg2(void) {
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

static void vdg_render_rg6(void) {
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

static void vdg_set_mode(unsigned int mode) {
	if (mode & 0x80) {
		/* Graphics modes */
		if (((mode & 0x70) == 0x70) && video_artifact_mode) {
			cg_colours = &vdg_colour[4 + video_artifact_mode * 4];
			fg_colour = vdg_colour[(mode & 0x08) >> 1];
		} else {
			cg_colours = &vdg_colour[(mode & 0x08) >> 1];
			fg_colour = cg_colours[0];
		}
		bg_colour = black;
		border_colour = (fg_colour<<24)|(fg_colour<<16)|(fg_colour<<8)|fg_colour;
	} else {
		bg_colour = darkgreen;
		border_colour = black;
		rendered_alpha = (uint8_t *)vdg_alpha_gp32[(mode & 0x08) >> 3];
	}
}

#endif  /* HAVE_GP32 */
