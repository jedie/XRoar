/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2005  Ciaran Anscomb
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_SDL_VIDEO

#include <stdlib.h>
#include <string.h>
#include <SDL.h>

#include "video.h"
#include "keyboard.h"
#include "joystick.h"
#include "sam.h"
#include "types.h"
#include "logging.h"

static int init(void);
static void shutdown(void);
static void fillrect(uint_least16_t x, uint_least16_t y,
		uint_least16_t w, uint_least16_t h, uint32_t colour);
static void blit(uint_least16_t x, uint_least16_t y, Sprite *src);
/* static void backup(void);
static void restore(void); */
static void resize(uint_least16_t w, uint_least16_t h);
static void reset(void);
static void vsync(void);
static void set_mode(unsigned int mode);
static void render_sg4(void);
/* static void render_sg6(void); */
static void render_cg1(void);
static void render_rg1(void);
static void render_cg2(void);
static void render_rg6(void);
static void render_border(void);

extern KeyboardModule keyboard_sdl_module;
extern JoystickModule joystick_sdl_module;

VideoModule video_sdlyuv_module = {
	NULL,
	"sdlyuv",
	"SDL YUV overlay, hopefully uses Xv acceleration",
	init, shutdown,
	fillrect, blit,
	NULL, NULL, resize,
	reset, vsync, set_mode,
	render_sg4, render_sg4 /* 6 */, render_cg1,
	render_rg1, render_cg2, render_rg6,
	render_border
};

typedef Uint32 Pixel;
#define MAPCOLOUR(r,g,b) map_colour((r), (g), (b))
#define VIDEO_SCREENBASE ((Pixel *)overlay->pixels[0])
#define XSTEP 1
#define NEXTLINE 0
#define VIDEO_TOPLEFT (VIDEO_SCREENBASE)
#define VIDEO_VIEWPORT_YOFFSET (0)
#define LOCK_SURFACE SDL_LockYUVOverlay(overlay)
#define UNLOCK_SURFACE SDL_UnlockYUVOverlay(overlay)
extern unsigned int vdg_alpha[768];

static unsigned int subline;
static Pixel *pixel;
static Pixel darkgreen, black;
static Pixel bg_colour;
static Pixel fg_colour;
static Pixel vdg_colour[16];
static Pixel *cg_colours;
static Pixel border_colour;

SDL_Surface *screen;
SDL_Overlay *overlay;
static SDL_Rect dstrect;

static Uint32 map_colour(int r, int g, int b) {
	Uint32 colour;
	uint8_t *d = (uint8_t *)&colour;
	/* From SDL example code */
	d[0] = d[2] = 0.299*r + 0.587*g + 0.114*b;
	d[1] = (b-d[0])*0.565 + 128;
	d[3] = (r-d[0])*0.713 + 128;
	return colour;
}

static int init(void) {
	LOG_DEBUG(2,"Initialising SDL-YUV video driver\n");
	if (!SDL_WasInit(SDL_INIT_NOPARACHUTE)) {
		if (SDL_Init(SDL_INIT_NOPARACHUTE) < 0) {
			LOG_ERROR("Failed to initialiase SDL\n");
			return 1;
		}
	}
	if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
		LOG_ERROR("Failed to initialiase SDL-YUV video driver\n");
		return 1;
	}
	screen = SDL_SetVideoMode(640, 480, 32, SDL_HWSURFACE|SDL_RESIZABLE);
	if (screen == NULL) {
		LOG_ERROR("Failed to allocate SDL surface for display\n");
		return 1;
	}
	overlay = SDL_CreateYUVOverlay(640, 240, SDL_YUY2_OVERLAY, screen);
	if (overlay == NULL) {
		LOG_ERROR("Failed to create SDL overlay for display: %s\n", SDL_GetError());
		return 1;
	}
	if (overlay->hw_overlay != 1) {
		LOG_WARN("Warning: SDL overlay is not hardware accelerated\n");
	}
	memcpy(&dstrect, &screen->clip_rect, sizeof(SDL_Rect));
	fillrect(0,0,320,240,0);
	vdg_colour[0] = MAPCOLOUR(0x00, 0xff, 0x00);
	vdg_colour[1] = MAPCOLOUR(0xff, 0xff, 0x00);
	vdg_colour[2] = MAPCOLOUR(0x00, 0x00, 0xff);
	vdg_colour[3] = MAPCOLOUR(0xff, 0x00, 0x00);
	vdg_colour[4] = MAPCOLOUR(0xff, 0xe0, 0xe0);
	vdg_colour[5] = MAPCOLOUR(0x00, 0xff, 0xff);
	vdg_colour[6] = MAPCOLOUR(0xff, 0x00, 0xff);
	vdg_colour[7] = MAPCOLOUR(0xff, 0xa5, 0x00);
	vdg_colour[8] = MAPCOLOUR(0x00, 0x00, 0x00);
	vdg_colour[9] = MAPCOLOUR(0x00, 0x80, 0xff);
	vdg_colour[10] = MAPCOLOUR(0xff, 0x80, 0x00);
	vdg_colour[11] = MAPCOLOUR(0xff, 0xff, 0xff);
	vdg_colour[12] = MAPCOLOUR(0x00, 0x00, 0x00);
	vdg_colour[13] = MAPCOLOUR(0xff, 0x80, 0x00);
	vdg_colour[14] = MAPCOLOUR(0x00, 0x80, 0xff);
	vdg_colour[15] = MAPCOLOUR(0xff, 0xff, 0xff);
	black = MAPCOLOUR(0x00, 0x00, 0x00);
	darkgreen = MAPCOLOUR(0x00, 0x20, 0x00);
	/* Set preferred keyboard driver */
	keyboard_module = &keyboard_sdl_module;
	joystick_module = &joystick_sdl_module;
	return 0;
}

static void shutdown(void) {
	LOG_DEBUG(2,"Shutting down SDL-YUV video driver\n");
	SDL_FreeYUVOverlay(overlay);
	SDL_FreeSurface(screen);
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

static void fillrect(uint_least16_t x, uint_least16_t y, uint_least16_t w, uint_least16_t h, uint32_t colour) {
	Pixel *p1 = VIDEO_SCREENBASE + (239 - y) * 320 + x;
	Pixel c = MAPCOLOUR(colour >> 24, (colour >> 16) & 0xff,
			(colour >> 8) & 0xff);
	int skip = -320 - w;
	uint_least16_t i;
	for (; h; h--) {
		for (i = w; i; i--) {
			*(p1++) = c;
		}
		p1 += skip;
	}
}

static void blit(uint_least16_t x, uint_least16_t y, Sprite *src) {
	(void)x;  /* unused */
	(void)y;  /* unused */
	(void)src;  /* unused */
}

static void resize(uint_least16_t w, uint_least16_t h) {
	if (w < 640) w = 640;
	if (h < 480) h = 480;
	if (((float)w/(float)h)>(320.0/240.0)) {
		dstrect.w = ((float)h/240.0)*320;
		dstrect.h = h;
		dstrect.x = (w - dstrect.w)/2;
		dstrect.y = 0;
	} else {
		dstrect.w = w;
		dstrect.h = ((float)w/320.0)*240;
		dstrect.x = 0;
		dstrect.y = (h - dstrect.h)/2;
	}
	screen = SDL_SetVideoMode(w, h, 32, SDL_HWSURFACE|SDL_RESIZABLE);
}

static void reset(void) {
	pixel = VIDEO_TOPLEFT + VIDEO_VIEWPORT_YOFFSET;
	subline = 0;
}

static void vsync(void) {
	SDL_DisplayYUVOverlay(overlay, &dstrect);
	pixel = VIDEO_TOPLEFT + VIDEO_VIEWPORT_YOFFSET;
	subline = 0;
}

static void set_mode(unsigned int mode) {
	if (mode & 0x80) {
		/* Graphics modes */
		if (((mode & 0x70) == 0x70) && video_artifact_mode) {
			cg_colours = &vdg_colour[4 + video_artifact_mode * 4];
			fg_colour = vdg_colour[(mode & 0x08) >> 1];
		} else {
			cg_colours = &vdg_colour[(mode & 0x08) >> 1];
			fg_colour = cg_colours[0];
		}
		bg_colour = black;
		border_colour = fg_colour;
	} else {
		bg_colour = darkgreen;
		border_colour = black;
		if (mode & 0x08)
			fg_colour = vdg_colour[7];
		else
			fg_colour = vdg_colour[0];
	}
}

#include "video_generic_ops.c"

#endif  /* HAVE_SDL_VIDEO */
