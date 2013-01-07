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

#include "fs.h"
#include "logging.h"
#include "module.h"

static int init(void);
static char *load_filename(const char **extensions);
static char *save_filename(const char **extensions);

FileReqModule filereq_gtk2_module = {
	.common = { .name = "gtk2", .description = "GTK+-2 file requester",
	            .init = init },
	.load_filename = load_filename,
	.save_filename = save_filename
};

extern GtkWidget *gtk2_top_window;

static int init(void) {
	/* Only initialise if not running as part of general GTK+ interface */
	if (gtk2_top_window == NULL) {
		gtk_init(NULL, NULL);
	}
	return 0;
}

static GtkWidget *load_dialog = NULL;
static GtkWidget *save_dialog = NULL;
static gchar *filename = NULL;

static char *load_filename(const char **extensions) {
	(void)extensions;  /* unused */
	if (filename) {
		g_free(filename);
		filename = NULL;
	}
	if (!load_dialog) {
		load_dialog = gtk_file_chooser_dialog_new("Load file",
		    GTK_WINDOW(gtk2_top_window), GTK_FILE_CHOOSER_ACTION_OPEN,
		    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		    GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
	}
	if (gtk_dialog_run(GTK_DIALOG(load_dialog)) == GTK_RESPONSE_ACCEPT) {
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(load_dialog));
	}
	gtk_widget_hide(load_dialog);
	if (!gtk2_top_window) {
		while (gtk_events_pending()) {
			gtk_main_iteration();
		}
	}
	return filename;
}

static char *save_filename(const char **extensions) {
	(void)extensions;  /* unused */
	if (filename) {
		g_free(filename);
		filename = NULL;
	}
	if (!save_dialog) {
		save_dialog = gtk_file_chooser_dialog_new("Save file",
		    GTK_WINDOW(gtk2_top_window), GTK_FILE_CHOOSER_ACTION_SAVE,
		    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		    GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
		gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(save_dialog), TRUE);
	}
	if (gtk_dialog_run(GTK_DIALOG(save_dialog)) == GTK_RESPONSE_ACCEPT) {
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(save_dialog));
	}
	gtk_widget_hide(save_dialog);
	if (!gtk2_top_window) {
		while (gtk_events_pending()) {
			gtk_main_iteration();
		}
	}
	return filename;
}
