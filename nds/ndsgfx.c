#include <string.h>
#include <nds.h>

#include "../types.h"
#include "ndsgfx.h"

extern unsigned char nds_font8x8[768];

static unsigned int fg_colour, bg_colour;

int ndsgfx_init(void) {
	videoSetModeSub(MODE_5_2D | DISPLAY_BG2_ACTIVE);
	vramSetBankC(VRAM_C_SUB_BG);
	REG_BG2CNT_SUB = BG_BMP16_256x256;
	REG_BG2PA_SUB = 1 << 8;
	REG_BG2PB_SUB = 0;
	REG_BG2PC_SUB = 0;
	REG_BG2PD_SUB = 1 << 8;
	REG_BG2X_SUB = 0;
	REG_BG2Y_SUB = 0;
	nds_set_text_colour(NDS_WHITE, NDS_BLACK);
	return 0;
}

void ndsgfx_fillrect(int x, int y, int w, int h, unsigned int colour) {
	uint16_t *d = (uint16_t *)BG_GFX_SUB + (y*256) + x;
	int skip = 256 - w;
	int i;
	for (; h; h--) {
		for (i = w; i; i--) {
			*(d++) = colour;
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

void nds_set_text_colour(unsigned int fg, unsigned int bg) {
	fg_colour = fg;
	bg_colour = bg;
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
