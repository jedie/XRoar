#include <string.h>
#include <nds.h>

#include "../types.h"
#include "ndsgfx.h"

extern unsigned char nds_font8x8[768];

static uint16_t fg_colour, bg_colour;
static uint16_t screen_backup[320*240];

int ndsgfx_init(void) {
	videoSetModeSub(MODE_5_2D | DISPLAY_BG2_ACTIVE);
	vramSetBankC(VRAM_C_SUB_BG);
	SUB_BG2_CR = BG_BMP16_256x256;
	SUB_BG2_XDX = 1 << 8;
	SUB_BG2_XDY = 0;
	SUB_BG2_YDX = 0;
	SUB_BG2_YDY = 1 << 8;
	SUB_BG2_CX = 0;
	SUB_BG2_CY = 0;
	nds_set_text_colour(0xffffffff, 0x000000ff);
	return 0;
}

void ndsgfx_fillrect(int x, int y, int w, int h, uint32_t colour) {
	uint16_t *d = (uint16_t *)BG_GFX_SUB + (y*256) + x;
	uint16_t c = NDS_MAPCOLOUR(colour >> 24, (colour >> 16) & 0xff,
			(colour >> 8) & 0xff);
	int skip = 256 - w;
	int i;
	for (; h; h--) {
		for (i = w; i; i--) {
			*(d++) = c;
		}
		d += skip;
	}
}

void ndsgfx_blit(int x, int y, Sprite *src) {
	uint16_t *s = src->data;
	uint16_t *d = (uint16_t *)BG_GFX_SUB + (y*256) + x;
	int skip = 256 - src->w;
	int i, h;
	for (h = src->h; h; h--) {
		for (i = src->w; i; i--) {
			*(d++) = *(s++);
		}
		d += skip;
	}
}

void ndsgfx_backup(void) {
	memcpy(screen_backup, BG_GFX_SUB, 320*240*sizeof(uint8_t));
}

void ndsgfx_restore(void) {
	memcpy(BG_GFX_SUB, screen_backup, 320*240*sizeof(uint8_t));
}

void nds_set_text_colour(uint32_t fg, uint32_t bg) {
	fg_colour = NDS_MAPCOLOUR(fg >> 24, (fg >> 16) & 0xff,
			(fg >> 8) & 0xff);
	bg_colour = NDS_MAPCOLOUR(bg >> 24, (bg >> 16) & 0xff,
			(bg >> 8) & 0xff);
}

/* Assumes 6x8 font in 8x8 representation (2 left bits clear).
 * Renders a blank 9th line for spacing. */
void nds_put_char(int x, int y, int c) {
	uint16_t *d = (uint16_t *)BG_GFX_SUB + (y*256) + x;
	int i, j;
	uint8_t *bitmap;
	if (c < 32 || c >= 128) c = 127;
	c -= 32;
	bitmap = &nds_font8x8[c*8];
	for (j = 0; j < 8; j++) {
		int b = *(bitmap++);
		for (i = 0; i < 6; i++) {
			*(d++) = (b & 0x20) ? fg_colour : bg_colour;
			b <<= 1;
		}
		d += 250;
	}
	for (i = 0; i < 6; i++) {
		*(d++) = bg_colour;
	}
}

void nds_print_string(int x, int y, const char *str) {
	const char *c;
	int i;
	if (str == NULL) return;
	c = str;
	i = x;
	while (*c) {
		if (*c == '\n') {
			i = x;
			y += 9;
		} else {
			nds_put_char(i, y, *c);
			i += 6;
		}
		c++;
	}
}
