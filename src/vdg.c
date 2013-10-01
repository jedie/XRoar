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

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "events.h"
#include "logging.h"
#include "machine.h"
#include "mc6809.h"
#include "module.h"
#include "sam.h"
#include "vdg.h"
#include "vdg_bitmaps.h"
#include "xroar.h"

// Convert VDG timing to SAM cycles:
#define VDG_CYCLES(c) ((c) * 2)

/* External handler to fetch data for display.  First arg is number of bytes,
 * second a pointer to a buffer to receive them. */
void (*vdg_fetch_bytes)(int, uint8_t *);

/* Delegates to notify on signal edges */
vdg_edge_delegate vdg_signal_hs = { NULL, NULL };
vdg_edge_delegate vdg_signal_fs = { NULL, NULL };

static event_ticks scanline_start;
static _Bool is_32byte;
static void render_scanline(void);

static unsigned beam_pos;

static _Bool nA_S;
static _Bool nA_G;
static _Bool nINT_EXT, nINT_EXT_, nINT_EXT__;
static _Bool GM0;
static _Bool CSS, CSS_, CSS__;
// 6847T1-only:
static _Bool inverse_text;
static _Bool text_border;

// Enough space for an entire scanline is reserved, but only the borders and
// active area are drawn into - the rest is initialised to black.
static uint8_t pixel_data[VDG_LINE_DURATION];

static uint8_t *pixel;
static uint8_t s_fg_colour;
static uint8_t s_bg_colour;
static uint8_t fg_colour;
static uint8_t bg_colour;
static uint8_t cg_colours;
static uint8_t border_colour;
// Colour to use in place of "bright orange":
static uint8_t bright_orange;
// 6847T1-only:
static uint8_t text_border_colour;

static unsigned scanline;
static unsigned int subline;
static int frame;

enum vdg_render_mode {
	VDG_RENDER_SG,
	VDG_RENDER_CG,
	VDG_RENDER_RG,
};

static enum vdg_render_mode render_mode;

static uint8_t vram[84];
static uint8_t *vram_ptr = vram;
static unsigned vram_nbytes = 0;
static unsigned lborder_remaining;
static unsigned vram_remaining;
static unsigned rborder_remaining;
static int vram_bit = 0;
static uint8_t vram_g_data;
static uint8_t vram_sg_data;

static struct event hs_fall_event, hs_rise_event;
static void do_hs_fall(void *);
static void do_hs_rise(void *);
static void do_hs_fall_pal_coco(void *);
static unsigned pal_padding;

#define SCANLINE(s) ((s) % VDG_FRAME_DURATION)

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

// Set to enable the extra stuff on a 6847T1
_Bool vdg_t1 = 0;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void vdg_init(void) {
	event_init(&hs_fall_event, do_hs_fall, NULL);
	event_init(&hs_rise_event, do_hs_rise, NULL);
}

void vdg_reset(void) {
	video_module->vsync();
	memset(pixel_data, VDG_BLACK, sizeof(pixel_data));
	pixel = pixel_data + VDG_LEFT_BORDER_START;
	frame = 0;
	scanline = 0;
	subline = 0;
	scanline_start = event_current_tick;
	hs_fall_event.at_tick = event_current_tick + VDG_CYCLES(VDG_LINE_DURATION);
	event_queue(&MACHINE_EVENT_LIST, &hs_fall_event);
	// 6847T1 doesn't appear to do bright orange:
	bright_orange = vdg_t1 ? VDG_ORANGE : VDG_BRIGHT_ORANGE;
	vdg_set_mode(0);
	beam_pos = 0;
	vram_ptr = vram;
	vram_bit = 0;
	lborder_remaining = VDG_tLB;
	vram_remaining = is_32byte ? 32 : 16;
	rborder_remaining = VDG_tRB;
}

static void do_hs_fall(void *data) {
	(void)data;
	// Finish rendering previous scanline
	if (frame == 0) {
		if (scanline < VDG_ACTIVE_AREA_START) {
			if (scanline == 0) {
				memset(pixel_data + VDG_LEFT_BORDER_START, border_colour, VDG_tAVB);
			}
			video_module->render_scanline(pixel_data);
		} else if (scanline >= VDG_ACTIVE_AREA_START && scanline < VDG_ACTIVE_AREA_END) {
			render_scanline();
			sam_vdg_hsync();
			subline++;
			if (subline > 11)
				subline = 0;
			video_module->render_scanline(pixel_data);
			pixel = pixel_data + VDG_LEFT_BORDER_START;
		} else if (scanline >= VDG_ACTIVE_AREA_END) {
			if (scanline == VDG_ACTIVE_AREA_END) {
				memset(pixel_data + VDG_LEFT_BORDER_START, border_colour, VDG_tAVB);
			}
			video_module->render_scanline(pixel_data);
		}
	}

	// HS falling edge.
	if (vdg_signal_hs.delegate)
		vdg_signal_hs.delegate(vdg_signal_hs.dptr, 0);

	scanline_start = hs_fall_event.at_tick;
	// Next HS rise and fall
	hs_rise_event.at_tick = scanline_start + VDG_CYCLES(VDG_HS_RISING_EDGE);
	hs_fall_event.at_tick = scanline_start + VDG_CYCLES(VDG_LINE_DURATION);

	/* On PAL machines, the clock to the VDG is interrupted at two points
	 * in every frame to fake up some extra scanlines, padding the signal
	 * from 262 lines to 312 lines.  Dragons do not generate an HS-related
	 * interrupt signal during this time, CoCos do.  The positioning and
	 * duration of each interruption differs also. */

	if (IS_PAL && IS_COCO) {
		if (scanline == SCANLINE(VDG_ACTIVE_AREA_END + 25)) {
			pal_padding = 26;
			hs_fall_event.delegate = do_hs_fall_pal_coco;
		} else if (scanline == SCANLINE(VDG_ACTIVE_AREA_END + 47)) {
			pal_padding = 24;
			hs_fall_event.delegate = do_hs_fall_pal_coco;
		}
	} else if (IS_PAL && IS_DRAGON) {
		if (scanline == SCANLINE(VDG_ACTIVE_AREA_END + 24)
		    || scanline == SCANLINE(VDG_ACTIVE_AREA_END + 32)) {
				hs_rise_event.at_tick += 25 * VDG_CYCLES(VDG_PAL_PADDING_LINE);
				hs_fall_event.at_tick += 25 * VDG_CYCLES(VDG_PAL_PADDING_LINE);
		}
	}

	event_queue(&MACHINE_EVENT_LIST, &hs_rise_event);
	event_queue(&MACHINE_EVENT_LIST, &hs_fall_event);

	// Next scanline
	scanline = SCANLINE(scanline + 1);
	beam_pos = 0;
	vram_nbytes = 0;
	vram_ptr = vram;
	vram_bit = 0;
	lborder_remaining = VDG_tLB;
	vram_remaining = is_32byte ? 32 : 16;
	rborder_remaining = VDG_tRB;

	if (scanline == VDG_ACTIVE_AREA_START) {
		subline = 0;
	}

	if (scanline == VDG_ACTIVE_AREA_END) {
		// FS falling edge
		if (vdg_signal_fs.delegate)
			vdg_signal_fs.delegate(vdg_signal_fs.dptr, 0);
	}

	if (scanline == VDG_VBLANK_START) {
		// FS rising edge
		if (vdg_signal_fs.delegate)
			vdg_signal_fs.delegate(vdg_signal_fs.dptr, 1);
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
	// HS rising edge.
	if (vdg_signal_hs.delegate)
		vdg_signal_hs.delegate(vdg_signal_hs.dptr, 1);
}

static void do_hs_fall_pal_coco(void *data) {
	(void)data;
	// HS falling edge
	if (vdg_signal_hs.delegate)
		vdg_signal_hs.delegate(vdg_signal_hs.dptr, 0);

	scanline_start = hs_fall_event.at_tick;
	// Next HS rise and fall
	hs_rise_event.at_tick = scanline_start + VDG_CYCLES(VDG_HS_RISING_EDGE);
	hs_fall_event.at_tick = scanline_start + VDG_CYCLES(VDG_LINE_DURATION);

	pal_padding--;
	if (pal_padding == 0)
		hs_fall_event.delegate = do_hs_fall;

	event_queue(&MACHINE_EVENT_LIST, &hs_rise_event);
	event_queue(&MACHINE_EVENT_LIST, &hs_fall_event);
}

static void render_scanline(void) {
	unsigned beam_to = (event_current_tick - scanline_start) >> 1;
	if (is_32byte && beam_to >= 102) {
		unsigned nbytes = (beam_to - 102) >> 3;
		if (nbytes > 42)
			nbytes = 42;
		if (nbytes > vram_nbytes) {
			vdg_fetch_bytes(nbytes - vram_nbytes, vram + vram_nbytes*2);
			vram_nbytes = nbytes;
		}
	} else if (!is_32byte && beam_to >= 102) {
		unsigned nbytes = (beam_to - 102) >> 4;
		if (nbytes > 22)
			nbytes = 22;
		if (nbytes > vram_nbytes) {
			vdg_fetch_bytes(nbytes - vram_nbytes, vram + vram_nbytes*2);
			vram_nbytes = nbytes;
		}
	}
	beam_to -= VDG_LEFT_BORDER_START;
	if (beam_to > (UINT_MAX/2))
		return;
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
			vram_g_data = *(vram_ptr++);
			uint8_t attr = *(vram_ptr++);
			vram_bit = 8;
			nA_S = attr & 0x80;

			CSS = CSS_;
			CSS_ = CSS__;
			nINT_EXT = nINT_EXT_;
			nINT_EXT_ = nINT_EXT__;
			cg_colours = !CSS ? VDG_GREEN : VDG_WHITE;
			text_border_colour = !CSS ? VDG_GREEN : bright_orange;

			if (!nA_G && !nA_S) {
				_Bool INV;
				if (vdg_t1) {
					INV = nINT_EXT || (attr & 0x40);
					INV ^= inverse_text;
					if (!nINT_EXT)
						vram_g_data |= 0x40;
					vram_g_data = font_6847t1[(vram_g_data&0x7f)*12 + subline];
				} else {
					INV = attr & 0x40;
					if (!nINT_EXT)
						vram_g_data = font_6847[(vram_g_data&0x3f)*12 + subline];
				}
				if (INV)
					vram_g_data = ~vram_g_data;
			}

			if (!nA_G && nA_S) {
				vram_sg_data = vram_g_data;
				if (vdg_t1 || !nINT_EXT) {
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
				fg_colour = !CSS ? VDG_GREEN : bright_orange;
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
			text_border_colour = !CSS ? VDG_GREEN : bright_orange;
		}
		border_colour = nA_G ? cg_colours : (text_border ? text_border_colour : VDG_BLACK);
		*(pixel++) = border_colour;
		beam_pos++;
		rborder_remaining--;
		if (beam_pos >= beam_to)
			return;
	}

	// If a program switches to 32 bytes per line mid-scanline, the whole
	// scanline might not have been rendered:
	while (beam_pos < (VDG_RIGHT_BORDER_END - VDG_LEFT_BORDER_START)) {
		*(pixel++) = VDG_BLACK;
		beam_pos++;
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
	nINT_EXT__ = mode & 0x100;
	_Bool new_nA_G = mode & 0x80;

	inverse_text = vdg_t1 && (GM & 2);
	text_border = vdg_t1 && !inverse_text && (GM & 4);
	text_border_colour = !CSS ? VDG_GREEN : bright_orange;

	/* If switching from graphics to alpha/semigraphics */
	if (nA_G && !new_nA_G) {
		subline = 0;
		render_mode = VDG_RENDER_RG;
		if (nA_S) {
			vram_g_data = 0x3f;
			fg_colour = VDG_GREEN;
			bg_colour = VDG_DARK_GREEN;
		} else {
			fg_colour = !CSS ? VDG_GREEN : bright_orange;
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
	} else {
		border_colour = text_border ? text_border_colour : VDG_BLACK;
	}

	is_32byte = !nA_G || !(GM == 0 || (GM0 && GM != 7));
}
