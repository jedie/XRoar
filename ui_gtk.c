/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2004  Ciaran Anscomb
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

/* This GTK code is probably all wrong, but it does seem to work */

#include "config.h"

#ifdef HAVE_GTK_UI

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include "ui.h"
#include "fs.h"
#include "types.h"
#include "logging.h"

static int init(void);
static void shutdown(void);
static void menu(void);
static char *get_filename(char **extensions);

UIModule ui_gtk_module = {
	NULL,
	"gtk",
	"GTK+ user-interface",
	init, shutdown,
	menu, get_filename
};

static char *filename = NULL;

static int init(void) {
	LOG_DEBUG(2, "Initialising GTK+ user-interface\n");
	gtk_init(NULL, NULL);
	return 0;
}

static void shutdown(void) {
	LOG_DEBUG(2, "Shutting down GTK+ user-interface\n");
	LOG_DEBUG(3, "\tNothing to do...\n");
}

static gboolean cancel(GtkWidget *w) {
	gtk_main_quit();
	return FALSE;
}

static void file_selected(GtkWidget *w, GtkFileSelection *fs) {
	char *fn = gtk_file_selection_get_filename(GTK_FILE_SELECTION(w));
	if (filename)
		free(filename);
	filename = (char *)malloc(strlen(fn)+1);
	strcpy(filename, fn);
	gtk_widget_destroy(w);
}

static void menu(void) {
}

static char *get_filename(char **extensions) {
	GtkWidget *fs = gtk_file_selection_new("Select file");
	if (filename)
		free(filename);
	filename = NULL;
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
	return filename;
}

#endif  /* HAVE_GTK_UI */
