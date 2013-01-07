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

/* OpenGL code is common to several video modules.  All the stuff that's not
 * toolkit-specific goes in here. */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>

#if defined(__APPLE_CC__)
# include <OpenGL/gl.h>
#else
# include <GL/gl.h>
#endif

#include "portalib/glib.h"

#ifdef WINDOWS32
#include "windows32/common_windows32.h"
#include <GL/glext.h>
#endif

#include "vdg.h"
#include "vo_opengl.h"
#include "xroar.h"

/*** ***/

/* Define stuff required for vo_generic_ops and include it */

typedef uint16_t Pixel;
#define MAPCOLOUR(r,g,b) ( (((r) & 0xf8) << 8) | (((g) & 0xfc) << 3) | (((b) & 0xf8) >> 3) )
#define VIDEO_SCREENBASE (screen_tex)
#define XSTEP 1
#define NEXTLINE 0
#define VIDEO_TOPLEFT VIDEO_SCREENBASE
#define VIDEO_VIEWPORT_YOFFSET (0)
#define LOCK_SURFACE
#define UNLOCK_SURFACE

static Pixel *screen_tex;

#include "vo_generic_ops.c"

/*** ***/

static unsigned window_width, window_height;
static GLuint texnum = 0;
static GLint xoffset, yoffset;

static enum {
	FILTER_AUTO,
	FILTER_NEAREST,
	FILTER_LINEAR,
} filter;

static const GLfloat tex_coords[][2] = {
	{ 0.0, 0.0 },
	{ 0.0, 0.9375 },
	{ 0.625, 0.0 },
	{ 0.625, 0.9375 },
};

static GLfloat vertices[][2] = {
	{ 0., 0. },
	{ 0., 0. },
	{ 0., 0. },
	{ 0., 0. }
};

int vo_opengl_init(void) {
	screen_tex = g_malloc(320 * 240 * sizeof(Pixel));
	window_width = 640;
	window_height = 480;
	xoffset = yoffset = 0;

	switch (xroar_opt_gl_filter) {
	case XROAR_GL_FILTER_NEAREST:
		filter = FILTER_NEAREST;
		break;
	case XROAR_GL_FILTER_LINEAR:
		filter = FILTER_LINEAR;
		break;
	default:
		filter = FILTER_AUTO;
		break;
	}

	alloc_colours();
	pixel = VIDEO_TOPLEFT + VIDEO_VIEWPORT_YOFFSET;
	return 0;
}

void vo_opengl_shutdown(void) {
	glDeleteTextures(1, &texnum);
	free(screen_tex);
}

void vo_opengl_alloc_colours(void) {
	alloc_colours();
}

void vo_opengl_set_window_size(unsigned w, unsigned h) {
	window_width = w;
	window_height = h;

	int width, height;

	if (((float)window_width/(float)window_height)>(4.0/3.0)) {
		height = window_height;
		width = ((float)height/3.0)*4;
		xoffset = (window_width - width) / 2;
		yoffset = 0;
	} else {
		width = window_width;
		height = ((float)width/4.0)*3;
		xoffset = 0;
		yoffset = (window_height - height)/2;
	}

	/* Configure OpenGL */
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_2D);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glViewport(0, 0, window_width, window_height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, window_width, window_height , 0, -1.0, 1.0);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClearDepth(1.0f);

	glDeleteTextures(1, &texnum);
	glGenTextures(1, &texnum);
	glBindTexture(GL_TEXTURE_2D, texnum);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5, 512, 256, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	if (filter == FILTER_NEAREST
	    || (filter == FILTER_AUTO && (width % 320) == 0 && (height % 240) == 0)) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	} else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	/* Is there a better way of clearing the texture? */
	memset(screen_tex, 0, 512 * sizeof(Pixel));
	glTexSubImage2D(GL_TEXTURE_2D, 0, 320,   0,   1, 256,
			GL_RGB, GL_UNSIGNED_SHORT_5_6_5, screen_tex);
	glTexSubImage2D(GL_TEXTURE_2D, 0,   0, 240, 512,   1,
			GL_RGB, GL_UNSIGNED_SHORT_5_6_5, screen_tex);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glColor4f(1.0, 1.0, 1.0, 1.0);

	vertices[0][0] = xoffset;
	vertices[0][1] = yoffset;
	vertices[1][0] = xoffset;
	vertices[1][1] = window_height - yoffset;
	vertices[2][0] = window_width - xoffset;
	vertices[2][1] = yoffset;
	vertices[3][0] = window_width - xoffset;
	vertices[3][1] = window_height - yoffset;

	/* The same vertex & texcoord lists will be used every draw,
	   so configure them here rather than in vsync() */
	glVertexPointer(2, GL_FLOAT, 0, vertices);
	glTexCoordPointer(2, GL_FLOAT, 0, tex_coords);
}

void vo_opengl_vsync(void) {
	/* Draw main window */
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
			320, 240, GL_RGB,
			GL_UNSIGNED_SHORT_5_6_5, screen_tex);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	pixel = VIDEO_TOPLEFT + VIDEO_VIEWPORT_YOFFSET;
	/* Video module should now do whatever's required to swap buffers */
}

void vo_opengl_render_scanline(uint8_t *scanline_data) {
	render_scanline(scanline_data);
}

void vo_opengl_update_cross_colour_phase(void) {
	update_cross_colour_phase();
}
