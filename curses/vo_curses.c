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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <curses.h>

#include "types.h"
#include "logging.h"
#include "module.h"
#include "xroar.h"

static int init(void);
static void shutdown(void);
static void reset(void);
static void vsync(void);
static void hsync(void);
static void set_mode(unsigned int mode);
static void render_border(void);

static int old_mode;
static int old_screen[16][32];
static int textline;
static int subline;
static int colourbase;

VideoModule video_curses_module = {
	.common = { .name = "curses", .description = "Curses video",
	            .init = init, .shutdown = shutdown },
	.reset = reset, .vsync = vsync, .hsync = hsync, .set_mode = set_mode,
	.render_border = render_border
};

static int init(void) {
	LOG_DEBUG(2,"Initialising curses video driver\n");
	memset(old_screen, 0, sizeof(old_screen));
	clear();
	old_mode = 0xff;
	set_mode(0);
	init_pair(1, COLOR_BLACK, COLOR_GREEN);
	init_pair(2, COLOR_BLACK, COLOR_YELLOW);
	init_pair(3, COLOR_BLACK, COLOR_BLUE);
	init_pair(4, COLOR_BLACK, COLOR_RED);
	init_pair(5, COLOR_BLACK, COLOR_WHITE);
	init_pair(6, COLOR_BLACK, COLOR_CYAN);
	init_pair(7, COLOR_BLACK, COLOR_MAGENTA);
	if (can_change_color()) {
		init_color(COLOR_BLACK, 0, 125, 0);
		init_color(COLOR_GREEN, 0, 1000, 0);
		init_color(COLOR_YELLOW, 1000, 1000, 0);
		init_color(COLOR_BLUE, 0, 0, 1000);
		init_color(COLOR_RED, 1000, 0, 0);
		init_color(COLOR_WHITE, 1000, 1000, 1000);
		init_color(COLOR_CYAN, 1000, 0, 1000);
		init_color(COLOR_MAGENTA, 0, 1000, 1000);
		init_color(8, 1000, 647, 0);
		init_color(9, 647, 78, 0);
		init_pair(8, 9, 8);
	} else {
		init_pair(8, COLOR_BLACK, COLOR_YELLOW);
	}
	return 0;
}

static void shutdown(void) {
	LOG_DEBUG(2,"Shutting down curses video driver\n");
}

#ifndef FAST_VDG
# define RENDER_ARGS uint8_t *vram_ptr, int beam_to
#else
# define RENDER_ARGS uint8_t *vram_ptr
# define beam_to (320)
#endif

static void render_sg4(RENDER_ARGS);
static void render_cg2(RENDER_ARGS);
static void render_nop(RENDER_ARGS);

static void reset(void) {
	textline = 0;
	subline = 0;
}

static void vsync(void) {
	move(15,32);
	refresh();
	reset();
}

static void set_mode(unsigned int mode) {
	switch ((mode & 0xf0) >> 4) {
		case 0: case 2: case 4: case 6:
			video_curses_module.render_scanline = render_sg4;
			if (mode & 0x08) {
				colourbase = 8;
			} else {
				colourbase = 1;
			}
			break;
		case 1: case 3: case 5: case 7:
			video_curses_module.render_scanline = render_sg4;
			break;
		case 8:
			video_curses_module.render_scanline = render_nop;
			break;
		case 9: case 11: case 13:
			video_curses_module.render_scanline = render_nop;
			break;
		case 10: case 12: case 14:
			video_curses_module.render_scanline = render_cg2;
			colourbase = 1 + ((mode & 0x08) >> 1);
			break;
		case 15: default:
			video_curses_module.render_scanline = render_cg2;
			break;
	}
}

static void render_sg4(RENDER_ARGS) {
	int i;
#ifndef FAST_VDG
	(void)beam_to;
#endif
	if (subline < 11) return;
	for (i = 0; i < 32; i++) {
		int c = vram_ptr[i];
		int attr;
		if ((c & 0x80) == 0x80) {
			attr = COLOR_PAIR(((c & 0x70) >> 4) + 1);
			if ((c & 0x0f) == 0) attr |= A_REVERSE;
			c = ' ';
		} else {
			attr = COLOR_PAIR(colourbase);
			if (!(c & 0x40)) attr |= A_REVERSE;
			c &= 0x3f;
			if (c < 0x20) {
				c += 0x40;
			}
		}
		c |= attr;
		if (c == old_screen[textline][i])
			continue;
		old_screen[textline][i] = c;
		(void)mvaddch(textline, i, c);
	}
}

static void render_cg2(RENDER_ARGS) {
	int i;
#ifndef FAST_VDG
	(void)beam_to;
#endif
	if (subline < 11) return;
	for (i = 0; i < 32; i++) {
		int c = vram_ptr[i];
		int attr;
		attr = COLOR_PAIR((c & 0x03) + colourbase);
		c = ' ';
		c |= attr;
		if (c == old_screen[textline][i])
			continue;
		old_screen[textline][i] = c;
		(void)mvaddch(textline, i, c);
	}
}

static void render_nop(RENDER_ARGS) {
	(void)vram_ptr;
#ifndef FAST_VDG
	(void)beam_to;
#endif
}

static void render_border(void) {
}

static void hsync(void) {
	subline++;
	if (subline > 11) {
		subline = 0;
		textline++;
	}
}
