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

#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_syswm.h>

#include "logging.h"
#include "module.h"
#include "vdg.h"
#include "vo_opengl.h"
#include "xroar.h"

#ifdef WINDOWS32
#include "windows32/common_windows32.h"
#endif

static int init(void);
static void shutdown(void);
static void vsync(void);
static void resize(unsigned int w, unsigned int h);
static int set_fullscreen(_Bool fullscreen);

VideoModule video_sdlgl_module = {
	.common = { .name = "sdlgl", .description = "SDL OpenGL video",
	            .init = init, .shutdown = shutdown },
	.update_palette = vo_opengl_alloc_colours,
	.vsync = vsync,
	.render_scanline = vo_opengl_render_scanline,
	.resize = resize, .set_fullscreen = set_fullscreen,
	.update_cross_colour_phase = vo_opengl_update_cross_colour_phase,
};

static SDL_Surface *screen;
static unsigned int screen_width, screen_height;
static unsigned int window_width, window_height;

static int init(void) {
	const SDL_VideoInfo *video_info;

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

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE,   5);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,  5);
	SDL_GL_SetAttribute(SDL_GL_BUFFER_SIZE, 16);

	vo_opengl_init();

	video_info = SDL_GetVideoInfo();
	screen_width = video_info->current_w;
	screen_height = video_info->current_h;
	window_width = 640;
	window_height = 480;

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
	vo_opengl_shutdown();
	/* Should not be freed by caller: SDL_FreeSurface(screen); */
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

static void resize(unsigned int w, unsigned int h) {
	window_width = w;
	window_height = h;
	set_fullscreen(video_sdlgl_module.is_fullscreen);
}

static int set_fullscreen(_Bool fullscreen) {
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

	screen = SDL_SetVideoMode(want_width, want_height, 0, SDL_OPENGL|(fullscreen?SDL_FULLSCREEN:SDL_RESIZABLE));
	if (screen == NULL) {
		LOG_ERROR("Failed to initialise display\n");
		return 1;
	}
	SDL_WM_SetCaption("XRoar", "XRoar");
	if (fullscreen)
		SDL_ShowCursor(SDL_DISABLE);
	else
		SDL_ShowCursor(SDL_ENABLE);
	video_sdlgl_module.is_fullscreen = fullscreen;

	vo_opengl_set_window_size(want_width, want_height);

	return 0;
}

static void vsync(void) {
	vo_opengl_vsync();
	SDL_GL_SwapBuffers();
}
