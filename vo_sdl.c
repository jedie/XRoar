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
static int set_fullscreen(int fullscreen);
static void vsync(void);
static void set_mode(unsigned int mode);
static void render_border(void);
static void alloc_colours(void);
static void hsync(void);

VideoModule video_sdl_module = {
	{ "sdl", "Standard SDL surface",
	  init, 0, shutdown },
	NULL, set_fullscreen, 0,
	vsync, set_mode,
	render_border, NULL, hsync
};

typedef Uint8 Pixel;
#define MAPCOLOUR(r,g,b) SDL_MapRGB(screen->format, r, g, b)
#define VIDEO_SCREENBASE ((Pixel *)screen->pixels)
#define XSTEP 1
#define NEXTLINE 0
#define VIDEO_TOPLEFT (VIDEO_SCREENBASE)
#define VIDEO_VIEWPORT_YOFFSET (0)
#define LOCK_SURFACE SDL_LockSurface(screen)
#define UNLOCK_SURFACE SDL_UnlockSurface(screen)
#define VIDEO_MODULE_NAME video_sdl_module

static SDL_Surface *screen;

#include "vo_generic_ops.c"

static int init(void) {
	LOG_DEBUG(2,"Initialising SDL video driver\n");
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
		LOG_ERROR("Failed to initialise SDL video driver: %s\n", SDL_GetError());
		return 1;
	}
	if (set_fullscreen(xroar_fullscreen))
		return 1;
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
	LOG_DEBUG(2,"Shutting down SDL video driver\n");
	set_fullscreen(0);
	/* Should not be freed by caller: SDL_FreeSurface(screen); */
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

static int set_fullscreen(int fullscreen) {
	screen = SDL_SetVideoMode(320, 240, 8, SDL_HWSURFACE|(fullscreen?SDL_FULLSCREEN:0));
	if (screen == NULL) {
		LOG_ERROR("Failed to allocate SDL surface for display\n");
		return 1;
	}
	pixel = VIDEO_TOPLEFT + VIDEO_VIEWPORT_YOFFSET;
	alloc_colours();
	if (fullscreen)
		SDL_ShowCursor(SDL_DISABLE);
	else
		SDL_ShowCursor(SDL_ENABLE);
	video_sdl_module.is_fullscreen = fullscreen;
	return 0;
}

static void vsync(void) {
	SDL_UpdateRect(screen, 0, 0, 320, 240);
	pixel = VIDEO_TOPLEFT + VIDEO_VIEWPORT_YOFFSET;
	subline = 0;
	beam_pos = 0;
}
