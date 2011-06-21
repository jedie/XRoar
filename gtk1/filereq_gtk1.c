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

/* The GTK+ function prototypes shadow 'index' from string.h in many places,
 * so expect lots of compiler warnings about that */

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

#include "types.h"
#include "logging.h"
#include "fs.h"
#include "module.h"

static int init(void);
static void shutdown(void);
static char *load_filename(const char **extensions);
static char *save_filename(const char **extensions);

FileReqModule filereq_gtk1_module = {
	{ "gtk1", "GTK+-1 file requester",
	  init, 0, shutdown },
	load_filename, save_filename
};

static char *filename = NULL;

static int init(void) {
	LOG_DEBUG(2, "GTK+-1 file requester selected.\n");
	gtk_init(NULL, NULL);
	return 0;
}

static void shutdown(void) {
}

static gboolean cancel(GtkWidget *w) {
	(void)w;  /* unused */
	gtk_main_quit();
	return FALSE;
}

static void file_selected(GtkWidget *w, GtkFileSelection *fs) {
	const char *fn = gtk_file_selection_get_filename(GTK_FILE_SELECTION(w));
	(void)fs;  /* unused */
	if (filename)
		free(filename);
	filename = strdup(fn);
	gtk_widget_destroy(w);
}

static char *load_filename(const char **extensions) {
	GtkWidget *fs;
	int was_fullscreen;
	(void)extensions;  /* unused */

	was_fullscreen = video_module->is_fullscreen;
	if (video_module->set_fullscreen && was_fullscreen)
		video_module->set_fullscreen(0);
	if (filename)
		free(filename);
	filename = NULL;
	fs = gtk_file_selection_new("Load file");
	gtk_signal_connect(GTK_OBJECT(fs), "destroy",
			GTK_SIGNAL_FUNC(cancel), NULL);
	gtk_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(fs)->ok_button),
			"clicked",GTK_SIGNAL_FUNC(file_selected),
			GTK_OBJECT (fs));
	gtk_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(fs)->cancel_button),
			"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),
			GTK_OBJECT (fs));
	gtk_widget_show(fs);
	gtk_main();
	if (video_module->set_fullscreen && was_fullscreen)
		video_module->set_fullscreen(1);
	return filename;
}

static char *save_filename(const char **extensions) {
	GtkWidget *fs;
	int was_fullscreen;
	(void)extensions;  /* unused */

	was_fullscreen = video_module->is_fullscreen;
	if (video_module->set_fullscreen && was_fullscreen)
		video_module->set_fullscreen(0);
	if (filename)
		free(filename);
	filename = NULL;
	fs = gtk_file_selection_new("Save file");
	gtk_signal_connect(GTK_OBJECT(fs), "destroy",
			GTK_SIGNAL_FUNC(cancel), NULL);
	gtk_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(fs)->ok_button),
			"clicked",GTK_SIGNAL_FUNC(file_selected),
			GTK_OBJECT (fs));
	gtk_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(fs)->cancel_button),
			"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),
			GTK_OBJECT (fs));
	gtk_widget_show(fs);
	gtk_main();
	if (video_module->set_fullscreen && was_fullscreen)
		video_module->set_fullscreen(1);
	return filename;
}
