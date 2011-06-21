/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2011  Ciaran Anscomb
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gtk/gtkgl.h>
#include <GL/gl.h>

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
static void alloc_colours(void);
static void reset(void);
static void vsync(void);
static void hsync(void);
static void set_mode(unsigned int mode);
static void render_border(void);
static void resize(unsigned int w, unsigned int h);
static int set_fullscreen(int fullscreen);

VideoModule video_gtkgl_module = {
	.common = { .name = "gtkgl", .description = "GtkGLExt",
	            .init = init, .shutdown = shutdown },
	.update_palette = alloc_colours,
	.reset = reset, .vsync = vsync, .hsync = hsync, .set_mode = set_mode,
	.render_border = render_border,
	.resize = resize, .set_fullscreen = set_fullscreen
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
#define VIDEO_MODULE_NAME video_gtkgl_module

extern GtkWidget *gtk2_top_window;
extern GtkWidget *gtk2_menubar;
extern GtkWidget *gtk2_drawing_area;
static gboolean configure(GtkWidget *, GdkEventConfigure *, gpointer);

static Pixel *screen_tex;
static GLuint texnum = 0;
static GLint xoffset, yoffset;

enum {
	FILTER_AUTO,
	FILTER_NEAREST,
	FILTER_LINEAR,
} filter;

#include "vo_generic_ops.c"

static int init(void) {
	LOG_DEBUG(2, "Initialising GtkGLExt video module\n");

	gtk_gl_init(NULL, NULL);

	if (gdk_gl_query_extension() != TRUE) {
		LOG_ERROR("OpenGL not available\n");
		return 1;
	}

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

	screen_tex = malloc(320 * 240 * sizeof(Pixel));
	if (screen_tex == NULL) {
		LOG_ERROR("Failed to allocate memory for screen texture\n");
		return 1;
	}

	xoffset = yoffset = 0;

	/* Configure drawing_area widget */
	gtk_widget_set_size_request(gtk2_drawing_area, 640, 480);
	GdkGLConfig *glconfig = gdk_gl_config_new_by_mode(GDK_GL_MODE_RGB | GDK_GL_MODE_DOUBLE);
	if (!glconfig) {
		LOG_ERROR("Failed to create OpenGL config\n");
		return 1;
	}
	if (!gtk_widget_set_gl_capability(gtk2_drawing_area, glconfig, NULL, TRUE, GDK_GL_RGBA_TYPE)) {
		LOG_ERROR("Failed to add OpenGL support to GTK widget\n");
		return 1;
	}

	g_signal_connect(gtk2_drawing_area, "configure-event", G_CALLBACK(configure), NULL);

	/* Show top window first so that drawing area is realised to the
	 * right size even if we then fullscreen.  */
	gtk_widget_show(gtk2_top_window);
	/* Use helper so UI is updated. */
	xroar_fullscreen(xroar_opt_fullscreen);

	alloc_colours();
	reset();
	set_mode(0);

	return 0;
}

static void shutdown(void) {
	LOG_DEBUG(2,"Shutting down GtkGLExt video module\n");
	set_fullscreen(0);
	glDeleteTextures(1, &texnum);
	free(screen_tex);
}

static void resize(unsigned int w, unsigned int h) {
	if (video_gtkgl_module.is_fullscreen) {
		return;
	}
	if (w < 160 || h < 120) {
		return;
	}
	if (w > 1920 || h > 1440) {
		return;
	}
	/* You can't just set the widget size and expect GTK to adapt the
	 * containing window, * or indeed ask it to.  This will hopefully work
	 * consistently.  It seems to be basically how GIMP "shrink wrap"s its
	 * windows.  */
	gint oldw = gtk2_top_window->allocation.width;
	gint oldh = gtk2_top_window->allocation.height;
	gint woff = oldw - gtk2_drawing_area->allocation.width;
	gint hoff = oldh - gtk2_drawing_area->allocation.height;
	w += woff;
	h += hoff;
	gtk_window_resize(GTK_WINDOW(gtk2_top_window), w, h);
}

static int set_fullscreen(int fullscreen) {
	(void)fullscreen;
	if (fullscreen) {
		gtk_widget_hide(gtk2_menubar);
		gtk_window_fullscreen(GTK_WINDOW(gtk2_top_window));
	} else {
		gtk_window_unfullscreen(GTK_WINDOW(gtk2_top_window));
		gtk_widget_show(gtk2_menubar);
	}
	video_gtkgl_module.is_fullscreen = fullscreen;
	return 0;
}

static gboolean configure(GtkWidget *da, GdkEventConfigure *event, gpointer data) {
	(void)event;
	(void)data;
	unsigned int width, height;

	GdkGLContext *glcontext = gtk_widget_get_gl_context(da);
	GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(da);

	if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext)) {
		g_assert_not_reached();
	}

	if (((float)da->allocation.width/(float)da->allocation.height)>(4.0/3.0)) {
		height = da->allocation.height;
		video_gtkgl_module.scale = (float)height / 240.;
		width = ((float)height/3.0)*4;
		xoffset = (da->allocation.width - width) / 2;
		yoffset = 0;
	} else {
		width = da->allocation.width;
		video_gtkgl_module.scale = (float)width / 320.;
		height = ((float)width/4.0)*3;
		xoffset = 0;
		yoffset = (da->allocation.height - height)/2;
	}

	/* Configure OpenGL */
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_2D);

	glViewport(0, 0, da->allocation.width, da->allocation.height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, da->allocation.width, da->allocation.height , 0, -1.0, 1.0);
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

	gdk_gl_drawable_gl_end(gldrawable);

	return 0;
}

static void reset(void) {
	pixel = VIDEO_TOPLEFT + VIDEO_VIEWPORT_YOFFSET;
	subline = 0;
	beam_pos = 0;
}

static void vsync(void) {
	GdkGLContext *glcontext = gtk_widget_get_gl_context(gtk2_drawing_area);
	GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(gtk2_drawing_area);

	if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext)) {
		g_assert_not_reached ();
	}

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
	glVertex3i(xoffset, gtk2_drawing_area->allocation.height - yoffset, 0);
	glTexCoord2f(0.625, 0.9375);
	glVertex3i(gtk2_drawing_area->allocation.width - xoffset, gtk2_drawing_area->allocation.height - yoffset, 0);
	glTexCoord2f(0.625, 0.0);
	glVertex3i(gtk2_drawing_area->allocation.width - xoffset, yoffset, 0);
	glEnd();

	gdk_gl_drawable_swap_buffers(gldrawable);
	gdk_gl_drawable_gl_end(gldrawable);

	reset();
}
