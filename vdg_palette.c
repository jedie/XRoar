/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2011  Ciaran Anscomb
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "vdg_palette.h"

static struct vdg_palette palette_templates[] = {

	/* The "typical" figures from the VDG data sheet */
	{
		.name = "ideal",
		.description = "Typical values from VDG data sheet",
		.blank_y = 0.77,
		.white_y = 0.42,
		.black_level = 0.,
		.rgb_black_level = 0.,
		.palette = {
			{ .y = 0.54, .chb = 1.50, .b = 1.00, .a = 1.00 },
			{ .y = 0.42, .chb = 1.50, .b = 1.00, .a = 1.50 },
			{ .y = 0.65, .chb = 1.50, .b = 2.00, .a = 1.50 },
			{ .y = 0.65, .chb = 1.50, .b = 1.50, .a = 2.00 },
			{ .y = 0.42, .chb = 1.50, .b = 1.50, .a = 1.50 },
			{ .y = 0.54, .chb = 1.50, .b = 1.50, .a = 1.00 },
			{ .y = 0.54, .chb = 1.50, .b = 2.00, .a = 2.00 },
			{ .y = 0.54, .chb = 1.50, .b = 1.00, .a = 2.00 },
			{ .y = 0.72, .chb = 1.50, .b = 1.50, .a = 1.50 },
			{ .y = 0.72, .chb = 1.50, .b = 1.00, .a = 1.00 },
			{ .y = 0.72, .chb = 1.50, .b = 1.00, .a = 2.00 },
			{ .y = 0.42, .chb = 1.50, .b = 1.00, .a = 2.00 },
		}
	},

	/* First real Dragon 64 */
	{
		.name = "dragon64",
		.description = "Measured from a real Dragon 64",
		.blank_y = 0.69,  /* XXX remeasure */
		.white_y = 0.39,
		.black_level = 0.,
		.palette = {
			{ .y = 0.49, .chb = 1.38, .b = 0.82, .a = 0.89 },
			{ .y = 0.39, .chb = 1.38, .b = 0.82, .a = 1.38 },
			{ .y = 0.58, .chb = 1.34, .b = 1.68, .a = 1.37 },
			{ .y = 0.58, .chb = 1.30, .b = 1.26, .a = 1.82 },
			{ .y = 0.39, .chb = 1.30, .b = 1.26, .a = 1.33 },
			{ .y = 0.49, .chb = 1.30, .b = 1.26, .a = 0.92 },
			{ .y = 0.49, .chb = 1.33, .b = 1.67, .a = 1.75 },
			{ .y = 0.49, .chb = 1.36, .b = 0.82, .a = 1.85 },
			{ .y = 0.64, .chb = 1.30, .b = 1.26, .a = 1.33 },
			{ .y = 0.64, .chb = 1.38, .b = 0.82, .a = 0.89 },
			{ .y = 0.64, .chb = 1.37, .b = 0.81, .a = 1.85 },
			{ .y = 0.39, .chb = 1.37, .b = 0.81, .a = 1.85 },
		}
	},

	/* Real Tano Dragon - close to ideal! */
	{
		.name = "tano",
		.description = "Measured from a real Tano Dragon",
		.blank_y = 0.75,  /* XXX remeasure */
		.white_y = 0.40,
		.black_level = 0.,
		.palette = {
			{ .y = 0.52, .chb = 1.50, .b = 1.02, .a = 1.02 },
			{ .y = 0.40, .chb = 1.50, .b = 1.02, .a = 1.50 },
			{ .y = 0.62, .chb = 1.50, .b = 1.98, .a = 1.50 },
			{ .y = 0.62, .chb = 1.50, .b = 1.50, .a = 1.98 },
			{ .y = 0.40, .chb = 1.50, .b = 1.50, .a = 1.50 },
			{ .y = 0.52, .chb = 1.50, .b = 1.50, .a = 1.02 },
			{ .y = 0.52, .chb = 1.50, .b = 1.98, .a = 1.98 },
			{ .y = 0.52, .chb = 1.50, .b = 1.02, .a = 1.98 },
			{ .y = 0.70, .chb = 1.50, .b = 1.50, .a = 1.50 },
			{ .y = 0.70, .chb = 1.50, .b = 1.02, .a = 1.02 },
			{ .y = 0.70, .chb = 1.50, .b = 1.02, .a = 1.98 },
			{ .y = 0.40, .chb = 1.50, .b = 1.02, .a = 1.98 },
		}
	},

	/* Second real Dragon 64 */
	{
		.name = "d64-2",
		.description = "Measured from a(nother) real Dragon 64",
		.blank_y = 0.74,  /* XXX remeasure */
		.white_y = 0.44,
		.black_level = 0.,
		.palette = {
			{ .y = 0.53, .chb = 1.42, .b = 0.90, .a = 0.95 },
			{ .y = 0.44, .chb = 1.42, .b = 0.90, .a = 1.41 },
			{ .y = 0.62, .chb = 1.40, .b = 1.76, .a = 1.40 },
			{ .y = 0.62, .chb = 1.36, .b = 1.32, .a = 1.87 },
			{ .y = 0.44, .chb = 1.36, .b = 1.32, .a = 1.38 },
			{ .y = 0.53, .chb = 1.36, .b = 1.32, .a = 0.97 },
			{ .y = 0.53, .chb = 1.38, .b = 1.73, .a = 1.81 },
			{ .y = 0.53, .chb = 1.40, .b = 0.88, .a = 1.89 },
			{ .y = 0.69, .chb = 1.36, .b = 1.32, .a = 1.38 },
			{ .y = 0.69, .chb = 1.42, .b = 0.90, .a = 0.95 },
			{ .y = 0.69, .chb = 1.40, .b = 0.88, .a = 1.89 },
			{ .y = 0.44, .chb = 1.40, .b = 0.88, .a = 1.89 },
		}
	},

	/* Third real Dragon 64 */
	{
		.name = "d64-3",
		.description = "Measured from a(nother) real Dragon 64",
		.blank_y = 0.74,  /* XXX remeasure */
		.white_y = 0.44,
		.black_level = 0.,
		.palette = {
			{ .y = 0.53, .chb = 1.42, .b = 0.90, .a = 0.95 },
			{ .y = 0.43, .chb = 1.42, .b = 0.90, .a = 1.41 },
			{ .y = 0.62, .chb = 1.40, .b = 1.76, .a = 1.40 },
			{ .y = 0.62, .chb = 1.36, .b = 1.32, .a = 1.87 },
			{ .y = 0.44, .chb = 1.36, .b = 1.32, .a = 1.38 },
			{ .y = 0.53, .chb = 1.36, .b = 1.32, .a = 0.97 },
			{ .y = 0.53, .chb = 1.38, .b = 1.73, .a = 1.81 },
			{ .y = 0.53, .chb = 1.40, .b = 0.88, .a = 1.89 },
			{ .y = 0.69, .chb = 1.36, .b = 1.32, .a = 1.38 },
			{ .y = 0.69, .chb = 1.42, .b = 0.90, .a = 0.95 },
			{ .y = 0.69, .chb = 1.40, .b = 0.88, .a = 1.89 },
			{ .y = 0.44, .chb = 1.40, .b = 0.88, .a = 1.89 },
		}
	},

};
#define NUM_PALETTE_TEMPLATES (int)(sizeof(palette_templates) / sizeof(struct vdg_palette))

static int num_palettes = NUM_PALETTE_TEMPLATES;

/**************************************************************************/

int vdg_palette_count(void) {
	return num_palettes;
}

struct vdg_palette *vdg_palette_index(int i) {
	if (i < 0 || i >= num_palettes) {
		return NULL;
	}
	return &palette_templates[i];
}

struct vdg_palette *vdg_palette_by_name(const char *name) {
	int count, i;
	if (!name) return NULL;
	count = vdg_palette_count();
	for (i = 0; i < count; i++) {
		struct vdg_palette *vp = vdg_palette_index(i);
		if (0 == strcmp(vp->name, name)) {
			return vp;
		}
	}
	return NULL;
}

/* ---------------------------------------------------------------------- */

/* Map Y'U'V' from palette to pixel value */
void vdg_palette_RGB(struct vdg_palette *vp, int is_pal, int colour,
                     float *Rout, float *Gout, float *Bout) {
	float blank_y = vp->blank_y;
	float white_y = vp->white_y;
	float black_level = vp->black_level;
	float rgb_black_level = vp->rgb_black_level;
	float y = vp->palette[colour].y;
	float chb = vp->palette[colour].chb;
	float b_y = vp->palette[colour].b - chb;
	float r_y = vp->palette[colour].a - chb;

	float scale_y = 1. / (blank_y - white_y);
	y = black_level + (blank_y - y) * scale_y;

	float r, g, b;
	float mlaw;
	if (is_pal) {
		float u = 0.493 * b_y;
		float v = 0.877 * r_y;
		r = 1.0 * y + 0.000 * u + 1.140 * v;
		g = 1.0 * y - 0.396 * u - 0.581 * v;
		b = 1.0 * y + 2.029 * u + 0.000 * v;
		mlaw = 2.8;
	} else {
		float i = -0.27 * b_y + 0.74 * r_y;
		float q =  0.41 * b_y + 0.48 * r_y;
		r = 1.0 * y + 0.956 * i + 0.621 * q;
		g = 1.0 * y - 0.272 * i - 0.647 * q;
		b = 1.0 * y - 1.105 * i + 1.702 * q;
		mlaw = 2.2;
	}

	/* Those are corrected (non-linear) values, but graphics card
	 * colourspaces tend to be linear, so un-correct here.  Proper
	 * colourspace conversion (e.g., to sRGB) to come later.  */
	if (r <= (0.018 * 4.5)) {
		*Rout = r / 4.5;
	} else {
		*Rout = powf((r+0.099)/(1.+0.099), mlaw);
	}
	if (g <= (0.018 * 4.5)) {
		*Gout = g / 4.5;
	} else {
		*Gout = powf((g+0.099)/(1.+0.099), mlaw);
	}
	if (b <= (0.018 * 4.5)) {
		*Bout = b / 4.5;
	} else {
		*Bout = powf((b+0.099)/(1.+0.099), mlaw);
	}
	*Rout += rgb_black_level;
	*Gout += rgb_black_level;
	*Bout += rgb_black_level;

	if (*Rout < 0.0) *Rout = 0.0; if (*Rout > 1.0) *Rout = 1.0;
	if (*Gout < 0.0) *Gout = 0.0; if (*Gout > 1.0) *Gout = 1.0;
	if (*Bout < 0.0) *Bout = 0.0; if (*Bout > 1.0) *Bout = 1.0;
}
