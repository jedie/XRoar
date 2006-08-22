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

#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include <SDL_opengl.h>

#include "types.h"
#include "logging.h"
#include "module.h"
#include "sam.h"
#include "ui_sdl.h"
#include "vdg.h"
#include "xroar.h"

static int init(int argc, char **argv);
static void shutdown(void);
static void resize(unsigned int w, unsigned int h);
static int set_fullscreen(int fullscreen);
static void vsync(void);
static void set_mode(unsigned int mode);
static void render_sg4(void);
static void render_sg6(void);
static void render_cg1(void);
static void render_rg1(void);
static void render_cg2(void);
static void render_rg6(void);
static void render_border(void);
static void alloc_colours(void);

VideoModule video_sdlgl_module = {
	{ "sdlgl", "SDL OpenGL",
	  init, 0, shutdown, NULL },
	resize, set_fullscreen, 0,
	vsync, set_mode,
	render_sg4, render_sg6, render_cg1,
	render_rg1, render_cg2, render_rg6,
	render_border
};

typedef Uint16 Pixel;
#define MAPCOLOUR(r,g,b) SDL_MapRGB(screen_tex->format, r, g, b)
#define VIDEO_SCREENBASE ((Pixel *)screen_tex->pixels)
#define XSTEP 1
#define NEXTLINE 192
#define VIDEO_TOPLEFT VIDEO_SCREENBASE
#define VIDEO_VIEWPORT_YOFFSET (0)
#define LOCK_SURFACE SDL_LockSurface(screen_tex)
#define UNLOCK_SURFACE SDL_UnlockSurface(screen_tex)

static SDL_Surface *screen;
static SDL_Surface *screen_tex;
static unsigned int screen_width = 640;
static unsigned int screen_height = 480;
static GLuint texnum = 0;
static GLint xoffset, yoffset;

#include "video_generic_ops.c"

static int init(int argc, char **argv) {
	(void)argc;
	(void)argv;
	LOG_DEBUG(2,"Initialising SDL OpenGL driver\n");
#ifdef WINDOWS32
	if (!getenv("SDL_VIDEODRIVER"))
		putenv("SDL_VIDEODRIVER=windib");
#endif
	if (!SDL_WasInit(SDL_INIT_NOPARACHUTE)) {
		if (SDL_Init(SDL_INIT_NOPARACHUTE) < 0) {
			LOG_ERROR("Failed to initialiase SDL OpenGL\n");
			return 1;
		}
	}
	if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
		LOG_ERROR("Failed to initialiase SDL OpenGL driver\n");
		return 1;
	}
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE,   5);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,  5);
	SDL_GL_SetAttribute(SDL_GL_BUFFER_SIZE, 16);

	/* Texture dimensions are next power of 2 greater than what we want */
	screen_tex = SDL_CreateRGBSurface(SDL_SWSURFACE, 512, 256, 16, 0, 0, 0, 0);
	if (screen_tex == NULL) {
		LOG_ERROR("Failed to create surface for screen texture\n");
		return 1;
	}

	if (set_fullscreen(sdl_video_want_fullscreen))
		return 1;

	xoffset = yoffset = 0;
	alloc_colours();
	return 0;
}

static void shutdown(void) {
	LOG_DEBUG(2,"Shutting down SDL OpenGL driver\n");
	set_fullscreen(0);
	glDeleteTextures(1, &texnum);
	SDL_FreeSurface(screen_tex);
	/* Should not be freed by caller: SDL_FreeSurface(screen); */
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

static void resize(unsigned int w, unsigned int h) {
	screen_width = w;
	screen_height = h;
	set_fullscreen(video_sdlgl_module.is_fullscreen);
}

static int set_fullscreen(int fullscreen) {
	uint_least16_t width, height;
	if (screen_width < 320) screen_width = 320;
	if (screen_height < 240) screen_height = 240;
	if (fullscreen) {
		screen_width = 640;
		screen_height = 480;
	}
	if (((float)screen_width/(float)screen_height)>(4.0/3.0)) {
		width = ((float)screen_height/3.0)*4;
		//height = screen_height;
		xoffset = (screen_width - width) / 2;
		yoffset = 0;
	} else {
		//width = screen_width;
		height = ((float)screen_width/4.0)*3;
		xoffset = 0;
		yoffset = (screen_height - height)/2;
	}
	screen = SDL_SetVideoMode(screen_width, screen_height, 0, SDL_OPENGL|(fullscreen?SDL_FULLSCREEN:SDL_RESIZABLE));
	if (screen == NULL) {
		LOG_ERROR("Failed to initialise display\n");
		return 1;
	}
	if (fullscreen)
		SDL_ShowCursor(SDL_DISABLE);
	else
		SDL_ShowCursor(SDL_ENABLE);
	video_sdlgl_module.is_fullscreen = fullscreen;

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
	glClearColor(0.0f, 0.0f, 0.0f, 0.5f);
	glClearDepth(1.0f);

	glDeleteTextures(1, &texnum);
	glGenTextures(1, &texnum);
	glBindTexture(GL_TEXTURE_2D, texnum);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5, 512, 256, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	return 0;
}

static void vsync(void) {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glColor4f(1.0, 1.0, 1.0, 1.0);
	/* Draw main window */
	glBindTexture(GL_TEXTURE_2D, texnum);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
			512, 256, GL_RGB,
			GL_UNSIGNED_SHORT_5_6_5, screen_tex->pixels);
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
}
