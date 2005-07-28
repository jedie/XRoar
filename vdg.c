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

static void (*vdg_render_scanline)(void);
static void vdg_hsync(void);
static event_t *hsync_event;
static int_least16_t scanline;

void vdg_init(void) {
	video_artifact_mode = 0;
	vdg_render_scanline = video_module->vdg_render_sg4;
	hsync_event = event_new();
	hsync_event->dispatch = vdg_hsync;
}

void vdg_reset(void) {
	video_module->vdg_reset();
	vdg_set_mode();
	scanline = 0;
	hsync_event->at_cycle = current_cycle + CYCLES_PER_SCANLINE;
	event_queue(hsync_event);
}

static void vdg_hsync(void) {
	if (scanline >= TOP_BORDER_OFFSET) {
		if (scanline < (TOP_BORDER_OFFSET + 24)) {
#ifndef HAVE_GP32
			video_module->render_border();
#endif
		} else {
			if (scanline < (216 + TOP_BORDER_OFFSET)) {
#ifdef HAVE_GP32
				if ((scanline & 3) == 0)
#endif
					vdg_render_scanline();
			} else {
				if (scanline < (240 + TOP_BORDER_OFFSET)) {
#ifndef HAVE_GP32
					video_module->render_border();
#endif
				}
			}
		}
	}
	PIA_SET_P0CA1;
	scanline++;
	if (scanline == ACTIVE_SCANLINES_PER_FRAME) {
		PIA_SET_P0CB1;
	}
	if (scanline >= TOTAL_SCANLINES_PER_FRAME) {
		sam_vdg_fsync();
		video_module->vdg_vsync();  // XXX
		scanline = 0;
	}
	hsync_event->at_cycle += CYCLES_PER_SCANLINE;
	event_queue(hsync_event);
}

void vdg_set_mode(void) {
	unsigned int mode = PIA_1B.port_output;
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
