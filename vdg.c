/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2005  Ciaran Anscomb
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
#include "events.h"
#include "logging.h"
#include "m6809.h"
#include "pia.h"
#include "sam.h"
#include "vdg.h"
#include "video.h"
#include "xroar.h"

#include "vdg_bitmaps.c"

Cycle scanline_start;
int beam_pos; 

static void (*vdg_render_scanline)(void);
static void vdg_hsync(void);
static event_t *hsync_event;
static int_least16_t scanline;
static unsigned int line_pulse_count;

void vdg_init(void) {
	vdg_render_scanline = video_module->vdg_render_sg4;
	hsync_event = event_new();
	hsync_event->dispatch = vdg_hsync;
}

void vdg_reset(void) {
	video_module->vdg_reset();
	scanline = 0;
	line_pulse_count = 0;
	hsync_event->at_cycle = current_cycle + CYCLES_PER_SCANLINE;
	event_queue(hsync_event);
	scanline_start = current_cycle;
	vdg_set_mode();
	beam_pos = 0;
}

static void vdg_hsync(void) {
	/* XXX Hack: this shouldn't be done here really */
	if (cart_filename) PIA_SET_P1CB1;
	if (line_pulse_count) {
		line_pulse_count--;
		if (line_pulse_count == 0)
			vdg_set_mode();
#ifndef HAVE_GP32
		else
			video_module->render_border();
#endif
		goto done_line_pulse;
	}
	if (scanline >= FIRST_DISPLAYED_SCANLINE) {
		if (scanline < (FIRST_DISPLAYED_SCANLINE + 24)) {
#ifndef HAVE_GP32
			if (IS_NTSC)
				video_module->render_border();
#endif
		} else {
			if (scanline < (216 + FIRST_DISPLAYED_SCANLINE)) {
#ifdef HAVE_GP32
				if ((scanline & 3) == 0)
#endif
					vdg_render_scanline();
			} else {
				if (scanline < (240 + FIRST_DISPLAYED_SCANLINE)) {
#ifndef HAVE_GP32
					if (IS_NTSC)
						video_module->render_border();
#endif
				}
			}
		}
	}
	PIA_RESET_P0CA1; PIA_SET_P0CA1;  // XXX
	if (scanline == SCANLINE_OF_FS_IRQ_LOW) {
		PIA_RESET_P0CB1;
	}
	if (scanline == SCANLINE_OF_DELAYED_FS && IS_PAL) {
		line_pulse_count = 25;
	}
	if (scanline == SCANLINE_OF_FS_IRQ_HIGH) {
		PIA_SET_P0CB1;
		if (IS_PAL) {
			line_pulse_count = 25;
		}
	}
	scanline++;
	if (scanline >= TOTAL_SCANLINES_PER_FRAME) {
		sam_vdg_fsync();
		video_module->vdg_vsync();
		scanline = 0;
	}
done_line_pulse:
	beam_pos = 0;
	if (line_pulse_count) {
		hsync_event->at_cycle += CYCLES_PER_LINE_PULSE;
		scanline_start += CYCLES_PER_LINE_PULSE;
	} else {
		hsync_event->at_cycle += CYCLES_PER_SCANLINE;
		scanline_start += CYCLES_PER_SCANLINE;
	}
	event_queue(hsync_event);
}

void vdg_set_mode(void) {
	unsigned int mode;
	if (line_pulse_count)
		return;
#ifndef HAVE_GP32
	if (scanline >= (FIRST_DISPLAYED_SCANLINE + 24) && scanline < (216 + FIRST_DISPLAYED_SCANLINE)) {
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
