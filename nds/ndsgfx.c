#include <string.h>
#include <nds.h>

#include "../types.h"
#include "ndsgfx.h"

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
