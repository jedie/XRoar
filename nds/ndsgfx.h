#ifndef __NDSGFX_H__
#define __NDSGFX_H__

#include <nds.h>

#include "../types.h"

#define NDS_MAPCOLOUR(r,g,b) (BIT(15)|RGB15((r)>>3,(g)>>3,(b)>>3))

typedef struct {
	int w;
	int h;
	void *data;
} Sprite;

int ndsgfx_init(void);
void ndsgfx_fillrect(int x, int y, int w, int h, uint32_t colour);
void ndsgfx_blit(int x, int y, Sprite *src);
void ndsgfx_backup(void);
void ndsgfx_restore(void);

#endif  /* __NDSGFX_H__ */
