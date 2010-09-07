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
#include <SDL_opengl.h>
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

VideoModule video_sdlgl_module = {
	{ "sdlgl", "SDL OpenGL",
	  init, 0, shutdown },
	resize, set_fullscreen, 0,
	vsync, set_mode,
	render_border, NULL, hsync
};

typedef uint16_t Pixel;
#define MAP_565(r,g,b) ( (((r) & 0xf8) << 8) | (((g) & 0xfc) << 3) | (((b) & 0xf8) >> 3) )
#define MAP_332(r,g,b) ( ((r) & 0xe0) | (((g) & 0xe0) >> 3) | (((b) & 0xc0) >> 6) )
#define MAPCOLOUR(r,g,b) MAP_565(r,g,b)
#define VIDEO_SCREENBASE (screen_tex)
#define XSTEP 1
#define NEXTLINE 0
#define VIDEO_TOPLEFT VIDEO_SCREENBASE
#define VIDEO_VIEWPORT_YOFFSET (0)
#define LOCK_SURFACE
#define UNLOCK_SURFACE
#define VIDEO_MODULE_NAME video_sdlgl_module

static SDL_Surface *screen;
static Pixel *screen_tex;
static unsigned int screen_width, screen_height;
static unsigned int window_width, window_height;
static GLuint texnum = 0;
static GLint xoffset, yoffset;

static enum {
	FILTER_AUTO,
	FILTER_NEAREST,
	FILTER_LINEAR,
} filter;

#include "vo_generic_ops.c"

static int init(void) {
	const SDL_VideoInfo *video_info;

	LOG_DEBUG(2,"Initialising SDL OpenGL driver\n");
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
		LOG_ERROR("Failed to initialise SDL OpenGL driver: %s\n", SDL_GetError());
		return 1;
	}

	filter = FILTER_AUTO;
	if (xroar_opt_gl_filter) {
		if (0 == strcmp(xroar_opt_gl_filter, "nearest")) {
			filter = FILTER_NEAREST;
		} else if (0 == strcmp(xroar_opt_gl_filter, "linear")) {
			filter = FILTER_LINEAR;
		}
	}

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE,   5);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,  5);
	SDL_GL_SetAttribute(SDL_GL_BUFFER_SIZE, 16);

	screen_tex = malloc(320 * 240 * sizeof(Pixel));
	if (screen_tex == NULL) {
		LOG_ERROR("Failed to allocate memory for screen texture\n");
		return 1;
	}

	video_info = SDL_GetVideoInfo();
	screen_width = video_info->current_w;
	screen_height = video_info->current_h;
	window_width = 640;
	window_height = 480;
	xoffset = yoffset = 0;

	if (set_fullscreen(xroar_fullscreen))
		return 1;

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
	LOG_DEBUG(2,"Shutting down SDL OpenGL driver\n");
	set_fullscreen(0);
	glDeleteTextures(1, &texnum);
	free(screen_tex);
	/* Should not be freed by caller: SDL_FreeSurface(screen); */
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

static void resize(unsigned int w, unsigned int h) {
	window_width = w;
	window_height = h;
	set_fullscreen(video_sdlgl_module.is_fullscreen);
}

static int set_fullscreen(int fullscreen) {
	unsigned int want_width, want_height;
	unsigned int width, height;

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
	if (fullscreen)
		SDL_ShowCursor(SDL_DISABLE);
	else
		SDL_ShowCursor(SDL_ENABLE);
	video_sdlgl_module.is_fullscreen = fullscreen;

	if (((float)screen->w/(float)screen->h)>(4.0/3.0)) {
		height = screen->h;
		width = ((float)height/3.0)*4;
		xoffset = (screen->w - width) / 2;
		yoffset = 0;
	} else {
		width = screen->w;
		height = ((float)width/4.0)*3;
		xoffset = 0;
		yoffset = (screen->h - height)/2;
	}

	/* Configure OpenGL */
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_2D);

	glViewport(0, 0, screen->w, screen->h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, screen->w, screen->h , 0, -1.0, 1.0);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClearDepth(1.0f);

	glDeleteTextures(1, &texnum);
	glGenTextures(1, &texnum);
	glBindTexture(GL_TEXTURE_2D, texnum);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5, 512, 256, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	if (filter == FILTER_NEAREST || (filter == FILTER_AUTO && (width % 320) == 0 && (height % 240) == 0)) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	} else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	/* There must be a better way of just clearing the texture? */
	memset(screen_tex, 0, 512 * sizeof(Pixel));
	glTexSubImage2D(GL_TEXTURE_2D, 0, 320,   0,   1, 256,
			GL_RGB, GL_UNSIGNED_SHORT_5_6_5, screen_tex);
	glTexSubImage2D(GL_TEXTURE_2D, 0,   0, 240, 512,   1,
			GL_RGB, GL_UNSIGNED_SHORT_5_6_5, screen_tex);

	return 0;
}

static void vsync(void) {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glColor4f(1.0, 1.0, 1.0, 1.0);
	/* Draw main window */
	glBindTexture(GL_TEXTURE_2D, texnum);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
			320, 240, GL_RGB,
			GL_UNSIGNED_SHORT_5_6_5, screen_tex);
	glBegin(GL_QUADS);
	glTexCoord2f(0.0, 0.0);
	glVertex3i(xoffset, yoffset, 0);
	glTexCoord2f(0.0, 0.9375);
	glVertex3i(xoffset, screen->h - yoffset, 0);
	glTexCoord2f(0.625, 0.9375);
	glVertex3i(screen->w - xoffset, screen->h - yoffset, 0);
	glTexCoord2f(0.625, 0.0);
	glVertex3i(screen->w - xoffset, yoffset, 0);
	glEnd();
	SDL_GL_SwapBuffers();
	pixel = VIDEO_TOPLEFT + VIDEO_VIEWPORT_YOFFSET;
	subline = 0;
	beam_pos = 0;
}
