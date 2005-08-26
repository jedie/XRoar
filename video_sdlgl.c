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
#include "video.h"

static int init(void);
static void shutdown(void);
static void fillrect(uint_least16_t x, uint_least16_t y,
		uint_least16_t w, uint_least16_t h, uint32_t colour);
static void blit(uint_least16_t x, uint_least16_t y, Sprite *src);
static void resize(uint_least16_t w, uint_least16_t h);
static void vdg_reset(void);
static void vdg_vsync(void);
static void vdg_set_mode(unsigned int mode);
static void render_sg4(void);
/* static void render_sg6(void); */
static void render_cg1(void);
static void render_rg1(void);
static void render_cg2(void);
static void render_rg6(void);
static void render_border(void);

VideoModule video_sdlgl_module = {
	NULL,
	"sdlgl",
	"SDL OpenGL",
	init, shutdown,
	fillrect, blit,
	NULL, NULL, resize,
	vdg_reset, vdg_vsync, vdg_set_mode,
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
#define BORDER_XSTEP 0
#define NEXTLINE 0
#define VIDEO_TOPLEFT (VIDEO_SCREENBASE)
#define VIDEO_VIEWPORT_YOFFSET (0)
#define LOCK_SURFACE SDL_LockSurface(screen_tex)
#define UNLOCK_SURFACE SDL_UnlockSurface(screen_tex)
#define SEPARATE_BORDER
#define LOCK_BORDER SDL_LockSurface(border_tex)
#define UNLOCK_BORDER SDL_UnlockSurface(border_tex)
#define TOP_BOTTOM_BORDER_PIXELS 256
extern unsigned int vdg_alpha[768];

static unsigned int subline;
static Pixel *pixel;
static Pixel *bpixel;
static Pixel darkgreen, black;
static Pixel bg_colour;
static Pixel fg_colour;
static Pixel vdg_colour[16];
static Pixel *cg_colours;
static Pixel border_colour;

static SDL_Surface *screen;
static SDL_Surface *screen_tex;
static SDL_Surface *border_tex;
static GLuint texnum;
static GLuint border_texnum;
static GLint xoffset, yoffset;

static void config_opengl(void);

static int init(void) {
	LOG_DEBUG(2,"Initialising SDL OpenGL driver\n");
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
	screen = SDL_SetVideoMode(640, 480, 0, SDL_OPENGL|SDL_RESIZABLE);
	if (screen == NULL) {
		LOG_ERROR("Failed to initialise display\n");
		return 1;
	}

	/* A simple 320x240 texture worked fine on a 6800GT under Linux,
	   but trying on a Mac with GeForce FX Go5200 failed, hence these
	   odd, but powers-of-two-dimensioned separate surfaces for screen
	   and border */
	screen_tex = SDL_CreateRGBSurface(SDL_SWSURFACE, 256, 256, 16, 0, 0, 0, 0);
	if (screen_tex == NULL) {
		LOG_ERROR("Failed to create surface for screen texture\n");
		return 1;
	}
	border_tex = SDL_CreateRGBSurface(SDL_SWSURFACE, 2, 256, 16, 0, 0, 0, 0);
	if (border_tex == NULL) {
		LOG_ERROR("Failed to create surface for border texture\n");
		return 1;
	}

	config_opengl();
	xoffset = 64;
	yoffset = 0;

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
	/* Set preferred keyboard & joystick drivers */
	keyboard_module = &keyboard_sdl_module;
	joystick_module = &joystick_sdl_module;
	return 0;
}

static void shutdown(void) {
	LOG_DEBUG(2,"Shutting down SDL OpenGL driver\n");
	glDeleteTextures(1, &texnum);
	glDeleteTextures(1, &border_texnum);
	SDL_FreeSurface(screen_tex);
	SDL_FreeSurface(border_tex);
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
	uint_least16_t width, height;
	if (w < 320) w = 320;
	if (h < 240) h = 240;
	if (((float)w/(float)h)>(4.0/3.0)) {
		width = ((float)h/3.0)*4;
		height = h;
		xoffset = ((w - width) / 2) + (width / 10);
		yoffset = 0;
	} else {
		width = w;
		height = ((float)w/4.0)*3;
		xoffset = width / 10;
		yoffset = (h - height)/2;
	}
	glDeleteTextures(1, &texnum);
	glDeleteTextures(1, &border_texnum);
	screen = SDL_SetVideoMode(w, h, 0, SDL_OPENGL|SDL_RESIZABLE);
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
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5, 256, 256, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glGenTextures(1, &border_texnum);
	glBindTexture(GL_TEXTURE_2D, border_texnum);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5, 2, 256, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

static void vdg_reset(void) {
	pixel = VIDEO_TOPLEFT + VIDEO_VIEWPORT_YOFFSET;
	bpixel = border_tex->pixels;
	subline = 0;
}

static void vdg_vsync(void) {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glColor4f(1.0, 1.0, 1.0, 1.0);
	/* Draw border */
	glBindTexture(GL_TEXTURE_2D, border_texnum);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
			2, 256, GL_RGB,
			GL_UNSIGNED_SHORT_5_6_5, border_tex->pixels);
	glBegin(GL_QUADS);
	glTexCoord2f(0.0, 0.0);
	glVertex3i(0, yoffset, 0);
	glTexCoord2f(0.0, 0.9375);
	glVertex3i(0, screen->h - yoffset, 0);
	glTexCoord2f(1.0, 0.9375);
	glVertex3i(screen->w, screen->h - yoffset, 0);
	glTexCoord2f(1.0, 0.0);
	glVertex3i(screen->w, yoffset, 0);
	glEnd();
	/* Draw main window */
	glBindTexture(GL_TEXTURE_2D, texnum);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
			256, 256, GL_RGB,
			GL_UNSIGNED_SHORT_5_6_5, screen_tex->pixels);
	glBegin(GL_QUADS);
	glTexCoord2f(0.0, 0.0);
	glVertex3i(xoffset, yoffset, 0);
	glTexCoord2f(0.0, 0.9375);
	glVertex3i(xoffset, screen->h - yoffset, 0);
	glTexCoord2f(1.0, 0.9375);
	glVertex3i(screen->w - xoffset, screen->h - yoffset, 0);
	glTexCoord2f(1.0, 0.0);
	glVertex3i(screen->w - xoffset, yoffset, 0);
	glEnd();
	SDL_GL_SwapBuffers();
	pixel = VIDEO_TOPLEFT + VIDEO_VIEWPORT_YOFFSET;
	bpixel = border_tex->pixels;
	subline = 0;
}

static void vdg_set_mode(unsigned int mode) {
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

#endif  /* HAVE_SDLGL */
