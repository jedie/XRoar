/*  Copyright 2003-2012 Ciaran Anscomb
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

#include "types.h"

#include "module.h"

#ifndef FAST_VDG
# define RENDER_ARGS uint8_t *vram_ptr, int beam_to
#else
# define RENDER_ARGS uint8_t *vram_ptr
# define beam_to (320)
#endif

static void no_op(void);
static void no_op_unsigned(unsigned int mode);
static void no_op_render(RENDER_ARGS);

VideoModule video_null_module = {
	.common = { .name = "null", .description = "No video" },
	.vsync = no_op, .hsync = no_op, .set_mode = no_op_unsigned,
	.render_border = no_op, .render_scanline = no_op_render,
};

static void no_op(void) {
}

static void no_op_unsigned(unsigned int mode) {
	(void)mode;
}

static void no_op_render(RENDER_ARGS) {
	(void)vram_ptr;
	(void)beam_to;
}
