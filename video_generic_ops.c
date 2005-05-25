/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2005  Ciaran Anscomb
 *
 *  See COPYING for redistribution conditions. */

/* This file contains generic scanline rendering routines.  It is included
 * into various video module source files and makes use of macros defined in
 * those files (eg, LOCK_SURFACE and XSTEP) */

/* Renders a line of alphanumeric/semigraphics 4 (mode is selected by data
 * line, so need to be handled together) */
static void render_sg4(void) {
	uint8_t *vram_ptr;
	unsigned int i, j, octet;
	Pixel tmp;
	LOCK_SURFACE;
#ifdef SEPARATE_BORDER
	LOCK_BORDER;
	*(bpixel++) = border_colour;
	*(bpixel++) = border_colour;
	UNLOCK_BORDER;
#else
	for (i = 32; i; i--) {
		*pixel = border_colour;
		*(pixel + 288 * XSTEP) = border_colour;
		pixel += XSTEP;
	}
#endif
	for (i = 0; i < 2; i++) {
		vram_ptr = (uint8_t *)sam_vram_ptr(sam_vdg_address);
		for (j = 16; j; j--) {
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
				pixel += 8*XSTEP;
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
				pixel += 8*XSTEP;
			}
		}
		sam_vdg_xstep(16);
	}
	UNLOCK_SURFACE;
	sam_vdg_hsync(32,10,16);
	pixel += NEXTLINE + (32 * XSTEP);
	subline++;
	if (subline > 11)
		subline = 0;
}

#define RENDER_BYTE_CG1(b,o,n) *(pixel+(o)*XSTEP) = *(pixel+(o+1)*XSTEP) \
		= *(pixel+(o+2)*XSTEP) = *(pixel+(o+3)*XSTEP) \
		= cg_colours[(b & (0xc0<<(n * 8))) >> ((n*8)+6)]; \
	*(pixel+(o+4)*XSTEP) = *(pixel+(o+5)*XSTEP) \
		= *(pixel+(o+6)*XSTEP) = *(pixel+(o+7)*XSTEP) \
		= cg_colours[(b & (0x30<<(n * 8))) >> ((n*8)+4)]; \
	*(pixel+(o+8)*XSTEP) = *(pixel+(o+9)*XSTEP) \
		= *(pixel+(o+10)*XSTEP) = *(pixel+(o+11)*XSTEP) \
		= cg_colours[(b & (0x0c<<(n * 8))) >> ((n*8)+2)]; \
	*(pixel+(o+12)*XSTEP) = *(pixel+(o+13)*XSTEP) \
		= *(pixel+(o+14)*XSTEP) = *(pixel+(o+15)*XSTEP) \
		= cg_colours[(b & (0x03<<(n * 8))) >> ((n*8))]

/* Render a 16-byte colour graphics line (CG1) */
static void render_cg1(void) {
	uint32_t *vram_ptr;
	unsigned int i;
	uint_least32_t octet;
	LOCK_SURFACE;
#ifdef SEPARATE_BORDER
	LOCK_BORDER;
	*(bpixel++) = border_colour;
	*(bpixel++) = border_colour;
	UNLOCK_BORDER;
#else
	for (i = 32; i; i--) {
		*pixel = border_colour;
		*(pixel + 288 * XSTEP) = border_colour;
		pixel += XSTEP;
	}
#endif
	vram_ptr = (uint32_t *)sam_vram_ptr(sam_vdg_address);
	for (i = 4; i; i--) {
		octet = *(vram_ptr++);
#ifdef WRONG_ENDIAN
		RENDER_BYTE_CG1(octet,0,0);
		RENDER_BYTE_CG1(octet,16,1);
		RENDER_BYTE_CG1(octet,32,2);
		RENDER_BYTE_CG1(octet,48,3);
#else
		RENDER_BYTE_CG1(octet,0,3);
		RENDER_BYTE_CG1(octet,16,2);
		RENDER_BYTE_CG1(octet,32,1);
		RENDER_BYTE_CG1(octet,48,0);
#endif
		pixel += 64*XSTEP;
	}
	UNLOCK_SURFACE;
	sam_vdg_xstep(16);
	sam_vdg_hsync(16,6,0);
	pixel += NEXTLINE + (32 * XSTEP);
	subline++;
	if (subline > 11)
		subline = 0;
}

#define RENDER_BYTE_RG1(b,o,n) *(pixel+(o)*XSTEP) = *(pixel+(o+1)*XSTEP) = (b & (0x80<<(n*8))) ? fg_colour : bg_colour; \
	*(pixel+(o+2)*XSTEP) = *(pixel+(o+3)*XSTEP) = (b & (0x40<<(n*8))) ? fg_colour : bg_colour; \
	*(pixel+(o+4)*XSTEP) = *(pixel+(o+5)*XSTEP) = (b & (0x20<<(n*8))) ? fg_colour : bg_colour; \
	*(pixel+(o+6)*XSTEP) = *(pixel+(o+7)*XSTEP) = (b & (0x10<<(n*8))) ? fg_colour : bg_colour; \
	*(pixel+(o+8)*XSTEP) = *(pixel+(o+9)*XSTEP) = (b & (0x08<<(n*8))) ? fg_colour : bg_colour; \
	*(pixel+(o+10)*XSTEP) = *(pixel+(o+11)*XSTEP) = (b & (0x04<<(n*8))) ? fg_colour : bg_colour; \
	*(pixel+(o+12)*XSTEP) = *(pixel+(o+13)*XSTEP) = (b & (0x02<<(n*8))) ? fg_colour : bg_colour; \
	*(pixel+(o+14)*XSTEP) = *(pixel+(o+15)*XSTEP) = (b & (0x01<<(n*8))) ? fg_colour : bg_colour

/* Render a 16-byte resolution graphics line (RG1,RG2,RG3) */
static void render_rg1(void) {
	uint32_t *vram_ptr;
	unsigned int i;
	uint_least32_t octet;
	LOCK_SURFACE;
#ifdef SEPARATE_BORDER
	LOCK_BORDER;
	*(bpixel++) = border_colour;
	*(bpixel++) = border_colour;
	UNLOCK_BORDER;
#else
	for (i = 32; i; i--) {
		*pixel = border_colour;
		*(pixel + 288 * XSTEP) = border_colour;
		pixel += XSTEP;
	}
#endif
	vram_ptr = (uint32_t *)sam_vram_ptr(sam_vdg_address);
	for (i = 4; i; i--) {
		octet = *(vram_ptr++);
#ifdef WRONG_ENDIAN
			RENDER_BYTE_RG1(octet,0,0);
			RENDER_BYTE_RG1(octet,16,1);
			RENDER_BYTE_RG1(octet,32,2);
			RENDER_BYTE_RG1(octet,48,3);
#else
			RENDER_BYTE_RG1(octet,0,3);
			RENDER_BYTE_RG1(octet,16,2);
			RENDER_BYTE_RG1(octet,32,1);
			RENDER_BYTE_RG1(octet,48,0);
#endif
		pixel += 64*XSTEP;
	}
	UNLOCK_SURFACE;
	sam_vdg_xstep(16);
	sam_vdg_hsync(16,6,0);
	pixel += NEXTLINE + (32 * XSTEP);
	subline++;
	if (subline > 11)
		subline = 0;
}

#define RENDER_BYTE_CG2(b,o,n) *(pixel+(o)*XSTEP) = *(pixel+(o+1)*XSTEP) = cg_colours[(b & (0xc0<<(n * 8))) >> ((n*8)+6)]; \
	*(pixel+(o+2)*XSTEP) = *(pixel+(o+3)*XSTEP) = cg_colours[(b & (0x30<<(n * 8))) >> ((n*8)+4)]; \
	*(pixel+(o+4)*XSTEP) = *(pixel+(o+5)*XSTEP) = cg_colours[(b & (0x0c<<(n * 8))) >> ((n*8)+2)]; \
	*(pixel+(o+6)*XSTEP) = *(pixel+(o+7)*XSTEP) = cg_colours[(b & (0x03<<(n * 8))) >> ((n*8))]

/* Render a 32-byte colour graphics line (CG2,CG3,CG6) */
static void render_cg2(void) {
	uint32_t *vram_ptr;
	unsigned int i, j;
	uint_least32_t octet;
	LOCK_SURFACE;
#ifdef SEPARATE_BORDER
	LOCK_BORDER;
	*(bpixel++) = border_colour;
	*(bpixel++) = border_colour;
	UNLOCK_BORDER;
#else
	for (i = 32; i; i--) {
		*pixel = border_colour;
		*(pixel + 288 * XSTEP) = border_colour;
		pixel += XSTEP;
	}
#endif
	for (i = 0; i < 2; i++) {
		vram_ptr = (uint32_t *)sam_vram_ptr(sam_vdg_address);
		for (j = 4; j; j--) {
			octet = *(vram_ptr++);
#ifdef WRONG_ENDIAN
			RENDER_BYTE_CG2(octet,0,0);
			RENDER_BYTE_CG2(octet,8,1);
			RENDER_BYTE_CG2(octet,16,2);
			RENDER_BYTE_CG2(octet,24,3);
#else
			RENDER_BYTE_CG2(octet,0,3);
			RENDER_BYTE_CG2(octet,8,2);
			RENDER_BYTE_CG2(octet,16,1);
			RENDER_BYTE_CG2(octet,24,0);
#endif
			pixel += 32*XSTEP;
		}
		sam_vdg_xstep(16);
	}
	UNLOCK_SURFACE;
	sam_vdg_hsync(32,10,16);
	pixel += NEXTLINE + (32 * XSTEP);
	subline++;
	if (subline > 11)
		subline = 0;
}

#define RENDER_BYTE_RG6(b,o,n) *(pixel+(o)*XSTEP) = (b & (0x80<<(n*8))) ? fg_colour : bg_colour; \
	*(pixel+(o+1)*XSTEP) = (b & (0x40<<(n*8))) ? fg_colour : bg_colour; \
	*(pixel+(o+2)*XSTEP) = (b & (0x20<<(n*8))) ? fg_colour : bg_colour; \
	*(pixel+(o+3)*XSTEP) = (b & (0x10<<(n*8))) ? fg_colour : bg_colour; \
	*(pixel+(o+4)*XSTEP) = (b & (0x08<<(n*8))) ? fg_colour : bg_colour; \
	*(pixel+(o+5)*XSTEP) = (b & (0x04<<(n*8))) ? fg_colour : bg_colour; \
	*(pixel+(o+6)*XSTEP) = (b & (0x02<<(n*8))) ? fg_colour : bg_colour; \
	*(pixel+(o+7)*XSTEP) = (b & (0x01<<(n*8))) ? fg_colour : bg_colour

/* Render a 32-byte resolution graphics line (RG6) */
static void render_rg6(void) {
	uint32_t *vram_ptr;
	unsigned int i, j;
	uint_least32_t octet;
	LOCK_SURFACE;
#ifdef SEPARATE_BORDER
	LOCK_BORDER;
	*(bpixel++) = border_colour;
	*(bpixel++) = border_colour;
	UNLOCK_BORDER;
#else
	for (i = 32; i; i--) {
		*pixel = border_colour;
		*(pixel + 288 * XSTEP) = border_colour;
		pixel += XSTEP;
	}
#endif
	for (i = 0; i < 2; i++) {
		vram_ptr = (uint32_t *)sam_vram_ptr(sam_vdg_address);
		for (j = 4; j; j--) {
			octet = *(vram_ptr++);
#ifdef WRONG_ENDIAN
			RENDER_BYTE_RG6(octet,0,0);
			RENDER_BYTE_RG6(octet,8,1);
			RENDER_BYTE_RG6(octet,16,2);
			RENDER_BYTE_RG6(octet,24,3);
#else
			RENDER_BYTE_RG6(octet,0,3);
			RENDER_BYTE_RG6(octet,8,2);
			RENDER_BYTE_RG6(octet,16,1);
			RENDER_BYTE_RG6(octet,24,0);
#endif
			pixel += 32*XSTEP;
		}
		sam_vdg_xstep(16);
	}
	UNLOCK_SURFACE;
	sam_vdg_hsync(32,10,16);
	pixel += NEXTLINE + (32 * XSTEP);
	subline++;
	if (subline > 11)
		subline = 0;
}

/* Render a line of border (top/bottom) */
static void render_border(void) {
	unsigned int i;
#ifdef SEPARATE_BORDER
	LOCK_BORDER;
	*(bpixel++) = border_colour;
	*(bpixel++) = border_colour;
	UNLOCK_BORDER;
#endif
	LOCK_SURFACE;
	for (i = 320; i; i--) {
		*pixel = border_colour;
		pixel += XSTEP;
	}
	UNLOCK_SURFACE;
	pixel += NEXTLINE;
}
