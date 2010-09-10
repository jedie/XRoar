/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2010  Ciaran Anscomb
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

#include "types.h"
#include "logging.h"
#include "events.h"
#include "keyboard.h"
#include "m6809.h"
#include "machine.h"
#include "module.h"
#include "xroar.h"

static int init(void);
static void shutdown(void);
static void run(void);

extern VideoModule video_gtkgl_module;
static VideoModule *gtk2_video_module_list[] = {
#ifdef HAVE_GTKGL
	&video_gtkgl_module,
#endif
	NULL
};

extern KeyboardModule keyboard_gtk2_module;
static KeyboardModule *gtk2_keyboard_module_list[] = {
	&keyboard_gtk2_module,
	NULL
};

UIModule ui_gtk2_module = {
	.common = { .name = "gtk2", .description = "GTK+-2 user-interface",
	            .init = init, .shutdown = shutdown },
	.run = &run,
	.video_module_list = gtk2_video_module_list,
	.keyboard_module_list = gtk2_keyboard_module_list,
};

GtkWidget *gtk2_top_window = NULL;
static GtkWidget *vbox;
GtkUIManager *gtk2_menu_manager;
GtkWidget *gtk2_menubar;
GtkWidget *gtk2_drawing_area;
extern gboolean gtk2_translated_keymap;

static int run_cpu(void *data);

static void save_snapshot(void) {
	g_idle_remove_by_data(run_cpu);
	xroar_save_snapshot();
	g_idle_add(run_cpu, run_cpu);
}

static void set_machine(GtkRadioAction *action, GtkRadioAction *current, gpointer user_data) {
	gint val = gtk_radio_action_get_current_value(current);
	(void)action;
	(void)user_data;
	xroar_set_machine(val);
}

static void set_dos(GtkRadioAction *action, GtkRadioAction *current, gpointer user_data) {
	gint val = gtk_radio_action_get_current_value(current);
	(void)action;
	(void)user_data;
	xroar_set_dos(val);
}

static void set_fullscreen(GtkToggleAction *current, gpointer user_data) {
	gboolean val = gtk_toggle_action_get_active(current);
	(void)user_data;
	xroar_fullscreen(val);
}

static void set_keymap(GtkRadioAction *action, GtkRadioAction *current, gpointer user_data) {
	gint val = gtk_radio_action_get_current_value(current);
	(void)action;
	(void)user_data;
	xroar_set_keymap(val);
}

static void toggle_keyboard_translation(GtkToggleAction *current, gpointer user_data) {
	gboolean val = gtk_toggle_action_get_active(current);
	(void)user_data;
	gtk2_translated_keymap = val;
}

static void close_about(GtkDialog *dialog, gint response_id, gpointer data) {
	(void)response_id;
	(void)data;
	gtk_widget_hide(GTK_WIDGET(dialog));
	gtk_widget_destroy(GTK_WIDGET(dialog));
}

static void about(GtkMenuItem *item, gpointer data) {
	(void)item;
	(void)data;
	GtkAboutDialog *dialog = (GtkAboutDialog *)gtk_about_dialog_new();
	gtk_about_dialog_set_version(dialog, VERSION);
	gtk_about_dialog_set_copyright(dialog, "Copyright © 2003–2010 Ciaran Anscomb <xroar@6809.org.uk>");
	gtk_about_dialog_set_license(dialog,
"This program is free software; you can redistribute it and/or modify\n"
"it under the terms of the GNU General Public License as published by\n"
"the Free Software Foundation; either version 2 of the License, or\n"
"(at your option) any later version.\n"
"\n"
"This program is distributed in the hope that it will be useful,\n"
"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
"GNU General Public License for more details.\n"
"\n"
"You should have received a copy of the GNU General Public License\n"
"along with this program; if not, write to the Free Software\n"
"Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA"
	);
	gtk_about_dialog_set_website(dialog, "http://www.6809.org.uk/dragon/xroar.shtml");
	g_signal_connect(dialog, "response", G_CALLBACK(close_about), NULL);
	gtk_widget_show(GTK_WIDGET(dialog));
}

static const gchar *ui =
	"<ui>"
	  "<menubar name='MainMenu'>"
	    "<menu name='FileMenu' action='FileMenuAction'>"
	      "<menuitem name='Run' action='RunAction'/>"
	      "<menuitem name='Load' action='LoadAction'/>"
	      "<separator/>"
	      "<menuitem name='SaveSnapshot' action='SaveSnapshotAction'/>"
	      "<separator/>"
	      "<menuitem name='Quit' action='QuitAction'/>"
	    "</menu>"
	    "<menu name='ViewMenu' action='ViewMenuAction'>"
	      "<menuitem name='FullScreen' action='FullScreenAction'/>"
	    "</menu>"
	    "<menu name='MachineMenu' action='MachineMenuAction'>"
	      "<menuitem action='dragon32'/>"
	      "<menuitem action='dragon64'/>"
	      "<menuitem action='coco'/>"
	      "<menuitem action='cocous'/>"
	      "<menuitem action='tano'/>"
	      "<separator/>"
	      "<menuitem name='SoftReset' action='SoftResetAction'/>"
	      "<menuitem name='HardReset' action='HardResetAction'/>"
	    "</menu>"
	    "<menu name='DOSMenu' action='DOSMenuAction'>"
	      "<menuitem action='none'/>"
	      "<menuitem action='auto'/>"
	      "<menuitem action='dragondos'/>"
	      "<menuitem action='rsdos'/>"
	      "<menuitem action='delta'/>"
	    "</menu>"
	    "<menu name='KeyboardMenu' action='KeyboardMenuAction'>"
	      "<menuitem action='keymap_dragon'/>"
	      "<menuitem action='keymap_coco'/>"
	      "<separator/>"
	      "<menuitem name='TranslateKeyboard' action='TranslateKeyboardAction'/>"
	    "</menu>"
	    "<menu name='HelpMenu' action='HelpMenuAction'>"
	      "<menuitem name='About' action='AboutAction'/>"
	    "</menu>"
	  "</menubar>"
	"</ui>";

static GtkActionEntry ui_entries[] = {
	{ .name = "FileMenuAction", .label = "_File" },
	{ .name = "RunAction", .stock_id = GTK_STOCK_EXECUTE, .label = "_Run",
	  .accelerator = "<shift><control>L",
	  .tooltip = "Load and attempt to autorun a file",
	  .callback = G_CALLBACK(xroar_run_file) },
	{ .name = "LoadAction", .stock_id = GTK_STOCK_OPEN, .label = "_Load",
	  .accelerator = "<control>L",
	  .tooltip = "Load a file",
	  .callback = G_CALLBACK(xroar_load_file) },
	{ .name = "SaveSnapshotAction", .stock_id = GTK_STOCK_SAVE_AS, .label = "_Save Snapshot",
	  .accelerator = "<control>S",
	  .callback = G_CALLBACK(save_snapshot) },
	{ .name = "QuitAction", .stock_id = GTK_STOCK_QUIT, .label = "_Quit",
	  .accelerator = "<control>Q",
	  .tooltip = "Quit",
	  .callback = G_CALLBACK(xroar_quit) },
	{ .name = "ViewMenuAction", .label = "_View" },
	{ .name = "MachineMenuAction", .label = "_Machine" },
	{ .name = "SoftResetAction", .label = "_Soft Reset",
	  .accelerator = "<control>R",
	  .tooltip = "Soft Reset",
	  .callback = G_CALLBACK(xroar_soft_reset) },
	{ .name = "HardResetAction",
	  .label = "_Hard Reset",
	  .accelerator = "<shift><control>R",
	  .tooltip = "Hard Reset",
	  .callback = G_CALLBACK(xroar_hard_reset) },
	{ .name = "DOSMenuAction", .label = "_DOS" },
	{ .name = "KeyboardMenuAction", .label = "_Keyboard" },
	{ .name = "HelpMenuAction", .label = "_Help" },
	{ .name = "AboutAction", .stock_id = GTK_STOCK_ABOUT,
	  .label = "_About",
	  .callback = G_CALLBACK(about) },
};
static guint ui_n_entries = G_N_ELEMENTS(ui_entries);

static GtkToggleActionEntry ui_toggles[] = {
	{ .name = "FullScreenAction", .label = "_Full Screen",
	  .stock_id = GTK_STOCK_FULLSCREEN,
	  .accelerator = "F11", .callback = G_CALLBACK(set_fullscreen) },
	{ .name = "TranslateKeyboardAction", .label = "_Keyboard Translation",
	  .accelerator = "<control>Z",
	  .callback = G_CALLBACK(toggle_keyboard_translation) },
};
static guint ui_n_toggles = G_N_ELEMENTS(ui_toggles);

static GtkRadioActionEntry keymap_radio_entries[] = {
	{ .name = "keymap_dragon", .label = "Dragon Layout", .value = KEYMAP_DRAGON },
	{ .name = "keymap_coco", .label = "CoCo Layout", .value = KEYMAP_COCO },
};

static void machine_changed_cb(int machine_type) {
	GtkRadioAction *radio = (GtkRadioAction *)gtk_ui_manager_get_action(gtk2_menu_manager, "/MainMenu/MachineMenu/dragon32");
	gtk_radio_action_set_current_value(radio, machine_type);
}

static void dos_changed_cb(int dos_type) {
	GtkRadioAction *radio = (GtkRadioAction *)gtk_ui_manager_get_action(gtk2_menu_manager, "/MainMenu/DOSMenu/dragondos");
	gtk_radio_action_set_current_value(radio, dos_type);
}

static void fullscreen_changed_cb(int fullscreen) {
	GtkToggleAction *toggle = (GtkToggleAction *)gtk_ui_manager_get_action(gtk2_menu_manager, "/MainMenu/ViewMenu/FullScreen");
	gtk_toggle_action_set_active(toggle, fullscreen);
}

static void keymap_changed_cb(int keymap) {
	GtkRadioAction *radio = (GtkRadioAction *)gtk_ui_manager_get_action(gtk2_menu_manager, "/MainMenu/KeyboardMenu/keymap_dragon");
	gtk_radio_action_set_current_value(radio, keymap);
}

static int init(void) {

	LOG_DEBUG(2, "Initialising GTK+2 UI\n");

	gtk_init(NULL, NULL);

	g_set_application_name("XRoar");

	/* Create top level window */
	gtk2_top_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect_swapped(gtk2_top_window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

	/* Create vbox */
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(gtk2_top_window), vbox);

	/* Set up action group and parse menu XML */
	GtkActionGroup *action_group = gtk_action_group_new("Actions");
	gtk_action_group_set_translation_domain(action_group, "atd");
	gtk2_menu_manager = gtk_ui_manager_new();
	gtk_action_group_add_actions(action_group, ui_entries, ui_n_entries, NULL);
	gtk_action_group_add_toggle_actions(action_group, ui_toggles, ui_n_toggles, NULL);

	int i;
	GtkRadioActionEntry machine_radio_entries[NUM_MACHINE_TYPES];
	memset(machine_radio_entries, 0, sizeof(machine_radio_entries));
	for (i = 0; i < NUM_MACHINE_TYPES; i++) {
		machine_radio_entries[i].name = machine_options[i];
		machine_radio_entries[i].label = machine_names[i];
		machine_radio_entries[i].value = i;
	}
	gtk_action_group_add_radio_actions(action_group, machine_radio_entries, NUM_MACHINE_TYPES, requested_machine, (GCallback)set_machine, NULL);

	GtkRadioActionEntry dos_radio_entries[NUM_DOS_TYPES+1];
	memset(dos_radio_entries, 0, sizeof(dos_radio_entries));
	dos_radio_entries[0].name = "auto";
	dos_radio_entries[0].label = "Automatic";
	dos_radio_entries[0].value = ANY_AUTO;
	for (i = 0; i < NUM_DOS_TYPES; i++) {
		dos_radio_entries[i+1].name = dos_type_options[i];
		dos_radio_entries[i+1].label = dos_type_names[i];
		dos_radio_entries[i+1].value = i;
	}
	gtk_action_group_add_radio_actions(action_group, dos_radio_entries, NUM_DOS_TYPES+1, -1, (GCallback)set_dos, NULL);

	gtk_action_group_add_radio_actions(action_group, keymap_radio_entries, 2, 0, (GCallback)set_keymap, NULL);

	gtk_ui_manager_insert_action_group(gtk2_menu_manager, action_group, 0);
	GError *error = NULL;
	gtk_ui_manager_add_ui_from_string(gtk2_menu_manager, ui, -1, &error);
	if (error) {
		g_message("building menus failed: %s", error->message);
		g_error_free(error);
	}

	/* Extract menubar widget and add to vbox */
	gtk2_menubar = gtk_ui_manager_get_widget(gtk2_menu_manager, "/MainMenu");
	gtk_box_pack_start(GTK_BOX(vbox), gtk2_menubar, FALSE, FALSE, 0);
	gtk_window_add_accel_group(GTK_WINDOW(gtk2_top_window), gtk_ui_manager_get_accel_group(gtk2_menu_manager));

	/* Create drawing_area widget, add to vbox */
	gtk2_drawing_area = gtk_drawing_area_new();
	gtk_widget_set_size_request(gtk2_drawing_area, 640, 480);
	GdkGeometry hints = {
		.min_width = 320, .min_height = 240,
		.base_width = 0, .base_height = 0,
	};
	gtk_window_set_geometry_hints(GTK_WINDOW(gtk2_top_window), GTK_WIDGET(gtk2_drawing_area), &hints, GDK_HINT_MIN_SIZE | GDK_HINT_BASE_SIZE);
	gtk_box_pack_start(GTK_BOX(vbox), gtk2_drawing_area, TRUE, TRUE, 0);
	gtk_widget_show(gtk2_drawing_area);

	/* Now up to video module to do something with this drawing_area */

	xroar_machine_changed_cb = machine_changed_cb;
	xroar_dos_changed_cb = dos_changed_cb;
	xroar_fullscreen_changed_cb = fullscreen_changed_cb;
	xroar_keymap_changed_cb = keymap_changed_cb;

	return 0;
}

static void shutdown(void) {
}

static int run_cpu(void *data) {
	(void)data;
	m6809_run(456);
	while (EVENT_PENDING(UI_EVENT_LIST))
		DISPATCH_NEXT_EVENT(UI_EVENT_LIST);
	return 1;
}

static void run(void) {
	gtk_widget_show_all(gtk2_top_window);
	g_idle_add(run_cpu, run_cpu);
	gtk_main();
}
