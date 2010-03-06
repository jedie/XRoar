/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2010  Ciaran Anscomb
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

#include "types.h"
#include "events.h"
#include "logging.h"
#include "m6809.h"
#include "machine.h"
#include "mc6821.h"
#include "module.h"
#include "sam.h"
#include "vdg.h"
#include "xroar.h"

/* Offset to account for unseen portion of left border */
#define SCAN_OFFSET (VDG_LEFT_BORDER_START - VDG_LEFT_BORDER_UNSEEN)

static Cycle scanline_start;
static int is_32byte;
static void render_scanline(void);

#ifndef FAST_VDG
static int beam_pos;
static int inhibit_mode_change;
# define SET_BEAM_POS(v) do { beam_pos = (v); } while (0)
#else
# define SET_BEAM_POS(v)
# define beam_to (320)
#endif

#ifdef HAVE_NDS
uint8_t scanline_data[6154];
static uint8_t *scanline_data_ptr;
#endif

static int scanline;
static int frame;

static event_t hs_fall_event, hs_rise_event;
static event_t fs_fall_event, fs_rise_event;
static void do_hs_fall(void);
static void do_hs_rise(void);
static void do_fs_fall(void);
static void do_fs_rise(void);

#define SCANLINE(s) ((s) % VDG_FRAME_DURATION)

#ifdef HAVE_NDS
int nds_update_screen = 0;
#endif

void vdg_init(void) {
	event_init(&hs_fall_event);
	hs_fall_event.dispatch = do_hs_fall;
	event_init(&hs_rise_event);
	hs_rise_event.dispatch = do_hs_rise;
	event_init(&fs_fall_event);
	fs_fall_event.dispatch = do_fs_fall;
	event_init(&fs_rise_event);
	fs_rise_event.dispatch = do_fs_rise;
}

void vdg_reset(void) {
	video_module->vdg_vsync();
	scanline = 0;
	scanline_start = current_cycle;
	hs_fall_event.at_cycle = current_cycle + VDG_LINE_DURATION;
	event_queue(&MACHINE_EVENT_LIST, &hs_fall_event);
	vdg_set_mode();
	frame = 0;
	SET_BEAM_POS(0);
#ifndef FAST_VDG
	inhibit_mode_change = 0;
#endif
#ifdef HAVE_NDS
	scanline_data_ptr = scanline_data;
#endif
}

static void do_hs_fall(void) {
	/* Finish rendering previous scanline */
#ifdef HAVE_GP32
	/* GP32 renders 4 scanlines at once */
	if (frame == 0 && scanline >= VDG_ACTIVE_AREA_START
			&& scanline < VDG_ACTIVE_AREA_END
			&& (scanline & 3) == ((VDG_ACTIVE_AREA_START+3)&3)
			) {
		video_module->render_scanline();
	}
#elif defined (HAVE_NDS)
	if (scanline >= VDG_ACTIVE_AREA_START
			&& scanline < VDG_ACTIVE_AREA_END) {
		render_scanline();
		sam_vdg_hsync();
	}
#elif !defined(HAVE_NDS)  /* NDS video module does its own thing */
	/* Normal code */
	if (frame == 0 && scanline >= (VDG_TOP_BORDER_START + 1)) {
		if (scanline < VDG_ACTIVE_AREA_START) {
			video_module->render_border();
		} else if (scanline < VDG_ACTIVE_AREA_END) {
			render_scanline();
			sam_vdg_hsync();
			video_module->hsync();
		} else if (scanline < (VDG_BOTTOM_BORDER_END - 2)) {
			video_module->render_border();
		}
	}
#endif
	/* Next scanline */
	scanline = (scanline + 1) % VDG_FRAME_DURATION;
	scanline_start = hs_fall_event.at_cycle;
	SET_BEAM_POS(0);
	PIA_RESET_Cx1(PIA0.a);
#ifdef FAST_VDG
	/* Faster, less accurate timing for GP32/NDS */
	PIA_SET_Cx1(PIA0.a);
#else
	/* Everything else schedule HS rise for later */
	hs_rise_event.at_cycle = scanline_start + VDG_HS_RISING_EDGE;
	event_queue(&MACHINE_EVENT_LIST, &hs_rise_event);
#endif
	hs_fall_event.at_cycle = scanline_start + VDG_LINE_DURATION;
	/* Frame sync */
	if (scanline == SCANLINE(VDG_VBLANK_START)) {
		sam_vdg_fsync();
#ifndef HAVE_NDS
		frame--;
		if (frame < 0)
			frame = xroar_frameskip;
		if (frame == 0)
			video_module->vdg_vsync();
#else
		scanline_data_ptr = scanline_data;
#endif
	}
#ifndef FAST_VDG
	/* Enable mode changes at beginning of active area */
	if (scanline == SCANLINE(VDG_ACTIVE_AREA_START)) {
		inhibit_mode_change = 0;
		vdg_set_mode();
	}
#endif
	/* FS falling edge at end of this scanline */
	if (scanline == SCANLINE(VDG_ACTIVE_AREA_END - 1)) {
#ifdef HAVE_NDS
		nds_update_screen = 1;
#endif
		fs_fall_event.at_cycle = scanline_start + VDG_LINE_DURATION;
		event_queue(&MACHINE_EVENT_LIST, &fs_fall_event);
	}
#ifndef FAST_VDG
	/* Disable mode changes after end of active area */
	if (scanline == SCANLINE(VDG_ACTIVE_AREA_END)) {
		inhibit_mode_change = 1;
	}
#endif
	/* PAL delay 24 lines after FS falling edge */
	if (IS_PAL && (scanline == SCANLINE(VDG_ACTIVE_AREA_END + 23))) {
		hs_fall_event.at_cycle += 25 * VDG_PAL_PADDING_LINE;
	}
	/* FS rising edge at end of this scanline */
	if (scanline == SCANLINE(VDG_ACTIVE_AREA_END + 31)) {
		/* Fig. 8, VDG data sheet: tWFS = 32 * (227.5 * 1/f) */
		fs_rise_event.at_cycle = scanline_start + VDG_LINE_DURATION;
		event_queue(&MACHINE_EVENT_LIST, &fs_rise_event);
		/* PAL delay after FS rising edge */
		if (IS_PAL) {
			hs_fall_event.at_cycle += 25 * VDG_PAL_PADDING_LINE;
		}
	}
	event_queue(&MACHINE_EVENT_LIST, &hs_fall_event);
}

static void do_hs_rise(void) {
	PIA_SET_Cx1(PIA0.a);
}

static void do_fs_fall(void) {
	PIA_RESET_Cx1(PIA0.b);
}

static void do_fs_rise(void) {
	PIA_SET_Cx1(PIA0.b);
}

/* Two versions of render_scanline(): first accounts for mid-scanline mode
 * changes and only fetches as many bytes from the SAM as required, second
 * does a whole scanline at a time. */

#ifndef HAVE_NDS

#ifndef FAST_VDG

static void render_scanline(void) {
	uint8_t scanline_data[42];
	int beam_to = (current_cycle - scanline_start - SCAN_OFFSET) / 2;
	if (beam_to < 0)
		return;
	if (beam_to > 32 && beam_to < 288) {
		if (!is_32byte)
			beam_to &= ~15;
		else
			beam_to &= ~7;
	}
	if (beam_to <= beam_pos)
		return;
	if (beam_to > 32 && beam_pos < 288) {
		int draw_to = (beam_to > 288) ? 288 : beam_to;
		if (beam_pos < 32)
			beam_pos = 32;
		if (!is_32byte) {
			int nbytes = (draw_to - beam_pos) >> 4;
			sam_vdg_bytes(nbytes, scanline_data);
			if (draw_to == 288)
				sam_vdg_bytes(6, NULL);
		} else {
			int nbytes = (draw_to - beam_pos) >> 3;
			sam_vdg_bytes(nbytes, scanline_data);
			if (draw_to == 288)
				sam_vdg_bytes(10, NULL);
		}
	}
	beam_pos = beam_to;
	video_module->render_scanline(scanline_data, beam_to);
}

#else  /* ndef FAST_VDG */

static void render_scanline(void) {
	uint8_t scanline_data[32];
	if (!is_32byte) {
		sam_vdg_bytes(16, scanline_data);
		sam_vdg_bytes(6, NULL);
	} else {
		sam_vdg_bytes(32, scanline_data);
		sam_vdg_bytes(10, NULL);
	}
	video_module->render_scanline(scanline_data);
}

#endif  /* ndef FAST_VDG */

#else  /* HAVE_NDS */

static void render_scanline(void) {
	if (!is_32byte) {
		sam_vdg_bytes(22, scanline_data_ptr);
		scanline_data_ptr += 16;
	} else {
		sam_vdg_bytes(42, scanline_data_ptr);
		scanline_data_ptr += 32;
	}
}

#endif  /* ndef HAVE_NDS */

void vdg_set_mode(void) {
	int mode;
#ifndef FAST_VDG
	/* No need to inhibit mode changes during borders on GP32/NDS, as
	 * they're not rendered anyway. */
	if (inhibit_mode_change)
		return;
	/* Render scanline so far before changing modes (disabled for speed
	 * on GP32/NDS). */
	if (frame == 0 && scanline >= VDG_ACTIVE_AREA_START && scanline < VDG_ACTIVE_AREA_END) {
		render_scanline();
	}
#endif
	/* 16 or 32 byte mode? */
	mode = PIA1.b.port_output;
	switch ((mode & 0xf0) >> 4) {
		case 8: case 9: case 11: case 13:
			is_32byte = 0;
			break;
		default:
			is_32byte = 1;
			break;
	}
	/* Update video module */
	video_module->vdg_set_mode(mode);
}
