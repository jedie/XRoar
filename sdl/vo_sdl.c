/*  Copyright 2003-2013 Ciaran Anscomb
 *
 *  This file is part of XRoar.
 *
 *  XRoar is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  XRoar is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XRoar.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include <SDL_syswm.h>

#include "logging.h"
#include "module.h"
#include "vdg.h"
#include "xroar.h"

#ifdef WINDOWS32
#include "windows32/common_windows32.h"
#endif

static int init(void);
static void shutdown(void);
static void alloc_colours(void);
static void vsync(void);
static void render_scanline(uint8_t *scanline_data);
static int set_fullscreen(_Bool fullscreen);
static void update_cross_colour_phase(void);

VideoModule video_sdl_module = {
	.common = { .name = "sdl", .description = "Minimal SDL video",
	            .init = init, .shutdown = shutdown },
	.update_palette = alloc_colours,
	.vsync = vsync,
	.render_scanline = render_scanline,
	.set_fullscreen = set_fullscreen,
	.update_cross_colour_phase = update_cross_colour_phase,
};

typedef Uint8 Pixel;
#define RESET_PALETTE() reset_palette()
#define MAPCOLOUR(r,g,b) alloc_and_map(r, g, b)
#define VIDEO_SCREENBASE ((Pixel *)screen->pixels)
#define XSTEP 1
#define NEXTLINE 0
#define VIDEO_TOPLEFT (VIDEO_SCREENBASE)
#define VIDEO_VIEWPORT_YOFFSET (0)
#define LOCK_SURFACE SDL_LockSurface(screen)
#define UNLOCK_SURFACE SDL_UnlockSurface(screen)
#define VIDEO_MODULE_NAME video_sdl_module

static SDL_Surface *screen;

static int palette_index = 0;

static void reset_palette(void) {
	palette_index = 0;
}

static Pixel alloc_and_map(int r, int g, int b) {
	SDL_Color c;
	c.r = r;
	c.g = g;
	c.b = b;
	SDL_SetPalette(screen, SDL_LOGPAL|SDL_PHYSPAL, &c, palette_index, 1);
	palette_index++;
	palette_index %= 256;
	return SDL_MapRGB(screen->format, r, g, b);
}

#include "vo_generic_ops.c"

static int init(void) {
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
		LOG_ERROR("Failed to initialise SDL video: %s\n", SDL_GetError());
		return 1;
	}
	if (set_fullscreen(xroar_opt_fullscreen))
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
	vsync();
	return 0;
}

static void shutdown(void) {
	set_fullscreen(0);
	/* Should not be freed by caller: SDL_FreeSurface(screen); */
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

static int set_fullscreen(_Bool fullscreen) {
	screen = SDL_SetVideoMode(320, 240, 8, SDL_HWSURFACE|(fullscreen?SDL_FULLSCREEN:0));
	if (screen == NULL) {
		LOG_ERROR("Failed to allocate SDL surface for display\n");
		return 1;
	}
	SDL_WM_SetCaption("XRoar", "XRoar");
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
}
