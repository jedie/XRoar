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

#ifdef HAVE_SDLGL

#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include <SDL_opengl.h>

#include "types.h"
#include "joystick.h"
#include "keyboard.h"
#include "logging.h"
#include "sam.h"
#include "vdg.h"
#include "video.h"
#include "xroar.h"

static int init(void);
static void shutdown(void);
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

VideoModule video_sdlgl_module = {
	"sdlgl",
	"SDL OpenGL",
	init, shutdown,
	resize, toggle_fullscreen,
	reset, vsync, set_mode,
	render_sg4, render_sg4 /* 6 */, render_cg1,
	render_rg1, render_cg2, render_rg6,
	render_border
};

extern KeyboardModule keyboard_sdl_module;
extern JoystickModule joystick_sdl_module;

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
static GLuint texnum;
static GLint xoffset, yoffset;

static void config_opengl(void);

#include "video_generic_ops.c"

static int init(void) {
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
	screen = SDL_SetVideoMode(640, 480, 0, SDL_OPENGL|SDL_RESIZABLE|(video_want_fullscreen?SDL_FULLSCREEN:0));
	if (screen == NULL) {
		LOG_ERROR("Failed to initialise display\n");
		return 1;
	}

	/* A simple 320x240 texture worked fine on a 6800GT under Linux,
	   but trying on a Mac with GeForce FX Go5200 failed, hence these
	   odd, but powers-of-two-dimensioned separate surfaces for screen
	   and border */
	screen_tex = SDL_CreateRGBSurface(SDL_SWSURFACE, 512, 256, 16, 0, 0, 0, 0);
	if (screen_tex == NULL) {
		LOG_ERROR("Failed to create surface for screen texture\n");
		return 1;
	}

	if (video_want_fullscreen)
		SDL_ShowCursor(SDL_DISABLE);
	else
		SDL_ShowCursor(SDL_ENABLE);

	config_opengl();
	xoffset = yoffset = 0;
	alloc_colours();
	/* Set preferred keyboard & joystick drivers */
	keyboard_module = &keyboard_sdl_module;
	joystick_module = &joystick_sdl_module;
	return 0;
}

static void shutdown(void) {
	LOG_DEBUG(2,"Shutting down SDL OpenGL driver\n");
	if (video_want_fullscreen)
		toggle_fullscreen();
	glDeleteTextures(1, &texnum);
	SDL_FreeSurface(screen_tex);
	/* Should not be freed by caller: SDL_FreeSurface(screen); */
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

static void toggle_fullscreen(void) {
	video_want_fullscreen = !video_want_fullscreen;
	resize(640, 480);
}

static void resize(uint_least16_t w, uint_least16_t h) {
	uint_least16_t width, height;
	if (w < 320) w = 320;
	if (h < 240) h = 240;
	if (((float)w/(float)h)>(4.0/3.0)) {
		width = ((float)h/3.0)*4;
		height = h;
		xoffset = (w - width) / 2;
		yoffset = 0;
	} else {
		width = w;
		height = ((float)w/4.0)*3;
		xoffset = 0;
		yoffset = (h - height)/2;
	}
	glDeleteTextures(1, &texnum);
	screen = SDL_SetVideoMode(w, h, 0, SDL_OPENGL|SDL_RESIZABLE|(video_want_fullscreen?SDL_FULLSCREEN:0));
	if (video_want_fullscreen)
		SDL_ShowCursor(SDL_DISABLE);
	else
		SDL_ShowCursor(SDL_ENABLE);
	config_opengl();
}

static void config_opengl(void) {
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

	glGenTextures(1, &texnum);
	glBindTexture(GL_TEXTURE_2D, texnum);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5, 512, 256, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

static void reset(void) {
	pixel = VIDEO_TOPLEFT + VIDEO_VIEWPORT_YOFFSET;
	subline = 0;
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

#endif  /* HAVE_SDLGL */
