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
#include <gtk/gtk.h>
#include <gtk/gtkgl.h>

#include "logging.h"
#include "module.h"
#include "vdg.h"
#include "vo_opengl.h"
#include "xroar.h"

static int init(void);
static void _shutdown(void);
static void vsync(void);
static void resize(unsigned int w, unsigned int h);
static int set_fullscreen(_Bool fullscreen);

VideoModule video_gtkgl_module = {
	.common = { .name = "gtkgl", .description = "GtkGLExt video",
	            .init = init, .shutdown = _shutdown },
	.update_palette = vo_opengl_alloc_colours,
	.vsync = vsync,
	.render_scanline = vo_opengl_render_scanline,
	.resize = resize, .set_fullscreen = set_fullscreen,
	.update_cross_colour_phase = vo_opengl_update_cross_colour_phase,
};


extern GtkWidget *gtk2_top_window;
extern GtkWidget *gtk2_menubar;
extern GtkWidget *gtk2_drawing_area;
static gboolean configure(GtkWidget *, GdkEventConfigure *, gpointer);

static int init(void) {
	gtk_gl_init(NULL, NULL);

	if (gdk_gl_query_extension() != TRUE) {
		LOG_ERROR("OpenGL not available\n");
		return 1;
	}
	vo_opengl_init();

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
	/* Set fullscreen and update UI. */
	set_fullscreen(xroar_opt_fullscreen);
	if (xroar_fullscreen_changed_cb) {
		xroar_fullscreen_changed_cb(xroar_opt_fullscreen);
	}

	vsync();

	return 0;
}

static void _shutdown(void) {
	set_fullscreen(0);
	vo_opengl_shutdown();
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

static int set_fullscreen(_Bool fullscreen) {
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

	GdkGLContext *glcontext = gtk_widget_get_gl_context(da);
	GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(da);

	if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext)) {
		g_assert_not_reached();
	}

	vo_opengl_set_window_size(da->allocation.width, da->allocation.height);

	gdk_gl_drawable_gl_end(gldrawable);

	return 0;
}

static void vsync(void) {
	GdkGLContext *glcontext = gtk_widget_get_gl_context(gtk2_drawing_area);
	GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(gtk2_drawing_area);

	if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext)) {
		g_assert_not_reached ();
	}

	vo_opengl_vsync();

	gdk_gl_drawable_swap_buffers(gldrawable);
	gdk_gl_drawable_gl_end(gldrawable);
}
