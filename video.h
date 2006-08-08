/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2006  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef __VIDEO_H__
#define __VIDEO_H__

typedef struct VideoModule VideoModule;
struct VideoModule {
	const char *name;
	const char *help;
	int (*init)(void);
	void (*shutdown)(void);
	void (*resize)(unsigned int w, unsigned int h);
	int (*set_fullscreen)(int fullscreen);
	int is_fullscreen;
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
extern int video_want_fullscreen;

void video_helptext(void);
void video_getargs(int argc, char **argv);
int video_init(void);
void video_shutdown(void);
void video_next(void);

#endif  /* __VIDEO_H__ */
