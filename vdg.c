/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2012  Ciaran Anscomb
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 */

#include "config.h"

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

/* Offset to the first displayed pixel.  Accounts for unseen portion of left
 * border.  10 VDG cycles (20 pixels) of fudge factor! */
#define SCAN_OFFSET (VDG_LEFT_BORDER_START + VDG_LEFT_BORDER_UNSEEN - VDG_CYCLES(10))

/* External handler to fetch data for display.  First arg is number of bytes,
 * second a pointer to a buffer to receive them. */
void (*vdg_fetch_bytes)(int, uint8_t *);

static cycle_t scanline_start;
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

static int scanline;
static int frame;

static event_t hs_fall_event, hs_rise_event;
static void do_hs_fall(void);
static void do_hs_rise(void);

#define SCANLINE(s) ((s) % VDG_FRAME_DURATION)

void vdg_init(void) {
	event_init(&hs_fall_event);
	hs_fall_event.dispatch = do_hs_fall;
	event_init(&hs_rise_event);
	hs_rise_event.dispatch = do_hs_rise;
}

void vdg_reset(void) {
	if (video_module->reset) video_module->reset();
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
}

static void do_hs_fall(void) {
	/* Finish rendering previous scanline */
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

	/* HS falling edge */
	PIA_RESET_Cx1(PIA0.a);

	scanline_start = hs_fall_event.at_cycle;
	/* Next HS rise and fall */
	hs_rise_event.at_cycle = scanline_start + VDG_HS_RISING_EDGE;
	hs_fall_event.at_cycle = scanline_start + VDG_LINE_DURATION;

	/* Two delays of 25 scanlines each occur 24 lines after FS falling edge
	 * and at FS rising edge in PAL systems */
	if (IS_PAL) {
		if (scanline == SCANLINE(VDG_ACTIVE_AREA_END + 24)
		    || scanline == SCANLINE(VDG_ACTIVE_AREA_END + 32)) {
			hs_rise_event.at_cycle += 25 * VDG_PAL_PADDING_LINE;
			hs_fall_event.at_cycle += 25 * VDG_PAL_PADDING_LINE;
		}
	}

#ifndef FAST_VDG
	event_queue(&MACHINE_EVENT_LIST, &hs_rise_event);
#else
	/* Faster, less accurate timing */
	PIA_SET_Cx1(PIA0.a);
#endif
	event_queue(&MACHINE_EVENT_LIST, &hs_fall_event);

	/* Next scanline */
	scanline = (scanline + 1) % VDG_FRAME_DURATION;
	SET_BEAM_POS(0);

	if (scanline == SCANLINE(VDG_ACTIVE_AREA_END)) {
		/* FS falling edge */
		PIA_RESET_Cx1(PIA0.b);
	}
	if (scanline == SCANLINE(VDG_ACTIVE_AREA_END + 32)) {
		/* FS rising edge */
		PIA_SET_Cx1(PIA0.b);
	}

	/* Frame sync */
	if (scanline == SCANLINE(VDG_VBLANK_START)) {
		sam_vdg_fsync();
		frame--;
		if (frame < 0)
			frame = xroar_frameskip;
		if (frame == 0)
			video_module->vsync();
	}

#ifndef FAST_VDG
	/* Enable mode changes at beginning of active area */
	if (scanline == SCANLINE(VDG_ACTIVE_AREA_START)) {
		inhibit_mode_change = 0;
		vdg_set_mode();
	}
	/* Disable mode changes after end of active area */
	if (scanline == SCANLINE(VDG_ACTIVE_AREA_END)) {
		inhibit_mode_change = 1;
	}
#endif

}

static void do_hs_rise(void) {
	/* HS rising edge */
	PIA_SET_Cx1(PIA0.a);
}

/* Two versions of render_scanline(): first accounts for mid-scanline mode
 * changes and only fetches as many bytes from the SAM as required, second
 * does a whole scanline at a time. */

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
			vdg_fetch_bytes(nbytes, scanline_data);
			if (draw_to == 288)
				vdg_fetch_bytes(6, NULL);
		} else {
			int nbytes = (draw_to - beam_pos) >> 3;
			vdg_fetch_bytes(nbytes, scanline_data);
			if (draw_to == 288)
				vdg_fetch_bytes(10, NULL);
		}
	}
	beam_pos = beam_to;
	video_module->render_scanline(scanline_data, beam_to);
}

#else  /* ndef FAST_VDG */

static void render_scanline(void) {
	uint8_t scanline_data[32];
	if (!is_32byte) {
		vdg_fetch_bytes(16, scanline_data);
		vdg_fetch_bytes(6, NULL);
	} else {
		vdg_fetch_bytes(32, scanline_data);
		vdg_fetch_bytes(10, NULL);
	}
	video_module->render_scanline(scanline_data);
}

#endif  /* ndef FAST_VDG */

void vdg_set_mode(void) {
	int mode;
#ifndef FAST_VDG
	if (inhibit_mode_change)
		return;
	/* Render scanline so far before changing modes */
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
	video_module->set_mode(mode);
}
