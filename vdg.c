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
#include <string.h>

#include "events.h"
#include "logging.h"
#include "machine.h"
#include "mc6809.h"
#include "mc6821.h"
#include "module.h"
#include "sam.h"
#include "vdg.h"
#include "vdg_bitmaps.h"
#include "xroar.h"

/* Offset to the first displayed pixel. */
#define SCAN_OFFSET (VDG_LEFT_BORDER_START)

/* External handler to fetch data for display.  First arg is number of bytes,
 * second a pointer to a buffer to receive them. */
void (*vdg_fetch_bytes)(int, uint8_t *);

static event_ticks scanline_start;
static _Bool is_32byte;
static void render_scanline(void);

static int beam_pos;

static _Bool nA_S;
static _Bool nA_G;
static _Bool nINT_EXT, nINT_EXT_, nINT_EXT__;
static _Bool GM0;
static _Bool CSS, CSS_, CSS__;
static uint8_t pixel_data[456];

static uint8_t *pixel;
static uint8_t s_fg_colour;
static uint8_t s_bg_colour;
static uint8_t fg_colour;
static uint8_t bg_colour;
static uint8_t cg_colours;
static uint8_t border_colour;

static unsigned scanline;
static unsigned int subline;
static int frame;

enum vdg_render_mode {
	VDG_RENDER_SG,
	VDG_RENDER_CG,
	VDG_RENDER_RG,
};

static enum vdg_render_mode render_mode;

static uint8_t vram[32];
static int lborder_remaining;
static int vram_remaining;
static int rborder_remaining;
static int vram_bit = 0;
static int vram_idx = 0;
static uint8_t vram_g_data;
static uint8_t vram_sg_data;

static struct event hs_fall_event, hs_rise_event;
static void do_hs_fall(void *);
static void do_hs_rise(void *);

#define SCANLINE(s) ((s) % VDG_FRAME_DURATION)

void vdg_init(void) {
	event_init(&hs_fall_event, do_hs_fall, NULL);
	event_init(&hs_rise_event, do_hs_rise, NULL);
}

void vdg_reset(void) {
	video_module->vsync();
	pixel = pixel_data;
	frame = 0;
	scanline = 0;
	subline = 0;
	scanline_start = event_current_tick;
	hs_fall_event.at_tick = event_current_tick + VDG_LINE_DURATION;
	event_queue(&MACHINE_EVENT_LIST, &hs_fall_event);
	vdg_set_mode(0);
	beam_pos = 0;
	vram_idx = 0;
	vram_bit = 0;
	lborder_remaining = VDG_tLB / 2;
	vram_remaining = is_32byte ? 32 : 16;
	rborder_remaining = VDG_tRB / 2;
}

static void do_hs_fall(void *data) {
	(void)data;
	/* Finish rendering previous scanline */
	if (frame == 0 && (scanline == VDG_TOP_BORDER_START || scanline == VDG_ACTIVE_AREA_END))
		memset(pixel_data, border_colour, 372);
	if (frame == 0 && scanline >= (VDG_TOP_BORDER_START + 1) && scanline < (VDG_BOTTOM_BORDER_END - 2)) {
		if (scanline >= VDG_ACTIVE_AREA_START && scanline < VDG_ACTIVE_AREA_END) {
			render_scanline();
			sam_vdg_hsync();
			pixel = pixel_data;
			subline++;
			if (subline > 11)
				subline = 0;
		}
		video_module->render_scanline(pixel_data);
	}

	/* HS falling edge */
	PIA_RESET_Cx1(PIA0.a);

	scanline_start = hs_fall_event.at_tick;
	/* Next HS rise and fall */
	hs_rise_event.at_tick = scanline_start + VDG_HS_RISING_EDGE;
	hs_fall_event.at_tick = scanline_start + VDG_LINE_DURATION;

	/* Two delays of 25 scanlines each occur 24 lines after FS falling edge
	 * and at FS rising edge in PAL systems */
	if (IS_PAL) {
		if (scanline == SCANLINE(VDG_ACTIVE_AREA_END + 24)
		    || scanline == SCANLINE(VDG_ACTIVE_AREA_END + 32)) {
			hs_rise_event.at_tick += 25 * VDG_PAL_PADDING_LINE;
			hs_fall_event.at_tick += 25 * VDG_PAL_PADDING_LINE;
		}
	}

	event_queue(&MACHINE_EVENT_LIST, &hs_rise_event);
	event_queue(&MACHINE_EVENT_LIST, &hs_fall_event);

	/* Next scanline */
	scanline = SCANLINE(scanline + 1);
	beam_pos = 0;
	vram_idx = 0;
	vram_bit = 0;
	lborder_remaining = VDG_tLB / 2;
	vram_remaining = is_32byte ? 32 : 16;
	rborder_remaining = VDG_tRB / 2;

	if (scanline == VDG_ACTIVE_AREA_START) {
		subline = 0;
	}

	if (scanline == VDG_ACTIVE_AREA_END) {
		/* FS falling edge */
		PIA_RESET_Cx1(PIA0.b);
	}

	if (scanline == VDG_VBLANK_START) {
		/* FS rising edge */
		PIA_SET_Cx1(PIA0.b);
		sam_vdg_fsync();
		frame--;
		if (frame < 0)
			frame = xroar_frameskip;
		if (frame == 0)
			video_module->vsync();
	}

}

static void do_hs_rise(void *data) {
	(void)data;
	/* HS rising edge */
	PIA_SET_Cx1(PIA0.a);
}

static void render_scanline(void) {
	int beam_to = ((int)(event_current_tick - scanline_start) - SCAN_OFFSET) / 2;
	if (beam_to > 372)
		beam_to = 372;
	if (beam_pos >= beam_to)
		return;

	while (lborder_remaining > 0) {
		if ((beam_pos & 7) == 4) {
			CSS_ = CSS__;
			nINT_EXT_ = nINT_EXT__;
		}
		*(pixel++) = border_colour;
		beam_pos++;
		lborder_remaining--;
		if (beam_pos >= beam_to)
			return;
	}

	while (vram_remaining > 0) {
		if (vram_bit == 0) {
			if (vram_idx == 0)
				vdg_fetch_bytes(16, vram);
			vram_g_data = vram[vram_idx];
			vram_idx = (vram_idx + 1) & 15;
			vram_bit = 8;
			nA_S = vram_g_data & 0x80;

			CSS = CSS_;
			CSS_ = CSS__;
			nINT_EXT = nINT_EXT_;
			nINT_EXT_ = nINT_EXT__;
			cg_colours = !CSS ? VDG_GREEN : VDG_WHITE;

			if (!nA_G && !nA_S) {
				_Bool INV = vram_g_data & 0x40;
				if (!nINT_EXT)
					vram_g_data = vdg_alpha[(vram_g_data&0x3f)*12 + subline];
				if (INV)
					vram_g_data = ~vram_g_data;
			}

			if (!nA_G && nA_S) {
				vram_sg_data = vram_g_data;
				if (!nINT_EXT) {
					if (subline < 6)
						vram_sg_data >>= 2;
					s_fg_colour = (vram_g_data >> 4) & 7;
				} else {
					if (subline < 4)
						vram_sg_data >>= 4;
					else if (subline < 8)
						vram_sg_data >>= 2;
					s_fg_colour = cg_colours + ((vram_g_data >> 6) & 3);
				}
				s_bg_colour = !nA_G ? VDG_BLACK : VDG_GREEN;
				vram_sg_data = ((vram_sg_data & 2) ? 0xf0 : 0) | ((vram_sg_data & 1) ? 0x0f : 0);
			}

			if (!nA_G) {
				render_mode = !nA_S ? VDG_RENDER_RG : VDG_RENDER_SG;
				fg_colour = !CSS ? VDG_GREEN : VDG_BRIGHT_ORANGE;
				bg_colour = !CSS ? VDG_DARK_GREEN : VDG_DARK_ORANGE;
			} else {
				render_mode = GM0 ? VDG_RENDER_RG : VDG_RENDER_CG;
				fg_colour = !CSS ? VDG_GREEN : VDG_WHITE;
				bg_colour = !CSS ? VDG_DARK_GREEN : VDG_BLACK;
			}
		}

		if (is_32byte) {
			switch (render_mode) {
			case VDG_RENDER_SG:
				*(pixel) = (vram_sg_data&0x80) ? s_fg_colour : s_bg_colour;
				*(pixel+1) = (vram_sg_data&0x40) ? s_fg_colour : s_bg_colour;
				break;
			case VDG_RENDER_CG:
				*(pixel) = *(pixel+1) = cg_colours + ((vram_g_data & 0xc0) >> 6);
				break;
			case VDG_RENDER_RG:
				*(pixel) = (vram_g_data&0x80) ? fg_colour : bg_colour;
				*(pixel+1) = (vram_g_data&0x40) ? fg_colour : bg_colour;
				break;
			}
			pixel += 2;
			beam_pos += 2;
		} else {
			switch (render_mode) {
			case VDG_RENDER_SG:
				*(pixel) = *(pixel+1) = (vram_sg_data&0x80) ? s_fg_colour : s_bg_colour;
				*(pixel+2) = *(pixel+3) = (vram_sg_data&0x40) ? s_fg_colour : s_bg_colour;
				break;
			case VDG_RENDER_CG:
				*(pixel) = *(pixel+1) = *(pixel+2) = *(pixel+3) = cg_colours + ((vram_g_data & 0xc0) >> 6);
				break;
			case VDG_RENDER_RG:
				*(pixel) = *(pixel+1) = (vram_g_data&0x80) ? fg_colour : bg_colour;
				*(pixel+2) = *(pixel+3) = (vram_g_data&0x40) ? fg_colour : bg_colour;
				break;
			}
			pixel += 4;
			beam_pos += 4;
		}

		vram_bit -= 2;
		if (vram_bit == 0) {
			vram_remaining--;
			if (vram_remaining == 0) {
				vdg_fetch_bytes(is_32byte ? 10 : 6, NULL);
			}
		}
		vram_g_data <<= 2;
		vram_sg_data <<= 2;
		if (beam_pos >= beam_to)
			return;
	}

	while (rborder_remaining > 0) {
		if ((beam_pos & 7) == 4) {
			CSS_ = CSS__;
			nINT_EXT_ = nINT_EXT__;
		}
		if (beam_pos == 316) {
			CSS = CSS_;
			nINT_EXT = nINT_EXT_;
		}
		border_colour = nA_G ? cg_colours : VDG_BLACK;
		*(pixel++) = border_colour;
		beam_pos++;
		rborder_remaining--;
		if (beam_pos >= beam_to)
			return;
	}
}

void vdg_set_mode(unsigned mode) {
	/* Render scanline so far before changing modes */
	if (frame == 0 && scanline >= VDG_ACTIVE_AREA_START && scanline < VDG_ACTIVE_AREA_END) {
		render_scanline();
	}

	unsigned GM = (mode >> 4) & 7;
	GM0 = mode & 0x10;
	CSS__ = mode & 0x08;
	nINT_EXT__ = mode & 0x10;
	_Bool new_nA_G = mode & 0x80;
	/* If switching from graphics to alpha/semigraphics */
	if (nA_G && !new_nA_G) {
		subline = 0;
		render_mode = VDG_RENDER_RG;
		border_colour = VDG_BLACK;
		if (nA_S) {
			vram_g_data = 0x3f;
			fg_colour = VDG_GREEN;
			bg_colour = VDG_DARK_GREEN;
		} else {
			fg_colour = !CSS ? VDG_GREEN : VDG_BRIGHT_ORANGE;
			bg_colour = !CSS ? VDG_DARK_GREEN : VDG_DARK_ORANGE;
		}
	}
	if (!nA_G && new_nA_G) {
		border_colour = cg_colours;
		fg_colour = !CSS ? VDG_GREEN : VDG_WHITE;
		bg_colour = !CSS ? VDG_DARK_GREEN : VDG_BLACK;
	}
	nA_G = new_nA_G;

	if (nA_G) {
		render_mode = GM0 ? VDG_RENDER_RG : VDG_RENDER_CG;
	}

	is_32byte = !nA_G || !(GM == 0 || (GM0 && GM != 7));
}
