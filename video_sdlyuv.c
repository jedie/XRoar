/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2006  Ciaran Anscomb
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

#ifdef HAVE_SDL

#include <stdlib.h>
#include <string.h>
#include <SDL.h>

#include "joystick.h"
#include "keyboard.h"
#include "logging.h"
#include "sam.h"
#include "types.h"
#include "vdg.h"
#include "video.h"
#include "xroar.h"

static int init(void);
static void shutdown(void);
static void fillrect(uint_least16_t x, uint_least16_t y,
		uint_least16_t w, uint_least16_t h, uint32_t colour);
static void blit(uint_least16_t x, uint_least16_t y, Sprite *src);
/* static void backup(void);
static void restore(void); */
static void resize(uint_least16_t w, uint_least16_t h);
static void toggle_fullscreen(void);
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
static void alloc_colours(void);

extern KeyboardModule keyboard_sdl_module;
extern JoystickModule joystick_sdl_module;

VideoModule video_sdlyuv_module = {
	"sdlyuv",
	"SDL YUV overlay, hopefully uses Xv acceleration",
	init, shutdown,
	fillrect, blit,
	NULL, NULL, resize, toggle_fullscreen,
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

static SDL_Surface *screen;
static SDL_Overlay *overlay;
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

#include "video_generic_ops.c"

static int init(void) {
	LOG_DEBUG(2,"Initialising SDL-YUV video driver\n");
#ifdef WINDOWS32
	if (!getenv("SDL_VIDEODRIVER"))
		putenv("SDL_VIDEODRIVER=windib");
#endif
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
	screen = SDL_SetVideoMode(640, 480, 32, SDL_HWSURFACE|SDL_RESIZABLE|(video_want_fullscreen?SDL_FULLSCREEN:0));
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
	if (video_want_fullscreen)
		SDL_ShowCursor(SDL_DISABLE);
	else
		SDL_ShowCursor(SDL_ENABLE);
	memcpy(&dstrect, &screen->clip_rect, sizeof(SDL_Rect));
	fillrect(0,0,320,240,0);
	alloc_colours();
	/* Set preferred keyboard driver */
	keyboard_module = &keyboard_sdl_module;
	joystick_module = &joystick_sdl_module;
	return 0;
}

static void shutdown(void) {
	LOG_DEBUG(2,"Shutting down SDL-YUV video driver\n");
	if (video_want_fullscreen)
		toggle_fullscreen();
	SDL_FreeYUVOverlay(overlay);
	/* Should not be freed by caller: SDL_FreeSurface(screen); */
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

static void toggle_fullscreen(void) {
	video_want_fullscreen = !video_want_fullscreen;
	resize(320, 240);
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
	screen = SDL_SetVideoMode(w, h, 32, SDL_HWSURFACE|SDL_RESIZABLE|(video_want_fullscreen?SDL_FULLSCREEN:0));
	if (video_want_fullscreen)
		SDL_ShowCursor(SDL_DISABLE);
	else
		SDL_ShowCursor(SDL_ENABLE);
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

#endif  /* HAVE_SDL */
