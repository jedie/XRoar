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

#ifdef HAVE_SDL

#include <stdlib.h>
#include <string.h>
#include <SDL.h>

#include "video.h"
#include "keyboard.h"
#include "joystick.h"
#include "sam.h"
#include "types.h"
#include "logging.h"
#include "vdg.h"
#include "xroar.h"

static int init(void);
static void shutdown(void);
static void fillrect(uint_least16_t x, uint_least16_t y,
		uint_least16_t w, uint_least16_t h, uint32_t colour);
static void blit(uint_least16_t x, uint_least16_t y, Sprite *src);
/* static void backup(void);
static void restore(void); */
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

VideoModule video_sdl_module = {
	NULL,
	"sdl",
	"Standard SDL surface",
	init, shutdown,
	fillrect, blit,
	NULL, NULL, NULL,
	reset, vsync, set_mode,
	render_sg4, render_sg4 /* 6 */, render_cg1,
	render_rg1, render_cg2, render_rg6,
	render_border
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
//extern unsigned int vdg_alpha[768];

static SDL_Surface *screen;

#include "video_generic_ops.c"

static int init(void) {
	LOG_DEBUG(2,"Initialising SDL video driver\n");
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
		LOG_ERROR("Failed to initialiase SDL video driver\n");
		return 1;
	}
	screen = SDL_SetVideoMode(320, 240, 8, SDL_SWSURFACE);
	if (screen == NULL) {
		LOG_ERROR("Failed to allocate SDL surface for display\n");
		return 1;
	}
	fillrect(0,0,320,240,0);
	alloc_colours();
	/* Set preferred keyboard & joystick drivers */
	keyboard_module = &keyboard_sdl_module;
	joystick_module = &joystick_sdl_module;
	return 0;
}

static void shutdown(void) {
	LOG_DEBUG(2,"Shutting down SDL video driver\n");
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

static void reset(void) {
	pixel = VIDEO_TOPLEFT + VIDEO_VIEWPORT_YOFFSET;
	subline = 0;
}

static void vsync(void) {
	SDL_UpdateRect(screen, 0, 0, 320, 240);
	pixel = VIDEO_TOPLEFT + VIDEO_VIEWPORT_YOFFSET;
	subline = 0;
}

#endif  /* HAVE_SDL */
