/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2007  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

/* This file contains generic scanline rendering routines.  It is included
 * into various video module source files and makes use of macros defined in
 * those files (eg, LOCK_SURFACE and XSTEP) */

/* VDG_tFP here is a kludge - I don't know why it's needed, but without it,
 * DragonFire doesn't render correctly.  Everything *should* be relative to
 * the horizontal sync pulse which occurs *after* the front porch. */
#define SCAN_OFFSET (VDG_LEFT_BORDER_START - VDG_LEFT_BORDER_UNSEEN + VDG_tFP)

#ifdef NO_BORDER
#define RENDER_LEFT_BORDER do { \
		while (beam_pos < 32 && beam_pos < beam_to) { \
			beam_pos += 8; \
		} \
	} while (0)
#define RENDER_RIGHT_BORDER do { \
		while (beam_pos >= 288 && beam_pos < 320 && beam_pos < beam_to) { \
			beam_pos += 8; \
		} \
	} while (0)
#else  /* NO_BORDER */
#define RENDER_LEFT_BORDER do { \
		while (beam_pos < 32 && beam_pos < beam_to) { \
			*(pixel) = *(pixel+1*XSTEP) = *(pixel+2*XSTEP) \
				= *(pixel+3*XSTEP) = *(pixel+4*XSTEP) \
				= *(pixel+5*XSTEP) = *(pixel+6*XSTEP) \
				= *(pixel+7*XSTEP) = border_colour; \
			pixel += 8*XSTEP; \
			beam_pos += 8; \
		} \
	} while (0)

#define RENDER_RIGHT_BORDER do { \
		while (beam_pos >= 288 && beam_pos < 320 && beam_pos < beam_to) { \
			*(pixel) = *(pixel+1*XSTEP) = *(pixel+2*XSTEP) \
				= *(pixel+3*XSTEP) = *(pixel+4*XSTEP) \
				= *(pixel+5*XSTEP) = *(pixel+6*XSTEP) \
				= *(pixel+7*XSTEP) = border_colour; \
			pixel += 8*XSTEP; \
			beam_pos += 8; \
		} \
	} while (0)
#endif  /* NO_BORDER */

#define ACTIVE_DISPLAY_AREA (beam_pos >= 32 && beam_pos < 288 && beam_pos < beam_to)

static unsigned int subline;
static Pixel *pixel;
static Pixel darkgreen, black;
static Pixel bg_colour;
static Pixel fg_colour;
static Pixel vdg_colour[16];
static Pixel *cg_colours;
static Pixel border_colour;
static uint8_t *vram_ptr;

/* Allocate colours */
static void alloc_colours(void) {
	vdg_colour[0] = MAPCOLOUR(0x00, 0xff, 0x00);
	vdg_colour[1] = MAPCOLOUR(0xff, 0xff, 0x00);
	vdg_colour[2] = MAPCOLOUR(0x00, 0x00, 0xff);
	vdg_colour[3] = MAPCOLOUR(0xff, 0x00, 0x00);
	vdg_colour[4] = MAPCOLOUR(0xff, 0xe0, 0xe0);
	vdg_colour[5] = MAPCOLOUR(0x00, 0xff, 0xff);
	vdg_colour[6] = MAPCOLOUR(0xff, 0x00, 0xff);
	vdg_colour[7] = MAPCOLOUR(0xff, 0xa5, 0x00);
	vdg_colour[8] = MAPCOLOUR(0x00, 0x00, 0x00);
	vdg_colour[9] = MAPCOLOUR(0x00, 0x80, 0xff);
	vdg_colour[10] = MAPCOLOUR(0xff, 0x80, 0x00);
	vdg_colour[11] = MAPCOLOUR(0xff, 0xff, 0xff);
	vdg_colour[12] = MAPCOLOUR(0x00, 0x00, 0x00);
	vdg_colour[13] = MAPCOLOUR(0xff, 0x80, 0x00);
	vdg_colour[14] = MAPCOLOUR(0x00, 0x80, 0xff);
	vdg_colour[15] = MAPCOLOUR(0xff, 0xff, 0xff);
	black = MAPCOLOUR(0x00, 0x00, 0x00);
	darkgreen = MAPCOLOUR(0x00, 0x20, 0x00);
}

/* Update graphics mode - change current select colour set */
static void set_mode(unsigned int mode) {
	if (mode & 0x80) {
		/* Graphics modes */
		if (((mode & 0x70) == 0x70) && video_artifact_mode) {
			cg_colours = &vdg_colour[4 + video_artifact_mode * 4];
			fg_colour = vdg_colour[(mode & 0x08) >> 1];
		} else {
			cg_colours = &vdg_colour[(mode & 0x08) >> 1];
			fg_colour = cg_colours[0];
		}
		bg_colour = black;
		border_colour = fg_colour;
	} else {
		bg_colour = darkgreen;
		border_colour = black;
		if (mode & 0x08)
			fg_colour = vdg_colour[7];
		else
			fg_colour = vdg_colour[0];
		cg_colours = &vdg_colour[(mode & 0x08) >> 1];
	}
}

/* Renders a line of alphanumeric/semigraphics 4 (mode is selected by data
 * line, so need to be handled together) */
static void render_sg4(void) {
	unsigned int octet;
	int beam_to = (current_cycle - scanline_start - SCAN_OFFSET) / 2;
	if (beam_to < 0)
		return;
	LOCK_SURFACE;
	RENDER_LEFT_BORDER;
	while (ACTIVE_DISPLAY_AREA) {
		Pixel tmp;
		if (beam_pos == 32 || beam_pos == 160)
			vram_ptr = (uint8_t *)sam_vram_ptr(sam_vdg_address);
		octet = *(vram_ptr++);
		if (octet & 0x80) {
			tmp = vdg_colour[(octet & 0x70)>>4];
			if (subline < 6) {
				*pixel = *(pixel+1*XSTEP) = *(pixel+2*XSTEP) = *(pixel+3*XSTEP) = (octet & 0x08) ? tmp : black;
				*(pixel+4*XSTEP) = *(pixel+5*XSTEP) = *(pixel+6*XSTEP) = *(pixel+7*XSTEP) = (octet & 0x04) ? tmp : black;
			} else {
				*pixel = *(pixel+1*XSTEP) = *(pixel+2*XSTEP) = *(pixel+3*XSTEP) = (octet & 0x02) ? tmp : black;
				*(pixel+4*XSTEP) = *(pixel+5*XSTEP) = *(pixel+6*XSTEP) = *(pixel+7*XSTEP) = (octet & 0x01) ? tmp : black;
			}
		} else {
			tmp = vdg_alpha[(octet&0x3f)*12 + subline];
			if (octet & 0x40)
				tmp = ~tmp;
			*pixel = (tmp & 0x80) ? fg_colour : bg_colour;
			*(pixel+1*XSTEP) = (tmp & 0x40) ? fg_colour : bg_colour;
			*(pixel+2*XSTEP) = (tmp & 0x20) ? fg_colour : bg_colour;
			*(pixel+3*XSTEP) = (tmp & 0x10) ? fg_colour : bg_colour;
			*(pixel+4*XSTEP) = (tmp & 0x08) ? fg_colour : bg_colour;
			*(pixel+5*XSTEP) = (tmp & 0x04) ? fg_colour : bg_colour;
			*(pixel+6*XSTEP) = (tmp & 0x02) ? fg_colour : bg_colour;
			*(pixel+7*XSTEP) = (tmp & 0x01) ? fg_colour : bg_colour;
		}
		pixel += 8*XSTEP;
		beam_pos += 8;
		if (beam_pos == 160 || beam_pos == 288)
			sam_vdg_xstep(16);
	}
	RENDER_RIGHT_BORDER;
	UNLOCK_SURFACE;
	if (beam_pos == 320) {
		sam_vdg_hsync(10);
		pixel += NEXTLINE;
		subline++;
		if (subline > 11)
			subline = 0;
		beam_pos++;
	}
}

/* Renders a line of external-alpha/semigraphics 6 (mode is selected by data
 * line, so need to be handled together) */
static void render_sg6(void) {
	unsigned int octet;
	int beam_to = (current_cycle - scanline_start - SCAN_OFFSET) / 2;
	if (beam_to < 0)
		return;
	LOCK_SURFACE;
	RENDER_LEFT_BORDER;
	while (ACTIVE_DISPLAY_AREA) {
		Pixel tmp;
		if (beam_pos == 32 || beam_pos == 160)
			vram_ptr = (uint8_t *)sam_vram_ptr(sam_vdg_address);
		octet = *(vram_ptr++);
		if (octet & 0x80) {
			tmp = cg_colours[(octet & 0xc0)>>6];
			if (subline < 4) {
				*pixel = *(pixel+1*XSTEP) = *(pixel+2*XSTEP) = *(pixel+3*XSTEP) = (octet & 0x20) ? tmp : black;
				*(pixel+4*XSTEP) = *(pixel+5*XSTEP) = *(pixel+6*XSTEP) = *(pixel+7*XSTEP) = (octet & 0x10) ? tmp : black;
			} else if (subline < 8) {
				*pixel = *(pixel+1*XSTEP) = *(pixel+2*XSTEP) = *(pixel+3*XSTEP) = (octet & 0x08) ? tmp : black;
				*(pixel+4*XSTEP) = *(pixel+5*XSTEP) = *(pixel+6*XSTEP) = *(pixel+7*XSTEP) = (octet & 0x04) ? tmp : black;
			} else {
				*pixel = *(pixel+1*XSTEP) = *(pixel+2*XSTEP) = *(pixel+3*XSTEP) = (octet & 0x02) ? tmp : black;
				*(pixel+4*XSTEP) = *(pixel+5*XSTEP) = *(pixel+6*XSTEP) = *(pixel+7*XSTEP) = (octet & 0x01) ? tmp : black;
			}
		} else {
			tmp = octet;
			if (octet & 0x40)
				tmp = ~tmp;
			*pixel = (tmp & 0x80) ? fg_colour : bg_colour;
			//*(pixel+1*XSTEP) = (tmp & 0x40) ? fg_colour : bg_colour;
			*(pixel+1*XSTEP) = bg_colour;
			*(pixel+2*XSTEP) = (tmp & 0x20) ? fg_colour : bg_colour;
			*(pixel+3*XSTEP) = (tmp & 0x10) ? fg_colour : bg_colour;
			*(pixel+4*XSTEP) = (tmp & 0x08) ? fg_colour : bg_colour;
			*(pixel+5*XSTEP) = (tmp & 0x04) ? fg_colour : bg_colour;
			*(pixel+6*XSTEP) = (tmp & 0x02) ? fg_colour : bg_colour;
			*(pixel+7*XSTEP) = (tmp & 0x01) ? fg_colour : bg_colour;
		}
		pixel += 8*XSTEP;
		beam_pos += 8;
		if (beam_pos == 160 || beam_pos == 288)
			sam_vdg_xstep(16);
	}
	RENDER_RIGHT_BORDER;
	UNLOCK_SURFACE;
	if (beam_pos == 320) {
		sam_vdg_hsync(10);
		pixel += NEXTLINE;
		subline++;
		if (subline > 11)
			subline = 0;
		beam_pos++;
	}
}

#define RENDER_BYTE_CG1(b) do { \
		*pixel = *(pixel+1*XSTEP) \
			= *(pixel+2*XSTEP) = *(pixel+3*XSTEP) \
			= cg_colours[(b & 0xc0) >> 6]; \
		*(pixel+4*XSTEP) = *(pixel+5*XSTEP) \
			= *(pixel+6*XSTEP) = *(pixel+7*XSTEP) \
			= cg_colours[(b & 0x30) >> 4]; \
		*(pixel+8*XSTEP) = *(pixel+9*XSTEP) \
			= *(pixel+10*XSTEP) = *(pixel+11*XSTEP) \
			= cg_colours[(b & 0x0c) >> 2]; \
		*(pixel+12*XSTEP) = *(pixel+13*XSTEP) \
			= *(pixel+14*XSTEP) = *(pixel+15*XSTEP) \
			= cg_colours[b & 0x03]; \
		pixel += 16 * XSTEP; \
		beam_pos += 16; \
	} while (0);

/* Render a 16-byte colour graphics line (CG1) */
static void render_cg1(void) {
	unsigned int octet;
	int beam_to = (current_cycle - scanline_start - SCAN_OFFSET) / 2;
	if (beam_to < 0)
		return;
	LOCK_SURFACE;
	RENDER_LEFT_BORDER;
	while (ACTIVE_DISPLAY_AREA) {
		if (beam_pos == 32)
			vram_ptr = (uint8_t *)sam_vram_ptr(sam_vdg_address);
		octet = *(vram_ptr++);
		RENDER_BYTE_CG1(octet);
		if (beam_pos == 288)
			sam_vdg_xstep(16);
	}
	RENDER_RIGHT_BORDER;
	UNLOCK_SURFACE;
	if (beam_pos == 320) {
		sam_vdg_hsync(6);
		pixel += NEXTLINE;
		subline++;
		if (subline > 11)
			subline = 0;
		beam_pos++;
	}
}

#define RENDER_BYTE_RG1(b) do { \
		*pixel = *(pixel+1*XSTEP) = (b & 0x80) ? fg_colour : bg_colour; \
		*(pixel+2*XSTEP) = *(pixel+3*XSTEP) = (b & 0x40) ? fg_colour : bg_colour; \
		*(pixel+4*XSTEP) = *(pixel+5*XSTEP) = (b & 0x20) ? fg_colour : bg_colour; \
		*(pixel+6*XSTEP) = *(pixel+7*XSTEP) = (b & 0x10) ? fg_colour : bg_colour; \
		*(pixel+8*XSTEP) = *(pixel+9*XSTEP) = (b & 0x08) ? fg_colour : bg_colour; \
		*(pixel+10*XSTEP) = *(pixel+11*XSTEP) = (b & 0x04) ? fg_colour : bg_colour; \
		*(pixel+12*XSTEP) = *(pixel+13*XSTEP) = (b & 0x02) ? fg_colour : bg_colour; \
		*(pixel+14*XSTEP) = *(pixel+15*XSTEP) = (b & 0x01) ? fg_colour : bg_colour; \
		pixel += 16 * XSTEP; \
		beam_pos += 16; \
	} while (0)

/* Render a 16-byte resolution graphics line (RG1,RG2,RG3) */
static void render_rg1(void) {
	unsigned int octet;
	int beam_to = (current_cycle - scanline_start - SCAN_OFFSET) / 2;
	if (beam_to < 0)
		return;
	LOCK_SURFACE;
	RENDER_LEFT_BORDER;
	while (ACTIVE_DISPLAY_AREA) {
		if (beam_pos == 32)
			vram_ptr = (uint8_t *)sam_vram_ptr(sam_vdg_address);
		octet = *(vram_ptr++);
		RENDER_BYTE_RG1(octet);
		if (beam_pos == 288)
			sam_vdg_xstep(16);
	}
	RENDER_RIGHT_BORDER;
	UNLOCK_SURFACE;
	if (beam_pos == 320) {
		sam_vdg_hsync(6);
		pixel += NEXTLINE;
		subline++;
		if (subline > 11)
			subline = 0;
		beam_pos++;
	}
}

#define RENDER_BYTE_CG2(o) do { \
		*pixel = *(pixel+1*XSTEP) = cg_colours[(o & 0xc0) >> 6]; \
		*(pixel+2*XSTEP) = *(pixel+3*XSTEP) = cg_colours[(o & 0x30) >> 4]; \
		*(pixel+4*XSTEP) = *(pixel+5*XSTEP) = cg_colours[(o & 0x0c) >> 2]; \
		*(pixel+6*XSTEP) = *(pixel+7*XSTEP) = cg_colours[o & 0x03]; \
		pixel += 8*XSTEP; \
		beam_pos += 8; \
	} while (0)

/* Render a 32-byte colour graphics line (CG2,CG3,CG6) */
static void render_cg2(void) {
	int beam_to = (current_cycle - scanline_start - SCAN_OFFSET) / 2;
	if (beam_to < 0)
		return;
	LOCK_SURFACE;
	RENDER_LEFT_BORDER;
	while (ACTIVE_DISPLAY_AREA) {
		unsigned int octet;
		if (beam_pos == 32 || beam_pos == 160)
			vram_ptr = (uint8_t *)sam_vram_ptr(sam_vdg_address);
		octet = *(vram_ptr++);
		RENDER_BYTE_CG2(octet);
		if (beam_pos == 160 || beam_pos == 288)
			sam_vdg_xstep(16);
	}
	RENDER_RIGHT_BORDER;
	UNLOCK_SURFACE;
	if (beam_pos == 320) {
		sam_vdg_hsync(10);
		pixel += NEXTLINE;
		subline++;
		if (subline > 11)
			subline = 0;
		beam_pos++;
	}
}

#define RENDER_BYTE_RG6(o) do { \
		*pixel = (o & 0x80) ? fg_colour : bg_colour; \
		*(pixel+1*XSTEP) = (o & 0x40) ? fg_colour : bg_colour; \
		*(pixel+2*XSTEP) = (o & 0x20) ? fg_colour : bg_colour; \
		*(pixel+3*XSTEP) = (o & 0x10) ? fg_colour : bg_colour; \
		*(pixel+4*XSTEP) = (o & 0x08) ? fg_colour : bg_colour; \
		*(pixel+5*XSTEP) = (o & 0x04) ? fg_colour : bg_colour; \
		*(pixel+6*XSTEP) = (o & 0x02) ? fg_colour : bg_colour; \
		*(pixel+7*XSTEP) = (o & 0x01) ? fg_colour : bg_colour; \
	} while (0)

/* Render a 32-byte resolution graphics line (RG6) */
static void render_rg6(void) {
	unsigned int octet;
	int beam_to = (current_cycle - scanline_start - SCAN_OFFSET) / 2;
	if (beam_to < 0)
		return;
	LOCK_SURFACE;
	RENDER_LEFT_BORDER;
	while (ACTIVE_DISPLAY_AREA) {
		if (beam_pos == 32 || beam_pos == 160)
			vram_ptr = (uint8_t *)sam_vram_ptr(sam_vdg_address);
		octet = *(vram_ptr++);
		RENDER_BYTE_RG6(octet);
		pixel += 8*XSTEP;
		beam_pos += 8;
		if (beam_pos == 160 || beam_pos == 288)
			sam_vdg_xstep(16);
	}
	RENDER_RIGHT_BORDER;
	UNLOCK_SURFACE;
	if (beam_pos == 320) {
		sam_vdg_hsync(10);
		pixel += NEXTLINE;
		subline++;
		if (subline > 11)
			subline = 0;
		beam_pos++;
	}
}

/* Render a line of border (top/bottom) */
static void render_border(void) {
#ifndef NO_BORDER
	unsigned int i;
	LOCK_SURFACE;
	for (i = 320; i; i--) {
		*pixel = border_colour;
		pixel += XSTEP;
	}
	UNLOCK_SURFACE;
	pixel += NEXTLINE;
#endif
}
