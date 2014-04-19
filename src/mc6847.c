/*  Copyright 2003-2014 Ciaran Anscomb
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

#include "xalloc.h"

#include "delegate.h"
#include "events.h"
#include "logging.h"
#include "machine.h"
#include "mc6809.h"
#include "mc6847.h"
#include "module.h"
#include "vdg_bitmaps.h"
#include "xroar.h"

// Convert VDG timing to SAM cycles:
#define VDG_CYCLES(c) ((c) * 2)

enum vdg_render_mode {
	VDG_RENDER_SG,
	VDG_RENDER_CG,
	VDG_RENDER_RG,
};

struct MC6847_private {
	struct MC6847 public;

	/* Control lines */
	_Bool nA_S;
	_Bool nA_G;
	_Bool GM0;
	_Bool EXT, EXTa, EXTb;
	_Bool CSS, CSSa, CSSb;
	_Bool inverted_text;

	/* Timing */
	struct event hs_fall_event;
	struct event hs_rise_event;
	event_ticks scanline_start;
	unsigned beam_pos;
	unsigned scanline;

	/* Data */
	uint8_t vram_g_data;
	uint8_t vram_sg_data;

	/* Output */
	uint8_t *pixel;
	int frame;  // frameskip counter

	/* Delegates to notify on signal edges */
	DELEGATE_T1(void, bool) signal_hs;
	DELEGATE_T1(void, bool) signal_fs;

	/* External handler to fetch data for display.  First arg is number of bytes,
	 * second a pointer to a buffer to receive them. */
	void (*fetch_bytes)(int, uint8_t *);

	/* Internal state */
	_Bool is_32byte;
	uint8_t s_fg_colour;
	uint8_t s_bg_colour;
	uint8_t fg_colour;
	uint8_t bg_colour;
	uint8_t cg_colours;
	uint8_t border_colour;
	uint8_t bright_orange;
	int vram_bit;
	enum vdg_render_mode render_mode;
	unsigned pal_padding;
	uint8_t pixel_data[VDG_LINE_DURATION];
	unsigned row;

	uint8_t vram[84];
	uint8_t *vram_ptr;
	unsigned vram_nbytes;

	uint8_t *ext_charset;

	/* Counters */
	unsigned lborder_remaining;
	unsigned vram_remaining;
	unsigned rborder_remaining;

	/* 6847T1 state */
	_Bool is_t1;
	_Bool inverse_text;
	_Bool text_border;
	uint8_t text_border_colour;
};

static void do_hs_fall(void *);
static void do_hs_rise(void *);
static void do_hs_fall_pal_coco(void *);

static void render_scanline(struct MC6847_private *vdg);

#define SCANLINE(s) ((s) % VDG_FRAME_DURATION)

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static void do_hs_fall(void *data) {
	struct MC6847_private *vdg = data;
	// Finish rendering previous scanline
	if (vdg->frame == 0) {
		if (vdg->scanline < VDG_ACTIVE_AREA_START) {
			if (vdg->scanline == 0) {
				memset(vdg->pixel_data + VDG_LEFT_BORDER_START, vdg->border_colour, VDG_tAVB);
			}
			video_module->render_scanline(vdg->pixel_data);
		} else if (vdg->scanline >= VDG_ACTIVE_AREA_START && vdg->scanline < VDG_ACTIVE_AREA_END) {
			render_scanline(vdg);
			vdg->row++;
			if (vdg->row > 11)
				vdg->row = 0;
			video_module->render_scanline(vdg->pixel_data);
			vdg->pixel = vdg->pixel_data + VDG_LEFT_BORDER_START;
		} else if (vdg->scanline >= VDG_ACTIVE_AREA_END) {
			if (vdg->scanline == VDG_ACTIVE_AREA_END) {
				memset(vdg->pixel_data + VDG_LEFT_BORDER_START, vdg->border_colour, VDG_tAVB);
			}
			video_module->render_scanline(vdg->pixel_data);
		}
	}

	// HS falling edge.
	DELEGATE_CALL1(vdg->signal_hs, 0);

	vdg->scanline_start = vdg->hs_fall_event.at_tick;
	// Next HS rise and fall
	vdg->hs_rise_event.at_tick = vdg->scanline_start + VDG_CYCLES(VDG_HS_RISING_EDGE);
	vdg->hs_fall_event.at_tick = vdg->scanline_start + VDG_CYCLES(VDG_LINE_DURATION);

	/* On PAL machines, the clock to the VDG is interrupted at two points
	 * in every frame to fake up some extra scanlines, padding the signal
	 * from 262 lines to 312 lines.  Dragons do not generate an HS-related
	 * interrupt signal during this time, CoCos do.  The positioning and
	 * duration of each interruption differs also. */

	if (IS_PAL && IS_COCO) {
		if (vdg->scanline == SCANLINE(VDG_ACTIVE_AREA_END + 25)) {
			vdg->pal_padding = 26;
			vdg->hs_fall_event.delegate.func = do_hs_fall_pal_coco;
		} else if (vdg->scanline == SCANLINE(VDG_ACTIVE_AREA_END + 47)) {
			vdg->pal_padding = 24;
			vdg->hs_fall_event.delegate.func = do_hs_fall_pal_coco;
		}
	} else if (IS_PAL && IS_DRAGON) {
		if (vdg->scanline == SCANLINE(VDG_ACTIVE_AREA_END + 24)
		    || vdg->scanline == SCANLINE(VDG_ACTIVE_AREA_END + 32)) {
				vdg->hs_rise_event.at_tick += 25 * VDG_CYCLES(VDG_PAL_PADDING_LINE);
				vdg->hs_fall_event.at_tick += 25 * VDG_CYCLES(VDG_PAL_PADDING_LINE);
		}
	}

	event_queue(&MACHINE_EVENT_LIST, &vdg->hs_rise_event);
	event_queue(&MACHINE_EVENT_LIST, &vdg->hs_fall_event);

	// Next scanline
	vdg->scanline = SCANLINE(vdg->scanline + 1);
	vdg->beam_pos = 0;
	vdg->vram_nbytes = 0;
	vdg->vram_ptr = vdg->vram;
	vdg->vram_bit = 0;
	vdg->lborder_remaining = VDG_tLB;
	vdg->vram_remaining = vdg->is_32byte ? 32 : 16;
	vdg->rborder_remaining = VDG_tRB;

	if (vdg->scanline == VDG_ACTIVE_AREA_START) {
		vdg->row = 0;
	}

	if (vdg->scanline == VDG_ACTIVE_AREA_END) {
		// FS falling edge
		DELEGATE_CALL1(vdg->signal_fs, 0);
	}

	if (vdg->scanline == VDG_VBLANK_START) {
		// FS rising edge
		DELEGATE_CALL1(vdg->signal_fs, 1);
		vdg->frame--;
		if (vdg->frame < 0)
			vdg->frame = xroar_frameskip;
		if (vdg->frame == 0)
			video_module->vsync();
	}

}

static void do_hs_rise(void *data) {
	struct MC6847_private *vdg = data;
	// HS rising edge.
	DELEGATE_CALL1(vdg->signal_hs, 1);
}

static void do_hs_fall_pal_coco(void *data) {
	struct MC6847_private *vdg = data;
	// HS falling edge
	DELEGATE_CALL1(vdg->signal_hs, 0);

	vdg->scanline_start = vdg->hs_fall_event.at_tick;
	// Next HS rise and fall
	vdg->hs_rise_event.at_tick = vdg->scanline_start + VDG_CYCLES(VDG_HS_RISING_EDGE);
	vdg->hs_fall_event.at_tick = vdg->scanline_start + VDG_CYCLES(VDG_LINE_DURATION);

	vdg->pal_padding--;
	if (vdg->pal_padding == 0)
		vdg->hs_fall_event.delegate.func = do_hs_fall;

	event_queue(&MACHINE_EVENT_LIST, &vdg->hs_rise_event);
	event_queue(&MACHINE_EVENT_LIST, &vdg->hs_fall_event);
}

static void render_scanline(struct MC6847_private *vdg) {
	unsigned beam_to = (event_current_tick - vdg->scanline_start) >> 1;
	if (vdg->is_32byte && beam_to >= 102) {
		unsigned nbytes = (beam_to - 102) >> 3;
		if (nbytes > 42)
			nbytes = 42;
		if (nbytes > vdg->vram_nbytes) {
			vdg->fetch_bytes(nbytes - vdg->vram_nbytes, vdg->vram + vdg->vram_nbytes*2);
			vdg->vram_nbytes = nbytes;
		}
	} else if (!vdg->is_32byte && beam_to >= 102) {
		unsigned nbytes = (beam_to - 102) >> 4;
		if (nbytes > 22)
			nbytes = 22;
		if (nbytes > vdg->vram_nbytes) {
			vdg->fetch_bytes(nbytes - vdg->vram_nbytes, vdg->vram + vdg->vram_nbytes*2);
			vdg->vram_nbytes = nbytes;
		}
	}
	beam_to -= VDG_LEFT_BORDER_START;
	if (beam_to > (UINT_MAX/2))
		return;
	if (vdg->beam_pos >= beam_to)
		return;

	while (vdg->lborder_remaining > 0) {
		if ((vdg->beam_pos & 7) == 4) {
			vdg->CSSa = vdg->CSS;
			vdg->EXTa = vdg->EXT;
		}
		*(vdg->pixel++) = vdg->border_colour;
		vdg->beam_pos++;
		vdg->lborder_remaining--;
		if (vdg->beam_pos >= beam_to)
			return;
	}

	while (vdg->vram_remaining > 0) {
		if (vdg->vram_bit == 0) {
			vdg->vram_g_data = *(vdg->vram_ptr++);
			uint8_t attr = *(vdg->vram_ptr++);
			vdg->vram_bit = 8;
			vdg->nA_S = attr & 0x80;

			vdg->CSSb = vdg->CSSa;
			vdg->CSSa = vdg->CSS;
			vdg->EXTb = vdg->EXTa;
			vdg->EXTa = vdg->EXT;
			vdg->cg_colours = !vdg->CSSb ? VDG_GREEN : VDG_WHITE;
			vdg->text_border_colour = !vdg->CSSb ? VDG_GREEN : vdg->bright_orange;

			if (!vdg->nA_G && !vdg->nA_S) {
				_Bool INV;
				if (vdg->is_t1) {
					INV = vdg->EXTb || (attr & 0x40);
					INV ^= vdg->inverse_text;
					if (!vdg->EXTb)
						vdg->vram_g_data |= 0x40;
					vdg->vram_g_data = font_6847t1[(vdg->vram_g_data&0x7f)*12 + vdg->row];
				} else {
					INV = attr & 0x40;
					if (vdg->ext_charset)
						vdg->vram_g_data = ~vdg->ext_charset[(vdg->row * 256) + vdg->vram_g_data];
					else if (!vdg->EXTb)
						vdg->vram_g_data = font_6847[(vdg->vram_g_data&0x3f)*12 + vdg->row];
				}
				if (INV ^ vdg->inverted_text)
					vdg->vram_g_data = ~vdg->vram_g_data;
			}

			if (!vdg->nA_G && vdg->nA_S) {
				vdg->vram_sg_data = vdg->vram_g_data;
				if (vdg->is_t1 || !vdg->EXTb) {
					if (vdg->row < 6)
						vdg->vram_sg_data >>= 2;
					vdg->s_fg_colour = (vdg->vram_g_data >> 4) & 7;
				} else {
					if (vdg->row < 4)
						vdg->vram_sg_data >>= 4;
					else if (vdg->row < 8)
						vdg->vram_sg_data >>= 2;
					vdg->s_fg_colour = vdg->cg_colours + ((vdg->vram_g_data >> 6) & 3);
				}
				vdg->s_bg_colour = !vdg->nA_G ? VDG_BLACK : VDG_GREEN;
				vdg->vram_sg_data = ((vdg->vram_sg_data & 2) ? 0xf0 : 0) | ((vdg->vram_sg_data & 1) ? 0x0f : 0);
			}

			if (!vdg->nA_G) {
				vdg->render_mode = !vdg->nA_S ? VDG_RENDER_RG : VDG_RENDER_SG;
				vdg->fg_colour = !vdg->CSSb ? VDG_GREEN : vdg->bright_orange;
				vdg->bg_colour = !vdg->CSSb ? VDG_DARK_GREEN : VDG_DARK_ORANGE;
			} else {
				vdg->render_mode = vdg->GM0 ? VDG_RENDER_RG : VDG_RENDER_CG;
				vdg->fg_colour = !vdg->CSSb ? VDG_GREEN : VDG_WHITE;
				vdg->bg_colour = !vdg->CSSb ? VDG_DARK_GREEN : VDG_BLACK;
			}
		}

		if (vdg->is_32byte) {
			switch (vdg->render_mode) {
			case VDG_RENDER_SG:
				*(vdg->pixel) = (vdg->vram_sg_data&0x80) ? vdg->s_fg_colour : vdg->s_bg_colour;
				*(vdg->pixel+1) = (vdg->vram_sg_data&0x40) ? vdg->s_fg_colour : vdg->s_bg_colour;
				break;
			case VDG_RENDER_CG:
				*(vdg->pixel) = *(vdg->pixel+1) = vdg->cg_colours + ((vdg->vram_g_data & 0xc0) >> 6);
				break;
			case VDG_RENDER_RG:
				*(vdg->pixel) = (vdg->vram_g_data&0x80) ? vdg->fg_colour : vdg->bg_colour;
				*(vdg->pixel+1) = (vdg->vram_g_data&0x40) ? vdg->fg_colour : vdg->bg_colour;
				break;
			}
			vdg->pixel += 2;
			vdg->beam_pos += 2;
		} else {
			switch (vdg->render_mode) {
			case VDG_RENDER_SG:
				*(vdg->pixel) = *(vdg->pixel+1) = (vdg->vram_sg_data&0x80) ? vdg->s_fg_colour : vdg->s_bg_colour;
				*(vdg->pixel+2) = *(vdg->pixel+3) = (vdg->vram_sg_data&0x40) ? vdg->s_fg_colour : vdg->s_bg_colour;
				break;
			case VDG_RENDER_CG:
				*(vdg->pixel) = *(vdg->pixel+1) = *(vdg->pixel+2) = *(vdg->pixel+3) = vdg->cg_colours + ((vdg->vram_g_data & 0xc0) >> 6);
				break;
			case VDG_RENDER_RG:
				*(vdg->pixel) = *(vdg->pixel+1) = (vdg->vram_g_data&0x80) ? vdg->fg_colour : vdg->bg_colour;
				*(vdg->pixel+2) = *(vdg->pixel+3) = (vdg->vram_g_data&0x40) ? vdg->fg_colour : vdg->bg_colour;
				break;
			}
			vdg->pixel += 4;
			vdg->beam_pos += 4;
		}

		vdg->vram_bit -= 2;
		if (vdg->vram_bit == 0) {
			vdg->vram_remaining--;
		}
		vdg->vram_g_data <<= 2;
		vdg->vram_sg_data <<= 2;
		if (vdg->beam_pos >= beam_to)
			return;
	}

	while (vdg->rborder_remaining > 0) {
		if ((vdg->beam_pos & 7) == 4) {
			vdg->CSSa = vdg->CSS;
			vdg->EXTa = vdg->EXT;
		}
		if (vdg->beam_pos == 316) {
			vdg->CSSb = vdg->CSSa;
			vdg->EXTb = vdg->EXTa;
			vdg->text_border_colour = !vdg->CSSb ? VDG_GREEN : vdg->bright_orange;
		}
		vdg->border_colour = vdg->nA_G ? vdg->cg_colours : (vdg->text_border ? vdg->text_border_colour : VDG_BLACK);
		*(vdg->pixel++) = vdg->border_colour;
		vdg->beam_pos++;
		vdg->rborder_remaining--;
		if (vdg->beam_pos >= beam_to)
			return;
	}

	// If a program switches to 32 bytes per line mid-scanline, the whole
	// scanline might not have been rendered:
	while (vdg->beam_pos < (VDG_RIGHT_BORDER_END - VDG_LEFT_BORDER_START)) {
		*(vdg->pixel++) = VDG_BLACK;
		vdg->beam_pos++;
	}
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static void dummy_fetch_bytes(int nbytes, uint8_t *dest) {
	memset(dest, 0, nbytes * 2);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

struct MC6847 *mc6847_new(_Bool t1) {
	struct MC6847_private *vdg = xzalloc(sizeof(*vdg));
	vdg->is_t1 = t1;
	vdg->vram_ptr = vdg->vram;
	vdg->pixel = vdg->pixel_data + VDG_LEFT_BORDER_START;
	vdg->signal_hs = DELEGATE_DEFAULT1(void, bool);
	vdg->signal_fs = DELEGATE_DEFAULT1(void, bool);
	vdg->fetch_bytes = dummy_fetch_bytes;
	event_init(&vdg->hs_fall_event, DELEGATE_AS0(void, do_hs_fall, vdg));
	event_init(&vdg->hs_rise_event, DELEGATE_AS0(void, do_hs_rise, vdg));
	return (struct MC6847 *)vdg;
}

void mc6847_free(struct MC6847 *vdgp) {
	struct MC6847_private *vdg = (struct MC6847_private *)vdgp;
	event_dequeue(&vdg->hs_fall_event);
	event_dequeue(&vdg->hs_rise_event);
	free(vdg);
}

void mc6847_set_fetch_bytes(struct MC6847 *vdgp, void (*d)(int, uint8_t *)) {
	struct MC6847_private *vdg = (struct MC6847_private *)vdgp;
	vdg->fetch_bytes = d;
}

void mc6847_set_signal_hs(struct MC6847 *vdgp, DELEGATE_T1(void, bool) d) {
	struct MC6847_private *vdg = (struct MC6847_private *)vdgp;
	vdg->signal_hs = d;
}

void mc6847_set_signal_fs(struct MC6847 *vdgp, DELEGATE_T1(void, bool) d) {
	struct MC6847_private *vdg = (struct MC6847_private *)vdgp;
	vdg->signal_fs = d;
}

void mc6847_reset(struct MC6847 *vdgp) {
	struct MC6847_private *vdg = (struct MC6847_private *)vdgp;
	video_module->vsync();
	memset(vdg->pixel_data, VDG_BLACK, sizeof(vdg->pixel_data));
	vdg->pixel = vdg->pixel_data + VDG_LEFT_BORDER_START;
	vdg->frame = 0;
	vdg->scanline = 0;
	vdg->row = 0;
	vdg->scanline_start = event_current_tick;
	vdg->hs_fall_event.at_tick = event_current_tick + VDG_CYCLES(VDG_LINE_DURATION);
	event_queue(&MACHINE_EVENT_LIST, &vdg->hs_fall_event);
	// 6847T1 doesn't appear to do bright orange:
	vdg->bright_orange = vdg->is_t1 ? VDG_ORANGE : VDG_BRIGHT_ORANGE;
	mc6847_set_mode(vdgp, 0);
	vdg->beam_pos = 0;
	vdg->vram_ptr = vdg->vram;
	vdg->vram_bit = 0;
	vdg->lborder_remaining = VDG_tLB;
	vdg->vram_remaining = vdg->is_32byte ? 32 : 16;
	vdg->rborder_remaining = VDG_tRB;
}

void mc6847_set_inverted_text(struct MC6847 *vdgp, _Bool invert) {
	struct MC6847_private *vdg = (struct MC6847_private *)vdgp;
	vdg->inverted_text = invert;
}

void mc6847_set_mode(struct MC6847 *vdgp, unsigned mode) {
	struct MC6847_private *vdg = (struct MC6847_private *)vdgp;
	/* Render scanline so far before changing modes */
	if (vdg->frame == 0 && vdg->scanline >= VDG_ACTIVE_AREA_START && vdg->scanline < VDG_ACTIVE_AREA_END) {
		render_scanline(vdg);
	}

	unsigned GM = (mode >> 4) & 7;
	vdg->GM0 = mode & 0x10;
	vdg->CSS = mode & 0x08;
	vdg->EXT = mode & 0x100;
	_Bool new_nA_G = mode & 0x80;

	vdg->inverse_text = vdg->is_t1 && (GM & 2);
	vdg->text_border = vdg->is_t1 && !vdg->inverse_text && (GM & 4);
	vdg->text_border_colour = !vdg->CSSb ? VDG_GREEN : vdg->bright_orange;

	/* If switching from graphics to alpha/semigraphics */
	if (vdg->nA_G && !new_nA_G) {
		vdg->row = 0;
		vdg->render_mode = VDG_RENDER_RG;
		if (vdg->nA_S) {
			vdg->vram_g_data = 0x3f;
			vdg->fg_colour = VDG_GREEN;
			vdg->bg_colour = VDG_DARK_GREEN;
		} else {
			vdg->fg_colour = !vdg->CSSb ? VDG_GREEN : vdg->bright_orange;
			vdg->bg_colour = !vdg->CSSb ? VDG_DARK_GREEN : VDG_DARK_ORANGE;
		}
	}
	if (!vdg->nA_G && new_nA_G) {
		vdg->border_colour = vdg->cg_colours;
		vdg->fg_colour = !vdg->CSSb ? VDG_GREEN : VDG_WHITE;
		vdg->bg_colour = !vdg->CSSb ? VDG_DARK_GREEN : VDG_BLACK;
	}
	vdg->nA_G = new_nA_G;

	if (vdg->nA_G) {
		vdg->render_mode = vdg->GM0 ? VDG_RENDER_RG : VDG_RENDER_CG;
	} else {
		vdg->border_colour = vdg->text_border ? vdg->text_border_colour : VDG_BLACK;
	}

	vdg->is_32byte = !vdg->nA_G || !(GM == 0 || (vdg->GM0 && GM != 7));
}

void mc6847_set_ext_charset(struct MC6847 *vdgp, uint8_t *rom) {
	struct MC6847_private *vdg = (struct MC6847_private *)vdgp;
	vdg->ext_charset = rom;
}
