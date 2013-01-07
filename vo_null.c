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

#include "module.h"

static void no_op(void);
static void no_op_render(uint8_t *);

VideoModule video_null_module = {
	.common = { .name = "null", .description = "No video" },
	.vsync = no_op,
	.render_scanline = no_op_render,
};

static void no_op(void) {
}

static void no_op_render(uint8_t *scanline_data) {
	(void)scanline_data;
}
