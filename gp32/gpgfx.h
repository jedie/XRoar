#ifndef XROAR_GP32_GPGFX_H_
#define XROAR_GP32_GPGFX_H_

#include <gpgraphic.h>

#include "../types.h"

#define GPGFX_MAPCOLOUR(r,g,b) ((r & 0xc0) | (g & 0xe0) >> 2 | (b & 0xe0) >> 5)

typedef struct {
	uint_least16_t w;
	uint_least16_t h;
	void *data;
} Sprite;

extern GPDRAWSURFACE gp_screen;

int gpgfx_init(void);
void gpgfx_fillrect(uint_least16_t x, uint_least16_t y,
		uint_least16_t w, uint_least16_t h, uint32_t colour);
void gpgfx_blit(uint_least16_t x, uint_least16_t y, Sprite *src);
void gpgfx_backup(void);
void gpgfx_restore(void);

#endif  /* XROAR_GP32_GPGFX_H_ */
