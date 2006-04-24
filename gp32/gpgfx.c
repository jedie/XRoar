#include "../types.h"
#include "gpgfx.h"

GPDRAWSURFACE gp_screen;
static uint8_t screen_backup[320*240];

int gpgfx_init(void) {
	GpGraphicModeSet(GPC_DFLAG_8BPP, NULL);
	GpLcdSurfaceGet(&gp_screen, 0);
	GpSurfaceSet(&gp_screen);
	GpLcdEnable();
	swi_mmu_change((unsigned char *)((uint32_t)gp_screen.ptbuffer & ~4095), (unsigned char *)((uint32_t)(gp_screen.ptbuffer + (320 * 240) + 4095) & ~4095), MMU_NCNB);
	return 0;
}

void gpgfx_fillrect(uint_least16_t x, uint_least16_t y, uint_least16_t w, uint_least16_t h, uint32_t colour) {
	uint8_t *d = (uint8_t *)gp_screen.ptbuffer + (x*240) + (240-y) - h;
	uint8_t c = GPGFX_MAPCOLOUR(colour >> 24, (colour >> 16) & 0xff,
			(colour >> 8) & 0xff);
	uint_least16_t skip = 240 - h;
	uint_least16_t j;
	for (; w; w--) {
		for (j = h; j; j--) {
			*(d++) = c;
		}
		d += skip;
	}
}

void gpgfx_blit(uint_least16_t x, uint_least16_t y, Sprite *src) {
	uint8_t *s = src->data;
	uint8_t *d = (uint8_t *)gp_screen.ptbuffer + (x*240) + (240-y) - src->h;
	uint_least16_t skip = 240 - src->h;
	uint_least16_t j, w = src->w;
	for (; w; w--) {
		for (j = src->h; j; j--) {
			*(d++) = *(s++);
		}
		d += skip;
	}
}

void gpgfx_backup(void) {
	memcpy(screen_backup, gp_screen.ptbuffer, 320*240*sizeof(uint8_t));
}

void gpgfx_restore(void) {
	memcpy(gp_screen.ptbuffer, screen_backup, 320*240*sizeof(uint8_t));
}
