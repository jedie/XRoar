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

#include "video.h"
#include "m6809.h"
#include "sam.h"
#include "vdg.h"
#include "pia.h"
#include "logging.h"
#include "types.h"

#include "vdg_bitmaps.c"

void (*vdg_render_scanline)(void);

void vdg_init(void) {
	video_artifact_mode = 0;
	vdg_render_scanline = video_module->vdg_render_sg4;
}

void vdg_reset(void) {
	video_module->vdg_reset();
	vdg_set_mode();
}

void vdg_vsync(void) {
	video_module->vdg_vsync();
	sam_vdg_fsync();
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
