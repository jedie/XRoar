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

#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "events.h"
#include "logging.h"
#include "m6809.h"
#include "machine.h"
#include "mc6821.h"
#include "module.h"
#include "sam.h"
#include "vdg.h"
#include "vdg_bitmaps.h"
#include "xroar.h"

/* Offset to the first displayed pixel.  6 VDG cycles (12 pixels) of fudge
 * factor! */
#define SCAN_OFFSET (VDG_LEFT_BORDER_START - VDG_CYCLES(6))

/* External handler to fetch data for display.  First arg is number of bytes,
 * second a pointer to a buffer to receive them. */
void (*vdg_fetch_bytes)(int, uint8_t *);

static cycle_t scanline_start;
static _Bool is_32byte;
static void render_scanline(void);

static int beam_pos;
static _Bool inhibit_mode_change;
# define SET_BEAM_POS(v) do { beam_pos = (v); } while (0)

static unsigned int colourburst_enabled;
static unsigned int scanline_flags;
static uint8_t pixel_data[372];

static uint8_t *pixel;
static uint8_t bg_colour;
static uint8_t fg_colour;
static uint8_t cg_colours;
static uint8_t border_colour;

/* Scanline rendering routines. */

static void render_sg4(uint8_t *vram_ptr, int nbytes);
static void render_sg6(uint8_t *vram_ptr, int nbytes);
static void render_cg1(uint8_t *vram_ptr, int nbytes);
static void render_rg1(uint8_t *vram_ptr, int nbytes);
static void render_cg2(uint8_t *vram_ptr, int nbytes);
static void render_rg6(uint8_t *vram_ptr, int nbytes);

static void (*render_scanline_data)(uint8_t *vram_ptr, int nbytes);

static int scanline;
static unsigned int subline;
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
	video_module->vsync();
	scanline_flags = VDG_COLOURBURST;
	colourburst_enabled = VDG_COLOURBURST;
	pixel = pixel_data;
	frame = 0;
	scanline = 0;
	subline = 0;
	scanline_start = current_cycle;
	hs_fall_event.at_cycle = current_cycle + VDG_LINE_DURATION;
	event_queue(&MACHINE_EVENT_LIST, &hs_fall_event);
	vdg_set_mode();
	SET_BEAM_POS(0);
	inhibit_mode_change = 0;
}

static void do_hs_fall(void) {
	/* Finish rendering previous scanline */
	/* Normal code */
	if (scanline == VDG_TOP_BORDER_START || scanline == VDG_ACTIVE_AREA_END)
		memset(pixel_data, border_colour, 372);
	if (frame == 0 && scanline >= (VDG_TOP_BORDER_START + 1) && scanline < (VDG_BOTTOM_BORDER_END - 2)) {
		if (scanline >= VDG_ACTIVE_AREA_START && scanline < VDG_ACTIVE_AREA_END) {
			render_scanline();
			sam_vdg_hsync();
			pixel = pixel_data;
			subline++;
			if (subline > 11)
				subline = 0;
		}
		video_module->render_scanline(pixel_data);
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

	event_queue(&MACHINE_EVENT_LIST, &hs_rise_event);
	event_queue(&MACHINE_EVENT_LIST, &hs_fall_event);

	/* Next scanline */
	scanline = (scanline + 1) % VDG_FRAME_DURATION;
	scanline_flags = colourburst_enabled;
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

	/* Enable mode changes at beginning of active area */
	if (scanline == SCANLINE(VDG_ACTIVE_AREA_START)) {
		inhibit_mode_change = 0;
		vdg_set_mode();
	}
	/* Disable mode changes after end of active area */
	if (scanline == SCANLINE(VDG_ACTIVE_AREA_END)) {
		inhibit_mode_change = 1;
	}

}

static void do_hs_rise(void) {
	/* HS rising edge */
	PIA_SET_Cx1(PIA0.a);
}

/* Two versions of render_scanline(): first accounts for mid-scanline mode
 * changes and only fetches as many bytes from the SAM as required, second
 * does a whole scanline at a time. */

static void render_scanline(void) {
	uint8_t scanline_data[42];
	int beam_to = (current_cycle - scanline_start - SCAN_OFFSET) / 2;
	if (beam_to < 0)
		return;
	if (beam_to <= beam_pos)
		return;
	while (beam_pos < beam_to && beam_pos < 60) {
		*(pixel++) = border_colour;
		beam_pos++;
	}
	if (beam_pos >= 60 && beam_pos < beam_to && beam_pos < 316) {
		int draw_to = (beam_to > 308) ? 308 : beam_to;
		int nbytes = 1 + ((draw_to - beam_pos) >> (is_32byte ? 3 : 4));
		vdg_fetch_bytes(nbytes, scanline_data);
		render_scanline_data(scanline_data, nbytes);
		if (is_32byte) {
			beam_pos += nbytes * 8;
			if (beam_pos == 316)
				vdg_fetch_bytes(10, NULL);
		} else {
			beam_pos += nbytes * 16;
			if (beam_pos == 316)
				vdg_fetch_bytes(6, NULL);
		}
	}
	if (beam_pos >= 316) {
		while (beam_pos < beam_to && beam_pos < 372) {
			*(pixel++) = border_colour;
			beam_pos++;
		}
	}
}

void vdg_set_mode(void) {
	int mode;
	if (inhibit_mode_change)
		return;
	/* Render scanline so far before changing modes */
	if (frame == 0 && scanline >= VDG_ACTIVE_AREA_START && scanline < VDG_ACTIVE_AREA_END) {
		render_scanline();
	}
	mode = PIA1.b.out_source & PIA1.b.out_sink;

	colourburst_enabled = VDG_COLOURBURST;

	/* Update renderer */
	switch ((mode & 0xf0) >> 4) {
		case 0: case 2: case 4: case 6:
			render_scanline_data = render_sg4;
			if (mode & 0x08) {
				fg_colour = VDG_BRIGHT_ORANGE;
				bg_colour = VDG_DARK_ORANGE;
			} else {
				fg_colour = VDG_GREEN;
				bg_colour = VDG_DARK_GREEN;
			}
			border_colour = VDG_BLACK;
			is_32byte = 1;
			break;
		case 1: case 3: case 5: case 7:
			render_scanline_data = render_sg6;
			cg_colours = VDG_GREEN + ((mode & 0x08) >> 1);
			if (mode & 0x08) {
				fg_colour = VDG_BRIGHT_ORANGE;
				bg_colour = VDG_DARK_ORANGE;
			} else {
				fg_colour = VDG_GREEN;
				bg_colour = VDG_DARK_GREEN;
			}
			border_colour = VDG_BLACK;
			is_32byte = 1;
			break;
		case 8:
			render_scanline_data = render_cg1;
			cg_colours = VDG_GREEN + ((mode & 0x08) >> 1);
			border_colour = cg_colours;
			is_32byte = 0;
			break;
		case 9: case 11: case 13:
			render_scanline_data = render_rg1;
			fg_colour = VDG_GREEN + ((mode & 0x08) >> 1);
			bg_colour = VDG_BLACK;
			border_colour = fg_colour;
			is_32byte = 0;
			break;
		case 10: case 12: case 14:
			render_scanline_data = render_cg2;
			cg_colours = VDG_GREEN + ((mode & 0x08) >> 1);
			border_colour = cg_colours;
			is_32byte = 1;
			break;
		case 15: default:
			render_scanline_data = render_rg6;
			fg_colour = VDG_GREEN + ((mode & 0x08) >> 1);
			border_colour = fg_colour;
			bg_colour = (mode & 0x08) ? VDG_BLACK : VDG_DARK_GREEN;
			is_32byte = 1;
			if (mode & 0x08)
				colourburst_enabled = 0;
			break;
	}

}

/* Scanline rendering routines. */

/* Renders a line of alphanumeric/semigraphics 4 (mode is selected by data
 * line, so need to be handled together) */
static void render_sg4(uint8_t *vram_ptr, int nbytes) {
	int i, j;
	for (j = nbytes; j; j--) {
		uint8_t octet;
		octet = *(vram_ptr++);
		if (octet & 0x80) {
			uint8_t tmp = VDG_GREEN + ((octet & 0x70)>>4);
			if (subline < 6)
				octet >>= 2;
			*pixel = *(pixel+1) = *(pixel+2) = *(pixel+3)
			       = (octet & 0x02) ? tmp : VDG_BLACK;
			*(pixel+4) = *(pixel+5) = *(pixel+6) = *(pixel+7)
			           = (octet & 0x01) ? tmp : VDG_BLACK;
			pixel += 8;
		} else {
			uint8_t tmp = vdg_alpha[(octet&0x3f)*12 + subline];
			if (octet & 0x40)
				tmp = ~tmp;
			for (i = 8; i; tmp <<= 1, i--)
				*(pixel++) = (tmp&0x80) ? fg_colour : bg_colour;
		}
	}
}

/* Renders a line of external-alpha/semigraphics 6 (mode is selected by data
 * line, so need to be handled together) */
static void render_sg6(uint8_t *vram_ptr, int nbytes) {
	int i, j;
	for (j = nbytes; j; j--) {
		uint8_t octet;
		octet = *(vram_ptr++);
		if (octet & 0x80) {
			uint8_t tmp = cg_colours + ((octet & 0xc0)>>6);
			if (subline < 4)
				octet >>= 4;
			else if (subline < 8)
				octet >>= 2;
			*pixel = *(pixel+1) = *(pixel+2) = *(pixel+3)
			       = (octet & 0x02) ? tmp : VDG_BLACK;
			*(pixel+4) = *(pixel+5) = *(pixel+6) = *(pixel+7)
			           = (octet & 0x01) ? tmp : VDG_BLACK;
			pixel += 8;
		} else {
			uint8_t tmp = octet;
			if (octet & 0x40)
				tmp = ~tmp;
			for (i = 8; i; tmp <<= 1, i--)
				*(pixel++) = (tmp&0x80) ? fg_colour : bg_colour;
		}
	}
}

/* Render a 16-byte colour graphics line (CG1) */
static void render_cg1(uint8_t *vram_ptr, int nbytes) {
	int i, j;
	for (j = nbytes; j; j--) {
		uint8_t octet = *(vram_ptr++);
		for (i = 4; i; pixel += 4, octet >>= 2, i--)
			*pixel = *(pixel+1) = *(pixel+2) = *(pixel+3)
				= cg_colours + ((octet & 0xc0) >> 6);
	}
}

/* Render a 16-byte resolution graphics line (RG1,RG2,RG3) */
static void render_rg1(uint8_t *vram_ptr, int nbytes) {
	int i, j;
	for (j = nbytes; j; j--) {
		uint8_t octet = *(vram_ptr++);
		for (i = 8; i; pixel += 2, octet <<= 1, i--)
			*pixel = *(pixel+1)
			       = (octet & 0x80) ? fg_colour : bg_colour;
	}
}

/* Render a 32-byte colour graphics line (CG2,CG3,CG6) */
static void render_cg2(uint8_t *vram_ptr, int nbytes) {
	int i, j;
	for (j = nbytes; j; j--) {
		uint8_t octet = *(vram_ptr++);
		for (i = 4; i; pixel += 2, octet <<= 2, i--)
			*pixel = *(pixel+1)
			       = cg_colours + ((octet & 0xc0) >> 6);
	}
}

/* Render a 32-byte resolution graphics line (RG6) */
static void render_rg6(uint8_t *vram_ptr, int nbytes) {
	int i, j;
	for (j = nbytes; j; j--) {
		uint8_t octet = *(vram_ptr++);
		for (i = 8; i; octet <<= 1, i--)
			*(pixel++) = (octet&0x80) ? fg_colour : bg_colour;
	}
}
