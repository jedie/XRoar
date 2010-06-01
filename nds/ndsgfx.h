#ifndef XROAR_NDS_NDSGFX_H_
#define XROAR_NDS_NDSGFX_H_

#include <nds.h>

#include "../types.h"

#define NDS_RGBA(c) ((((c)&0x80)<<8)|RGB15(((c)>>27)&31,((c)>>19)&31,((c)>>11)&31))
#define NDS_RGB(c) (BIT(15)|RGB15(((c)>>19)&31,((c)>>11)&31,((c)>>3)&31))

#define NDS_BLACK      NDS_RGB(0x000000)
#define NDS_GREY20     NDS_RGB(0x333333)
#define NDS_GREY40     NDS_RGB(0x666666)
#define NDS_GREY60     NDS_RGB(0x999999)
#define NDS_GREY80     NDS_RGB(0xcccccc)
#define NDS_WHITE      NDS_RGB(0xffffff)
#define NDS_DARKPURPLE NDS_RGB(0x301830)

typedef struct {
	int w;
	int h;
	void *data;
} Sprite;

int ndsgfx_init(void);
void ndsgfx_fillrect(int x, int y, int w, int h, unsigned int colour);
void ndsgfx_blit(int x, int y, Sprite *src);
void nds_set_text_colour(unsigned int fg, unsigned int bg);
void nds_put_char(int x, int y, int c);
void nds_print_string(int x, int y, const char *str);

#endif  /* XROAR_NDS_NDSGFX_H_ */
