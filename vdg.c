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

#include "types.h"
#include "logging.h"
#include "events.h"
#include "m6809.h"
#include "pia.h"
#include "sam.h"
#include "vdg.h"
#include "xroar.h"

#include "vdg_bitmaps.c"

Cycle scanline_start;
int beam_pos;

static void (*vdg_render_scanline)(void);
static int_least16_t scanline;
#ifndef HAVE_GP32
static int inhibit_mode_change;
#endif
static int frame;

static event_t *hs_fall_event, *hs_rise_event;
static event_t *fs_fall_event, *fs_rise_event;
static void do_hs_fall(void);
static void do_hs_rise(void);
static void do_fs_fall(void);
static void do_fs_rise(void);

#define SCANLINE(s) ((s) % VDG_FRAME_DURATION)

void vdg_init(void) {
	vdg_render_scanline = video_module->vdg_render_sg4;
	hs_fall_event = event_new();
	hs_fall_event->dispatch = do_hs_fall;
	hs_rise_event = event_new();
	hs_rise_event->dispatch = do_hs_rise;
	fs_fall_event = event_new();
	fs_fall_event->dispatch = do_fs_fall;
	fs_rise_event = event_new();
	fs_rise_event->dispatch = do_fs_rise;
}

void vdg_reset(void) {
	video_module->vdg_vsync();
	scanline = 0;
	scanline_start = current_cycle;
	hs_fall_event->at_cycle = current_cycle + VDG_LINE_DURATION;
	event_queue(hs_fall_event);
	vdg_set_mode();
	beam_pos = 0;
#ifndef HAVE_GP32
	inhibit_mode_change = 0;
#endif
	frameskip = requested_frameskip;
	frame = 0;
}

static void do_hs_fall(void) {
	/* Finish rendering previous scanline */
#ifdef HAVE_GP32
	/* GP32-specific code */
	if (frame == 0 && scanline >= VDG_ACTIVE_AREA_START
			&& scanline < VDG_ACTIVE_AREA_END
			&& (scanline & 3) == ((VDG_ACTIVE_AREA_START+3)&3)) {
		vdg_render_scanline();
	}
#else
	/* Normal code */
	if (frame == 0 && scanline >= (VDG_TOP_BORDER_START + 1)) {
		if (scanline < VDG_ACTIVE_AREA_START) {
			video_module->render_border();
		} else if (scanline < VDG_ACTIVE_AREA_END) {
			vdg_render_scanline();
		} else if (scanline < VDG_BOTTOM_BORDER_END) {
			video_module->render_border();
		}
	}
#endif
	/* Next scanline */
	scanline = (scanline + 1) % VDG_FRAME_DURATION;
	scanline_start = hs_fall_event->at_cycle;
	beam_pos = 0;
	PIA_RESET_P0CA1;
#ifdef HAVE_GP32
	/* Faster, less accurate timing for GP32 */
	PIA_SET_P0CA1;
#else
	/* Everything else schedule HS rise for later */
	hs_rise_event->at_cycle = scanline_start + VDG_HS_RISING_EDGE;
	event_queue(hs_rise_event);
#endif
	hs_fall_event->at_cycle = scanline_start + VDG_LINE_DURATION;
	/* Frame sync */
	if (scanline == SCANLINE(VDG_VBLANK_START)) {
		sam_vdg_fsync();
		frame--;
		if (frame < 0)
			frame = frameskip;
		if (frame == 0)
			video_module->vdg_vsync();
	}
#ifndef HAVE_GP32
	/* Enable mode changes at beginning of active area */
	if (scanline == SCANLINE(VDG_ACTIVE_AREA_START)) {
		inhibit_mode_change = 0;
		vdg_set_mode();
	}
#endif
	/* FS falling edge at end of this scanline */
	if (scanline == SCANLINE(VDG_ACTIVE_AREA_END - 1)) {
		fs_fall_event->at_cycle = scanline_start + VDG_LINE_DURATION + 16;
		event_queue(fs_fall_event);
	}
#ifndef HAVE_GP32
	/* Disable mode changes after end of active area */
	if (scanline == SCANLINE(VDG_ACTIVE_AREA_END)) {
		inhibit_mode_change = 1;
	}
#endif
	/* PAL delay 24 lines after FS falling edge */
	if (IS_PAL && (scanline == SCANLINE(VDG_ACTIVE_AREA_END + 23))) {
		hs_fall_event->at_cycle += 25 * VDG_PAL_PADDING_LINE;
	}
	/* FS rising edge at end of this scanline */
	if (scanline == SCANLINE(VDG_ACTIVE_AREA_END + 31)) {
		/* Fig. 8, VDG data sheet: tWFS = 32 * (227.5 * 1/f) */
		fs_rise_event->at_cycle = scanline_start + VDG_LINE_DURATION + 16;
		event_queue(fs_rise_event);
		/* PAL delay after FS rising edge */
		if (IS_PAL) {
			hs_fall_event->at_cycle += 25 * VDG_PAL_PADDING_LINE;
		}
	}
	event_queue(hs_fall_event);
}

static void do_hs_rise(void) {
	PIA_SET_P0CA1;
}

static void do_fs_fall(void) {
	PIA_RESET_P0CB1;
}

static void do_fs_rise(void) {
	PIA_SET_P0CB1;
}

void vdg_set_mode(void) {
	unsigned int mode;
#ifndef HAVE_GP32
	/* No need to inhibit mode changes during borders on GP32, as they're
	 * not rendered anyway. */
	if (inhibit_mode_change)
		return;
	/* Render scanline so far before changing modes (disabled for speed
	 * on GP32). */
	if (frame == 0 && scanline >= VDG_ACTIVE_AREA_START && scanline < VDG_ACTIVE_AREA_END) {
		vdg_render_scanline();
	}
#endif
	mode = PIA_1B.port_output;
	/* Update video module */
	video_module->vdg_set_mode(mode);
	switch ((mode & 0xf0) >> 4) {
		case 0: case 2: case 4: case 6:
			vdg_render_scanline = video_module->vdg_render_sg4;
			break;
		case 1: case 3: case 5: case 7:
			vdg_render_scanline = video_module->vdg_render_sg6;
			break;
		case 8:
			vdg_render_scanline = video_module->vdg_render_cg1;
			break;
		case 9: case 11: case 13:
			vdg_render_scanline = video_module->vdg_render_rg1;
			break;
		case 10: case 12: case 14:
			vdg_render_scanline = video_module->vdg_render_cg2;
			break;
		case 15: default:
			if (video_artifact_mode) {
				vdg_render_scanline = video_module->vdg_render_cg2;
			} else {
				vdg_render_scanline = video_module->vdg_render_rg6;
			}
			break;
	}
}
