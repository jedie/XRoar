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

#include <stdlib.h>
#include <string.h>
#include <nds.h>

#include "types.h"
#include "logging.h"
#include "module.h"
#include "sam.h"
//#include "ui_nds.h"
#include "vdg.h"
#include "xroar.h"

static int init(int argc, char **argv);
static void shutdown(void);
static int set_fullscreen(int fullscreen);
static void vsync(void);
static void set_mode(unsigned int mode);
static void render_sg4(void);
static void render_sg6(void);
static void render_cg1(void);
static void render_rg1(void);
static void render_cg2(void);
static void render_rg6(void);
static void render_border(void);
static void alloc_colours(void);

VideoModule video_nds_module = {
	{ "nds", "NDS video",
	  init, 0, shutdown, NULL },
	NULL, set_fullscreen, 0,
	vsync, set_mode,
	render_sg4, render_sg6, render_cg1,
	render_rg1, render_cg2, render_rg6,
	render_border
};

typedef uint16_t Pixel;
#define NO_BORDER
#define MAPCOLOUR(r,g,b) (RGB15((r)>>3,(g)>>3,(b)>>3))
#define VIDEO_SCREENBASE (VRAM_A)
#define XSTEP 1
#define NEXTLINE 0
#define VIDEO_TOPLEFT (VIDEO_SCREENBASE)
#define VIDEO_VIEWPORT_YOFFSET (0)
#define LOCK_SURFACE 
#define UNLOCK_SURFACE 

#include "video_generic_ops.c"

static int init(int argc, char **argv) {
	(void)argc;
	(void)argv;
	LOG_DEBUG(2,"Initialising NDS video driver\n");
	videoSetMode(MODE_FB0);
	vramSetBankA(VRAM_A_LCD);
	alloc_colours();
	return 0;
}

static void shutdown(void) {
	LOG_DEBUG(2,"Shutting down NDS video driver\n");
}

static int set_fullscreen(int fullscreen) {
	return 0;
}

static void vsync(void) {
	pixel = VIDEO_TOPLEFT + VIDEO_VIEWPORT_YOFFSET;
	subline = 0;
}
