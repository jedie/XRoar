/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2005  Ciaran Anscomb
 *
 *  See COPYING for redistribution conditions. */

#ifndef __VIDEO_H__
#define __VIDEO_H__

#include "types.h"

typedef struct {
	uint_least16_t w;
	uint_least16_t h;
	void *data;
} Sprite;

typedef struct VideoModule VideoModule;
struct VideoModule {
	VideoModule *next;
	const char *name;
	const char *help;
	int (*init)(void);
	void (*shutdown)(void);
	void (*fillrect)(uint_least16_t x, uint_least16_t y,
			uint_least16_t w, uint_least16_t h, uint32_t colour);
	void (*blit)(uint_least16_t x, uint_least16_t y, Sprite *src);
	void (*backup)(void);
	void (*restore)(void);
	void (*resize)(uint_least16_t w, uint_least16_t h);
	void (*vdg_reset)(void);
	void (*vdg_vsync)(void);
	void (*vdg_set_mode)(unsigned int mode);
	void (*vdg_render_sg4)(void);
	void (*vdg_render_sg6)(void);
	void (*vdg_render_cg1)(void);
	void (*vdg_render_rg1)(void);
	void (*vdg_render_cg2)(void);
	void (*vdg_render_rg6)(void);
	void (*render_border)(void);
};

extern VideoModule *video_module;
extern int video_artifact_mode;

void video_getargs(int argc, char **argv);
int video_init(void);
void video_shutdown(void);
void video_next(void);

#endif  /* __VIDEO_H__ */
