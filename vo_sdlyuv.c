/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2010  Ciaran Anscomb
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

#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include <SDL/SDL_syswm.h>

#include "types.h"
#include "logging.h"
#include "module.h"
#include "vdg_bitmaps.h"
#include "xroar.h"
#ifdef WINDOWS32
#include "common_windows32.h"
#endif

static int init(void);
static void shutdown(void);
static void resize(unsigned int w, unsigned int h);
static int set_fullscreen(int fullscreen);
static void vsync(void);
static void set_mode(unsigned int mode);
static void render_border(void);
static void alloc_colours(void);
static void hsync(void);

VideoModule video_sdlyuv_module = {
	{ "sdlyuv", "SDL YUV overlay, hopefully uses Xv acceleration",
	  init, 0, shutdown },
	resize, set_fullscreen, 0,
	vsync, set_mode,
	render_border, NULL, hsync
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
#define VIDEO_MODULE_NAME video_sdlyuv_module

static SDL_Surface *screen;
static SDL_Overlay *overlay;
static unsigned int screen_width, screen_height;
static unsigned int window_width, window_height;
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

#include "vo_generic_ops.c"

static int init(void) {
	const SDL_VideoInfo *video_info;

	LOG_DEBUG(2,"Initialising SDL-YUV video driver\n");
#ifdef WINDOWS32
	if (!getenv("SDL_VIDEODRIVER"))
		putenv("SDL_VIDEODRIVER=windib");
#endif
	if (!SDL_WasInit(SDL_INIT_NOPARACHUTE)) {
		if (SDL_Init(SDL_INIT_NOPARACHUTE) < 0) {
			LOG_ERROR("Failed to initialise SDL: %s\n", SDL_GetError());
			return 1;
		}
	}
	if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
		LOG_ERROR("Failed to initialise SDL-YUV video driver: %s\n", SDL_GetError());
		return 1;
	}

	video_info = SDL_GetVideoInfo();
	screen_width = video_info->current_w;
	screen_height = video_info->current_h;
	window_width = 640;
	window_height = 480;

	if (set_fullscreen(xroar_fullscreen))
		return 1;
	overlay = SDL_CreateYUVOverlay(640, 240, SDL_YUY2_OVERLAY, screen);
	if (overlay == NULL) {
		LOG_ERROR("Failed to create SDL overlay for display: %s\n", SDL_GetError());
		return 1;
	}
	if (overlay->hw_overlay != 1) {
		LOG_WARN("Warning: SDL overlay is not hardware accelerated\n");
	}
	alloc_colours();
#ifdef WINDOWS32
	{
		SDL_version sdlver;
		SDL_SysWMinfo sdlinfo;
		SDL_VERSION(&sdlver);
		sdlinfo.version = sdlver;
		SDL_GetWMInfo(&sdlinfo);
		windows32_main_hwnd = sdlinfo.window;
	}
#endif
	set_mode(0);
	return 0;
}

static void shutdown(void) {
	LOG_DEBUG(2,"Shutting down SDL-YUV video driver\n");
	set_fullscreen(0);
	SDL_FreeYUVOverlay(overlay);
	/* Should not be freed by caller: SDL_FreeSurface(screen); */
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

static void resize(unsigned int w, unsigned int h) {
	window_width = w;
	window_height = h;
	set_fullscreen(video_sdlyuv_module.is_fullscreen);
}

static int set_fullscreen(int fullscreen) {
	unsigned int want_width, want_height;

	if (fullscreen) {
		want_width = screen_width;
		want_height = screen_height;
	} else {
		want_width = window_width;
		want_height = window_height;
	}
	if (want_width < 320) want_width = 320;
	if (want_height < 240) want_height = 240;

	screen = SDL_SetVideoMode(want_width, want_height, 0, SDL_HWSURFACE|SDL_ANYFORMAT|(fullscreen?SDL_FULLSCREEN:SDL_RESIZABLE));
	if (screen == NULL) {
		LOG_ERROR("Failed to allocate SDL surface for display\n");
		return 1;
	}
	if (fullscreen)
		SDL_ShowCursor(SDL_DISABLE);
	else
		SDL_ShowCursor(SDL_ENABLE);
	video_sdlyuv_module.is_fullscreen = fullscreen;

	memcpy(&dstrect, &screen->clip_rect, sizeof(SDL_Rect));
	if (((float)screen->w/(float)screen->h)>(4.0/3.0)) {
		dstrect.w = ((float)screen->h/3.0)*4;
		dstrect.h = screen->h;
		dstrect.x = (screen->w - dstrect.w)/2;
		dstrect.y = 0;
	} else {
		dstrect.w = screen->w;
		dstrect.h = ((float)screen->w/4.0)*3;
		dstrect.x = 0;
		dstrect.y = (screen->h - dstrect.h)/2;
	}
	return 0;
}

static void vsync(void) {
	SDL_DisplayYUVOverlay(overlay, &dstrect);
	pixel = VIDEO_TOPLEFT + VIDEO_VIEWPORT_YOFFSET;
	subline = 0;
	beam_pos = 0;
}
