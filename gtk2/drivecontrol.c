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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

#include "gtk2/drivecontrol.h"
#include "gtk2/ui_gtk2.h"
#include "vdisk.h"
#include "vdrive.h"
#include "xroar.h"

#include "gtk2/drivecontrol_glade.h"

/* Module callbacks */
static void update_drive_cyl_head(int drive, int cyl, int head);

/* Drive control widgets */
static GtkWidget *dc_window = NULL;
static GtkWidget *dc_filename_drive[4] = { NULL, NULL, NULL, NULL };
static GtkToggleButton *dc_we_drive[4] = { NULL, NULL, NULL, NULL };
static GtkToggleButton *dc_wb_drive[4] = { NULL, NULL, NULL, NULL };
static GtkWidget *dc_drive_cyl_head = NULL;

static void hide_dc_window(void);
static void dc_insert(GtkButton *button, gpointer user_data);
static void dc_eject(GtkButton *button, gpointer user_data);

void gtk2_insert_disk(int drive) {
	static GtkFileChooser *file_dialog = NULL;
	static GtkComboBox *drive_combo = NULL;
	if (!file_dialog) {
		file_dialog = GTK_FILE_CHOOSER(
		    gtk_file_chooser_dialog_new("Insert Disk",
			GTK_WINDOW(gtk2_top_window),
			GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL,
			GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN,
			GTK_RESPONSE_ACCEPT, NULL));
	}
	if (!drive_combo) {
		drive_combo = GTK_COMBO_BOX(gtk_combo_box_new_text());
		gtk_combo_box_append_text(drive_combo, "Drive 1");
		gtk_combo_box_append_text(drive_combo, "Drive 2");
		gtk_combo_box_append_text(drive_combo, "Drive 3");
		gtk_combo_box_append_text(drive_combo, "Drive 4");
		gtk_file_chooser_set_extra_widget(file_dialog, GTK_WIDGET(drive_combo));
	}
	if (drive < 0 || drive > 3) drive = 0;
	gtk_combo_box_set_active(GTK_COMBO_BOX(drive_combo), drive);
	if (gtk_dialog_run(GTK_DIALOG(file_dialog)) == GTK_RESPONSE_ACCEPT) {
		char *filename = gtk_file_chooser_get_filename(file_dialog);
		drive = gtk_combo_box_get_active(GTK_COMBO_BOX(drive_combo));
		if (drive < 0 || drive > 3) drive = 0;
		if (filename) {
			xroar_insert_disk_file(drive, filename);
			g_free(filename);
		}
	}
	gtk_widget_hide(GTK_WIDGET(file_dialog));
}

static void dc_toggled_we(GtkToggleButton *togglebutton, gpointer user_data);
static void dc_toggled_wb(GtkToggleButton *togglebutton, gpointer user_data);

void gtk2_create_dc_window(void) {
	GtkBuilder *builder;
	GtkWidget *widget;
	GError *error = NULL;
	int i;
	builder = gtk_builder_new();
	if (!gtk_builder_add_from_string(builder, drivecontrol_glade, -1, &error)) {
		g_warning("Couldn't create UI: %s", error->message);
		g_error_free(error);
		return;
	}

	/* Extract UI elements modified elsewhere */
	dc_window = GTK_WIDGET(gtk_builder_get_object(builder, "dc_window"));
	dc_filename_drive[0] = GTK_WIDGET(gtk_builder_get_object(builder, "filename_drive1"));
	dc_filename_drive[1] = GTK_WIDGET(gtk_builder_get_object(builder, "filename_drive2"));
	dc_filename_drive[2] = GTK_WIDGET(gtk_builder_get_object(builder, "filename_drive3"));
	dc_filename_drive[3] = GTK_WIDGET(gtk_builder_get_object(builder, "filename_drive4"));
	dc_we_drive[0] = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "we_drive1"));
	dc_we_drive[1] = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "we_drive2"));
	dc_we_drive[2] = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "we_drive3"));
	dc_we_drive[3] = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "we_drive4"));
	dc_wb_drive[0] = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "wb_drive1"));
	dc_wb_drive[1] = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "wb_drive2"));
	dc_wb_drive[2] = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "wb_drive3"));
	dc_wb_drive[3] = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "wb_drive4"));
	dc_drive_cyl_head = GTK_WIDGET(gtk_builder_get_object(builder, "drive_cyl_head"));

	/* Connect signals */
	for (i = 0; i < 4; i++) {
		g_signal_connect(dc_we_drive[i], "toggled", G_CALLBACK(dc_toggled_we), (char *)0 + i);
		g_signal_connect(dc_wb_drive[i], "toggled", G_CALLBACK(dc_toggled_wb), (char *)0 + i);
	}
	g_signal_connect(dc_window, "delete-event", G_CALLBACK(hide_dc_window), NULL);
	widget = GTK_WIDGET(gtk_builder_get_object(builder, "eject_drive1"));
	g_signal_connect(widget, "clicked", G_CALLBACK(dc_eject), (char *)0);
	widget = GTK_WIDGET(gtk_builder_get_object(builder, "eject_drive2"));
	g_signal_connect(widget, "clicked", G_CALLBACK(dc_eject), (char *)0 + 1);
	widget = GTK_WIDGET(gtk_builder_get_object(builder, "eject_drive3"));
	g_signal_connect(widget, "clicked", G_CALLBACK(dc_eject), (char *)0 + 2);
	widget = GTK_WIDGET(gtk_builder_get_object(builder, "eject_drive4"));
	g_signal_connect(widget, "clicked", G_CALLBACK(dc_eject), (char *)0 + 3);
	widget = GTK_WIDGET(gtk_builder_get_object(builder, "insert_drive1"));
	g_signal_connect(widget, "clicked", G_CALLBACK(dc_insert), (char *)0 + 0);
	widget = GTK_WIDGET(gtk_builder_get_object(builder, "insert_drive2"));
	g_signal_connect(widget, "clicked", G_CALLBACK(dc_insert), (char *)0 + 1);
	widget = GTK_WIDGET(gtk_builder_get_object(builder, "insert_drive3"));
	g_signal_connect(widget, "clicked", G_CALLBACK(dc_insert), (char *)0 + 2);
	widget = GTK_WIDGET(gtk_builder_get_object(builder, "insert_drive4"));
	g_signal_connect(widget, "clicked", G_CALLBACK(dc_insert), (char *)0 + 3);

	/* In case any signals remain... */
	gtk_builder_connect_signals(builder, NULL);
	g_object_unref(builder);

	vdrive_update_drive_cyl_head = update_drive_cyl_head;
}

/* Drive Control - Signal Handlers */

void gtk2_toggle_dc_window(GtkToggleAction *current, gpointer user_data) {
	gboolean val = gtk_toggle_action_get_active(current);
	(void)user_data;
	if (val) {
		gtk_widget_show(dc_window);
	} else {
		gtk_widget_hide(dc_window);
	}
}

static void hide_dc_window(void) {
	GtkToggleAction *toggle = (GtkToggleAction *)gtk_ui_manager_get_action(gtk2_menu_manager, "/MainMenu/ToolMenu/DriveControl");
	gtk_toggle_action_set_active(toggle, 0);
}

static void dc_insert(GtkButton *button, gpointer user_data) {
	int drive = (char *)user_data - (char *)0;
	(void)button;
	xroar_insert_disk(drive);
}

static void dc_eject(GtkButton *button, gpointer user_data) {
	int drive = (char *)user_data - (char *)0;
	(void)button;
	xroar_eject_disk(drive);
}

static void dc_toggled_we(GtkToggleButton *togglebutton, gpointer user_data) {
	int set = gtk_toggle_button_get_active(togglebutton) ? 1 : 0;
	int drive = (char *)user_data - (char *)0;
	xroar_set_write_enable(drive, set);
}

static void dc_toggled_wb(GtkToggleButton *togglebutton, gpointer user_data) {
	int set = gtk_toggle_button_get_active(togglebutton) ? 1 : 0;
	int drive = (char *)user_data - (char *)0;
	xroar_set_write_back(drive, set);
}

/* Drive Control - UI callbacks */

void gtk2_update_drive_write_enable(int drive, _Bool write_enable) {
	if (drive >= 0 && drive <= 3) {
		gtk_toggle_button_set_active(dc_we_drive[drive], write_enable ? TRUE : FALSE);
	}
}

void gtk2_update_drive_write_back(int drive, _Bool write_back) {
	if (drive >= 0 && drive <= 3) {
		gtk_toggle_button_set_active(dc_wb_drive[drive], write_back ? TRUE : FALSE);
	}
}

void gtk2_update_drive_disk(int drive, struct vdisk *disk) {
	if (drive < 0 || drive > 3)
		return;
	char *filename = NULL;
	_Bool we = 1, wb = 0;
	if (disk) {
		filename = disk->filename;
		we = !disk->write_protect;
		wb = !disk->file_write_protect;
	}
	gtk_label_set_text(GTK_LABEL(dc_filename_drive[drive]), filename);
	gtk2_update_drive_write_enable(drive, we);
	gtk2_update_drive_write_back(drive, wb);
}

static void update_drive_cyl_head(int drive, int cyl, int head) {
	char string[16];
	snprintf(string, sizeof(string), "Dr %01d Tr %02d He %01d", drive + 1, cyl, head);
	gtk_label_set_text(GTK_LABEL(dc_drive_cyl_head), string);
}
