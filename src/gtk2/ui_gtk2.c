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

// For strsep()
#define _BSD_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <gtk/gtk.h>

#include "pl_string.h"

#include "cart.h"
#include "events.h"
#include "joystick.h"
#include "keyboard.h"
#include "logging.h"
#include "machine.h"
#include "mc6847.h"
#include "module.h"
#include "vdrive.h"
#include "xroar.h"

#include "gtk2/drivecontrol.h"
#include "gtk2/tapecontrol.h"
#include "gtk2/ui_gtk2.h"

#include "gtk2/top_window_glade.h"

static _Bool init(void);
static void shutdown(void);
static void run(void);

unsigned gtk2_window_x = 0, gtk2_window_y = 0;
unsigned gtk2_window_w = 640, gtk2_window_h = 480;

extern VideoModule video_gtkgl_module;
extern VideoModule video_null_module;
static VideoModule *gtk2_video_module_list[] = {
#ifdef HAVE_GTKGL
	&video_gtkgl_module,
#endif
	&video_null_module,
	NULL
};

extern KeyboardModule keyboard_gtk2_module;
static KeyboardModule *gtk2_keyboard_module_list[] = {
	&keyboard_gtk2_module,
	NULL
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static struct joystick_axis *configure_axis(char *, unsigned);
static struct joystick_button *configure_button(char *, unsigned);

static struct joystick_interface gtk2_js_if_mouse = {
	.name = "mouse",
	.configure_axis = configure_axis,
	.configure_button = configure_button,
};

static float mouse_xoffset = 34.0;
static float mouse_yoffset = 25.5;
static float mouse_xdiv = 252.;
static float mouse_ydiv = 189.;

static unsigned mouse_axis[2] = { 0, 0 };
static _Bool mouse_button[3] = { 0, 0, 0 };

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static struct joystick_interface *js_iflist[] = {
	&gtk2_js_if_keyboard,
	&gtk2_js_if_mouse,
	NULL
};

JoystickModule gtk2_js_internal = {
	.common = { .name = "gtk2", .description = "GTK+ joystick" },
	.interface_list = js_iflist,
};

static JoystickModule *gtk2_js_modlist[] = {
	&gtk2_js_internal,
	NULL
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

/* Module callbacks */
static void cross_colour_changed_cb(int cc);
static void vdg_inverse_cb(_Bool inverse);
static void machine_changed_cb(int machine_type);
static void cart_changed_cb(int cart_index);
static void keymap_changed_cb(int map);
static void fast_sound_changed_cb(_Bool fast);

UIModule ui_gtk2_module = {
	.common = { .name = "gtk2", .description = "GTK+-2 UI",
	            .init = init, .shutdown = shutdown },
	.run = &run,
	.video_module_list = gtk2_video_module_list,
	.keyboard_module_list = gtk2_keyboard_module_list,
	.joystick_module_list = gtk2_js_modlist,
	.cross_colour_changed_cb = cross_colour_changed_cb,
	.vdg_inverse_cb = vdg_inverse_cb,
	.machine_changed_cb = machine_changed_cb,
	.cart_changed_cb = cart_changed_cb,
	.keymap_changed_cb = keymap_changed_cb,
	.fast_sound_changed_cb = fast_sound_changed_cb,
	.input_tape_filename_cb = gtk2_input_tape_filename_cb,
	.output_tape_filename_cb = gtk2_output_tape_filename_cb,
	.update_tape_state = gtk2_update_tape_state,
	.update_drive_disk = gtk2_update_drive_disk,
	.update_drive_write_enable = gtk2_update_drive_write_enable,
	.update_drive_write_back = gtk2_update_drive_write_back,
};

GtkWidget *gtk2_top_window = NULL;
static GtkWidget *vbox;
GtkUIManager *gtk2_menu_manager;
GtkWidget *gtk2_menubar;
GtkWidget *gtk2_drawing_area;

/* Dynamic menus */
static GtkActionGroup *main_action_group;
static GtkActionGroup *machine_action_group;
static GtkActionGroup *cart_action_group;
static guint merge_machines, merge_carts;
static void update_machine_menu(void);
static void update_cartridge_menu(void);

/* for hiding cursor: */
static int cursor_hidden = 0;
static GdkCursor *old_cursor, *blank_cursor;
static gboolean hide_cursor(GtkWidget *widget, GdkEventMotion *event, gpointer data);
static gboolean show_cursor(GtkWidget *widget, GdkEventMotion *event, gpointer data);

/* UI-specific callbacks */
static void fullscreen_changed_cb(_Bool fullscreen);
static void kbd_translate_changed_cb(_Bool kbd_translate);

static gboolean run_cpu(gpointer data);

/* Helpers */
static char *escape_underscores(const char *str);

static void insert_disk(int drive) {
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

/* This is just stupid... */
static void insert_disk1(void) { gtk2_insert_disk(0); }
static void insert_disk2(void) { gtk2_insert_disk(1); }
static void insert_disk3(void) { gtk2_insert_disk(2); }
static void insert_disk4(void) { gtk2_insert_disk(3); }

static void save_snapshot(void) {
	g_idle_remove_by_data(gtk2_top_window);
	xroar_save_snapshot();
	g_idle_add(run_cpu, gtk2_top_window);
}

static void set_fullscreen(GtkToggleAction *current, gpointer user_data) {
	gboolean val = gtk_toggle_action_get_active(current);
	(void)user_data;
	xroar_fullscreen(val);
}

static void zoom_1_1(void) {
	if (video_module && video_module->resize) {
		video_module->resize(320, 240);
	}
}

static void zoom_2_1(void) {
	if (video_module && video_module->resize) {
		video_module->resize(640, 480);
	}
}

static void zoom_in(void) {
	if (video_module && video_module->resize) {
		int xscale = gtk2_window_w / 160;
		int yscale = gtk2_window_h / 120;
		int scale = 1;
		if (xscale < yscale)
			scale = yscale;
		else if (xscale > yscale)
			scale = xscale;
		else
			scale = xscale + 1;
		if (scale < 1)
			scale = 1;
		video_module->resize(160 * scale, 120 * scale);
	}
}

static void zoom_out(void) {
	if (video_module && video_module->resize) {
		int xscale = gtk2_window_w / 160;
		int yscale = gtk2_window_h / 120;
		int scale = 1;
		if (xscale < yscale)
			scale = xscale;
		else if (xscale > yscale)
			scale = yscale;
		else
			scale = xscale - 1;
		if (scale < 1)
			scale = 1;
		video_module->resize(160 * scale, 120 * scale);
	}
}

static void toggle_inverse_text(GtkToggleAction *current, gpointer user_data) {
	gboolean val = gtk_toggle_action_get_active(current);
	(void)user_data;
	xroar_set_vdg_inverted_text(1, val);
}

static void set_cc(GtkRadioAction *action, GtkRadioAction *current, gpointer user_data) {
	gint val = gtk_radio_action_get_current_value(current);
	(void)action;
	(void)user_data;
	xroar_set_cross_colour(val);
}

static void set_machine(GtkRadioAction *action, GtkRadioAction *current, gpointer user_data) {
	gint val = gtk_radio_action_get_current_value(current);
	(void)action;
	(void)user_data;
	xroar_set_machine(val);
}

static void set_cart(GtkRadioAction *action, GtkRadioAction *current, gpointer user_data) {
	gint val = gtk_radio_action_get_current_value(current);
	(void)action;
	(void)user_data;
	struct cart_config *cc = cart_config_index(val);
	xroar_set_cart(cc ? cc->name : NULL);
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
	xroar_set_kbd_translate(val);
}

static void toggle_fast_sound(GtkToggleAction *current, gpointer user_data) {
	gboolean val = gtk_toggle_action_get_active(current);
	(void)user_data;
	machine_set_fast_sound(val);
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
	gtk_about_dialog_set_copyright(dialog, "Copyright © 2003–2013 Ciaran Anscomb <xroar@6809.org.uk>");
	gtk_about_dialog_set_license(dialog,
"XRoar is free software: you can redistribute it and/or modify\n"
"it under the terms of the GNU General Public License as published by\n"
"the Free Software Foundation, either version 2 of the License, or\n"
"(at your option) any later version.\n"
"\n"
"XRoar is distributed in the hope that it will be useful,\n"
"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
"GNU General Public License for more details.\n"
"\n"
"You should have received a copy of the GNU General Public License\n"
"along with XRoar.  If not, see <http://www.gnu.org/licenses/>."
	);
	gtk_about_dialog_set_website(dialog, "http://www.6809.org.uk/dragon/xroar.shtml");
	g_signal_connect(dialog, "response", G_CALLBACK(close_about), NULL);
	gtk_widget_show(GTK_WIDGET(dialog));
}

static const gchar *ui =
	"<ui>"
	  "<accelerator name='InsertDisk1' action='InsertDisk1Action'/>"
	  "<accelerator name='InsertDisk2' action='InsertDisk2Action'/>"
	  "<accelerator name='InsertDisk3' action='InsertDisk3Action'/>"
	  "<accelerator name='InsertDisk4' action='InsertDisk4Action'/>"
	  "<menubar name='MainMenu'>"
	    "<menu name='FileMenu' action='FileMenuAction'>"
	      "<menuitem name='Run' action='RunAction'/>"
	      "<menuitem name='Load' action='LoadAction'/>"
	      "<separator/>"
	      "<menuitem name='InsertDisk' action='InsertDiskAction'/>"
	      "<separator/>"
	      "<menuitem name='SaveSnapshot' action='SaveSnapshotAction'/>"
	      "<separator/>"
	      "<menuitem name='Quit' action='QuitAction'/>"
	    "</menu>"
	    "<menu name='ViewMenu' action='ViewMenuAction'>"
	      "<menuitem name='InverseText' action='InverseTextAction'/>"
	      "<menu name='CrossColourMenu' action='CrossColourMenuAction'>"
	        "<menuitem action='cc-none'/>"
	        "<menuitem action='cc-blue-red'/>"
	        "<menuitem action='cc-red-blue'/>"
	      "</menu>"
	      "<separator/>"
	      "<menu name='ZoomMenu' action='ZoomMenuAction'>"
	        "<menuitem action='zoom_in'/>"
	        "<menuitem action='zoom_out'/>"
	        "<separator/>"
	        "<menuitem action='zoom_320x240'/>"
	        "<menuitem action='zoom_640x480'/>"
	        "<separator/>"
	        "<menuitem action='zoom_reset'/>"
	      "</menu>"
	      "<separator/>"
	      "<menuitem name='FullScreen' action='FullScreenAction'/>"
	    "</menu>"
	    "<menu name='MachineMenu' action='MachineMenuAction'>"
	      "<separator/>"
	      "<menu name='KeymapMenu' action='KeymapMenuAction'>"
	        "<menuitem action='keymap_dragon'/>"
	        "<menuitem action='keymap_dragon200e'/>"
	        "<menuitem action='keymap_coco'/>"
	      "</menu>"
	      "<separator/>"
	      "<menuitem name='SoftReset' action='SoftResetAction'/>"
	      "<menuitem name='HardReset' action='HardResetAction'/>"
	    "</menu>"
	    "<menu name='CartridgeMenu' action='CartridgeMenuAction'>"
	    "</menu>"
	    "<menu name='ToolMenu' action='ToolMenuAction'>"
	      "<menuitem name='TranslateKeyboard' action='TranslateKeyboardAction'/>"
	      "<menuitem name='DriveControl' action='DriveControlAction'/>"
	      "<menuitem name='TapeControl' action='TapeControlAction'/>"
	      "<menuitem name='FastSound' action='FastSoundAction'/>"
	    "</menu>"
	    "<menu name='HelpMenu' action='HelpMenuAction'>"
	      "<menuitem name='About' action='AboutAction'/>"
	    "</menu>"
	  "</menubar>"
	"</ui>";

static GtkActionEntry ui_entries[] = {
	/* Top level */
	{ .name = "FileMenuAction", .label = "_File" },
	{ .name = "ViewMenuAction", .label = "_View" },
	{ .name = "MachineMenuAction", .label = "_Machine" },
	{ .name = "CartridgeMenuAction", .label = "_Cartridge" },
	{ .name = "ToolMenuAction", .label = "_Tool" },
	{ .name = "HelpMenuAction", .label = "_Help" },
	/* File */
	{ .name = "RunAction", .stock_id = GTK_STOCK_EXECUTE, .label = "_Run",
	  .accelerator = "<shift><control>L",
	  .tooltip = "Load and attempt to autorun a file",
	  .callback = G_CALLBACK(xroar_run_file) },
	{ .name = "LoadAction", .stock_id = GTK_STOCK_OPEN, .label = "_Load",
	  .accelerator = "<control>L",
	  .tooltip = "Load a file",
	  .callback = G_CALLBACK(xroar_load_file) },
	{ .name = "InsertDiskAction",
	  .label = "Insert _Disk",
	  .tooltip = "Load a virtual disk image",
	  .callback = G_CALLBACK(insert_disk) },
	{ .name = "InsertDisk1Action", .accelerator = "<control>1", .callback = G_CALLBACK(insert_disk1) },
	{ .name = "InsertDisk2Action", .accelerator = "<control>2", .callback = G_CALLBACK(insert_disk2) },
	{ .name = "InsertDisk3Action", .accelerator = "<control>3", .callback = G_CALLBACK(insert_disk3) },
	{ .name = "InsertDisk4Action", .accelerator = "<control>4", .callback = G_CALLBACK(insert_disk4) },
	{ .name = "SaveSnapshotAction", .stock_id = GTK_STOCK_SAVE_AS, .label = "_Save Snapshot",
	  .accelerator = "<control>S",
	  .callback = G_CALLBACK(save_snapshot) },
	{ .name = "QuitAction", .stock_id = GTK_STOCK_QUIT, .label = "_Quit",
	  .accelerator = "<control>Q",
	  .tooltip = "Quit",
	  .callback = G_CALLBACK(xroar_quit) },
	/* View */
	{ .name = "CrossColourMenuAction", .label = "_Cross-colour" },
	{ .name = "ZoomMenuAction", .label = "_Zoom" },
	{ .name = "zoom_in", .label = "Zoom In",
	  .accelerator = "<control>plus",
	  .callback = G_CALLBACK(zoom_in) },
	{ .name = "zoom_out", .label = "Zoom Out",
	  .accelerator = "<control>minus",
	  .callback = G_CALLBACK(zoom_out) },
	{ .name = "zoom_320x240", .label = "320x240 (1:1)",
	  .callback = G_CALLBACK(zoom_1_1) },
	{ .name = "zoom_640x480", .label = "640x480 (2:1)",
	  .callback = G_CALLBACK(zoom_2_1) },
	{ .name = "zoom_reset", .label = "Reset",
	  .accelerator = "<control>0",
	  .callback = G_CALLBACK(zoom_2_1) },
	/* Machine */
	{ .name = "KeymapMenuAction", .label = "_Keyboard Map" },
	{ .name = "SoftResetAction", .label = "_Soft Reset",
	  .accelerator = "<control>R",
	  .tooltip = "Soft Reset",
	  .callback = G_CALLBACK(xroar_soft_reset) },
	{ .name = "HardResetAction",
	  .label = "_Hard Reset",
	  .accelerator = "<shift><control>R",
	  .tooltip = "Hard Reset",
	  .callback = G_CALLBACK(xroar_hard_reset) },
	/* Help */
	{ .name = "AboutAction", .stock_id = GTK_STOCK_ABOUT,
	  .label = "_About",
	  .callback = G_CALLBACK(about) },
};
static guint ui_n_entries = G_N_ELEMENTS(ui_entries);

static GtkToggleActionEntry ui_toggles[] = {
	/* View */
	{ .name = "InverseTextAction", .label = "_Inverse Text",
	  .accelerator = "<shift><control>I",
	  .callback = G_CALLBACK(toggle_inverse_text) },
	{ .name = "FullScreenAction", .label = "_Full Screen",
	  .stock_id = GTK_STOCK_FULLSCREEN,
	  .accelerator = "F11", .callback = G_CALLBACK(set_fullscreen) },
	/* Tool */
	{ .name = "TranslateKeyboardAction", .label = "_Keyboard Translation",
	  .accelerator = "<control>Z",
	  .callback = G_CALLBACK(toggle_keyboard_translation) },
	{ .name = "DriveControlAction", .label = "_Drive Control",
	  .accelerator = "<control>D",
	  .callback = G_CALLBACK(gtk2_toggle_dc_window) },
	{ .name = "TapeControlAction", .label = "_Tape Control",
	  .accelerator = "<control>T",
	  .callback = G_CALLBACK(gtk2_toggle_tc_window) },
	{ .name = "FastSoundAction", .label = "_Fast Sound",
	  .callback = G_CALLBACK(toggle_fast_sound) },
};
static guint ui_n_toggles = G_N_ELEMENTS(ui_toggles);

static GtkRadioActionEntry cross_colour_radio_entries[] = {
	{ .name = "cc-none", .label = "None", .value = CROSS_COLOUR_OFF },
	{ .name = "cc-blue-red", .label = "Blue-red", .value = CROSS_COLOUR_KBRW },
	{ .name = "cc-red-blue", .label = "Red-blue", .value = CROSS_COLOUR_KRBW },
};

static GtkRadioActionEntry keymap_radio_entries[] = {
	{ .name = "keymap_dragon", .label = "Dragon Layout", .value = KEYMAP_DRAGON },
	{ .name = "keymap_dragon200e", .label = "Dragon 200-E Layout", .value = KEYMAP_DRAGON200E },
	{ .name = "keymap_coco", .label = "CoCo Layout", .value = KEYMAP_COCO },
};

static _Bool init(void) {

	gtk_init(NULL, NULL);

	g_set_application_name("XRoar");

	GtkBuilder *builder;
	GError *error = NULL;
	builder = gtk_builder_new();
	if (!gtk_builder_add_from_string(builder, top_window_glade, -1, &error)) {
		g_warning("Couldn't create UI: %s", error->message);
		g_error_free(error);
		return 0;
	}

	/* Fetch top level window */
	gtk2_top_window = GTK_WIDGET(gtk_builder_get_object(builder, "top_window"));

	/* Fetch vbox */
	vbox = GTK_WIDGET(gtk_builder_get_object(builder, "vbox1"));

	/* Create a UI from XML */
	gtk2_menu_manager = gtk_ui_manager_new();

	gtk_ui_manager_add_ui_from_string(gtk2_menu_manager, ui, -1, &error);
	if (error) {
		g_message("building menus failed: %s", error->message);
		g_error_free(error);
	}

	/* Action groups */
	main_action_group = gtk_action_group_new("Main");
	machine_action_group = gtk_action_group_new("Machine");
	cart_action_group = gtk_action_group_new("Cartridge");
	gtk_ui_manager_insert_action_group(gtk2_menu_manager, main_action_group, 0);
	gtk_ui_manager_insert_action_group(gtk2_menu_manager, machine_action_group, 0);
	gtk_ui_manager_insert_action_group(gtk2_menu_manager, cart_action_group, 0);

	/* Set up main action group */
	gtk_action_group_add_actions(main_action_group, ui_entries, ui_n_entries, NULL);
	gtk_action_group_add_toggle_actions(main_action_group, ui_toggles, ui_n_toggles, NULL);
	gtk_action_group_add_radio_actions(main_action_group, keymap_radio_entries, 3, 0, (GCallback)set_keymap, NULL);
	gtk_action_group_add_radio_actions(main_action_group, cross_colour_radio_entries, 3, 0, (GCallback)set_cc, NULL);

	/* Menu merge points */
	merge_machines = gtk_ui_manager_new_merge_id(gtk2_menu_manager);
	merge_carts = gtk_ui_manager_new_merge_id(gtk2_menu_manager);

	/* Update all dynamic menus */
	update_machine_menu();
	update_cartridge_menu();

	/* Extract menubar widget and add to vbox */
	gtk2_menubar = gtk_ui_manager_get_widget(gtk2_menu_manager, "/MainMenu");
	gtk_box_pack_start(GTK_BOX(vbox), gtk2_menubar, FALSE, FALSE, 0);
	gtk_window_add_accel_group(GTK_WINDOW(gtk2_top_window), gtk_ui_manager_get_accel_group(gtk2_menu_manager));
	gtk_box_reorder_child(GTK_BOX(vbox), gtk2_menubar, 0);

	/* Create drawing_area widget, add to vbox */
	gtk2_drawing_area = GTK_WIDGET(gtk_builder_get_object(builder, "drawing_area"));
	GdkGeometry hints = {
		.min_width = 160, .min_height = 120,
		.base_width = 0, .base_height = 0,
	};
	gtk_window_set_geometry_hints(GTK_WINDOW(gtk2_top_window), GTK_WIDGET(gtk2_drawing_area), &hints, GDK_HINT_MIN_SIZE | GDK_HINT_BASE_SIZE);
	gtk_widget_show(gtk2_drawing_area);

	/* Parse initial geometry */
	if (xroar_cfg.geometry) {
		gtk_window_parse_geometry(GTK_WINDOW(gtk2_top_window), xroar_cfg.geometry);
	}

	/* Now up to video module to do something with this drawing_area */

	xroar_fullscreen_changed_cb = fullscreen_changed_cb;
	xroar_kbd_translate_changed_cb = kbd_translate_changed_cb;

	/* Cursor hiding */
	blank_cursor = gdk_cursor_new(GDK_BLANK_CURSOR);
	gtk_widget_add_events(gtk2_drawing_area, GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);
	g_signal_connect(G_OBJECT(gtk2_top_window), "key-press-event", G_CALLBACK(hide_cursor), NULL);
	g_signal_connect(G_OBJECT(gtk2_drawing_area), "motion-notify-event", G_CALLBACK(show_cursor), NULL);

	gtk_builder_connect_signals(builder, NULL);
	g_object_unref(builder);

	/* Create (hidden) drive control window */
	gtk2_create_dc_window();

	/* Create (hidden) tape control window */
	gtk2_create_tc_window();

	return 1;
}

static void shutdown(void) {
}

static gboolean run_cpu(gpointer data) {
	(void)data;
	return xroar_run();
}

static void run(void) {
	g_idle_add(run_cpu, gtk2_top_window);
	gtk_main();
}

static void remove_action_from_group(gpointer data, gpointer user_data) {
	GtkAction *action = data;
	GtkActionGroup *action_group = user_data;
	gtk_action_group_remove_action(action_group, action);
}

static void free_action_group(GtkActionGroup *action_group) {
	GList *list = gtk_action_group_list_actions(action_group);
	g_list_foreach(list, remove_action_from_group, action_group);
	g_list_free(list);
}

/* Dynamic machine menu */
static void update_machine_menu(void) {
	int num_machines = machine_config_count();
	int i;
	int selected = -1;
	free_action_group(machine_action_group);
	gtk_ui_manager_remove_ui(gtk2_menu_manager, merge_machines);
	if (xroar_machine_config) selected = xroar_machine_config->index;
	GtkRadioActionEntry *radio_entries = g_malloc0(num_machines * sizeof(*radio_entries));
	/* add these to the ui in reverse order, as each will be
	 * inserted before the previous */
	for (i = num_machines-1; i >= 0; i--) {
		struct machine_config *mc = machine_config_index(i);
		radio_entries[i].name = g_strconcat("machine-", mc->name, NULL);
		radio_entries[i].label = escape_underscores(mc->description);
		radio_entries[i].value = i;
		gtk_ui_manager_add_ui(gtk2_menu_manager, merge_machines, "/MainMenu/MachineMenu", radio_entries[i].name, radio_entries[i].name, GTK_UI_MANAGER_MENUITEM, TRUE);
	}
	gtk_action_group_add_radio_actions(machine_action_group, radio_entries, num_machines, selected, (GCallback)set_machine, NULL);
	for (i = 0; i < num_machines; i++) {
		g_free((gpointer)radio_entries[i].name);
		g_free((gpointer)radio_entries[i].label);
	}
	g_free(radio_entries);
}

/* Dynamic cartridge menu */
static void update_cartridge_menu(void) {
	int num_carts = cart_config_count();
	int i;
	int selected = 0;
	free_action_group(cart_action_group);
	gtk_ui_manager_remove_ui(gtk2_menu_manager, merge_carts);
	if (xroar_cart)
		selected = xroar_cart->config->index;
	GtkRadioActionEntry *radio_entries = g_malloc0((num_carts+1) * sizeof(*radio_entries));
	/* add these to the ui in reverse order, as each will be
	   inserted before the previous */
	for (i = num_carts-1; i >= 0; i--) {
		struct cart_config *cc = cart_config_index(i);
		radio_entries[i].name = g_strconcat("cart-", cc->name, NULL);
		radio_entries[i].label = escape_underscores(cc->description);
		radio_entries[i].value = i;
		gtk_ui_manager_add_ui(gtk2_menu_manager, merge_carts, "/MainMenu/CartridgeMenu", radio_entries[i].name, radio_entries[i].name, GTK_UI_MANAGER_MENUITEM, TRUE);
	}
	radio_entries[num_carts].name = "none";
	radio_entries[num_carts].label = "None";
	radio_entries[num_carts].value = -1;
	gtk_ui_manager_add_ui(gtk2_menu_manager, merge_carts, "/MainMenu/CartridgeMenu", radio_entries[num_carts].name, radio_entries[num_carts].name, GTK_UI_MANAGER_MENUITEM, TRUE);
	gtk_action_group_add_radio_actions(cart_action_group, radio_entries, num_carts+1, selected, (GCallback)set_cart, NULL);
	/* don't need to free last label */
	for (i = 0; i < num_carts; i++) {
		g_free((gpointer)radio_entries[i].name);
		g_free((gpointer)radio_entries[i].label);
	}
	g_free(radio_entries);
}

/* Cursor hiding */

static gboolean hide_cursor(GtkWidget *widget, GdkEventMotion *event, gpointer data) {
	(void)widget;
	(void)event;
	(void)data;
#ifndef WINDOWS32
	if (cursor_hidden)
		return FALSE;
	GdkWindow *window = gtk_widget_get_window(gtk2_drawing_area);
	old_cursor = gdk_window_get_cursor(window);
	gdk_window_set_cursor(window, blank_cursor);
	cursor_hidden = 1;
#endif
	return FALSE;
}

static gboolean show_cursor(GtkWidget *widget, GdkEventMotion *event, gpointer data) {
	(void)widget;
	(void)event;
	(void)data;
#ifndef WINDOWS32
	if (!cursor_hidden)
		return FALSE;
	GdkWindow *window = gtk_widget_get_window(gtk2_drawing_area);
	gdk_window_set_cursor(window, old_cursor);
	cursor_hidden = 0;
#endif
	return FALSE;
}

/* View callbacks */

static void fullscreen_changed_cb(_Bool fullscreen) {
	GtkToggleAction *toggle = (GtkToggleAction *)gtk_ui_manager_get_action(gtk2_menu_manager, "/MainMenu/ViewMenu/FullScreen");
	gtk_toggle_action_set_active(toggle, fullscreen ? TRUE : FALSE);
}

static void vdg_inverse_cb(_Bool inverse) {
	GtkToggleAction *toggle = (GtkToggleAction *)gtk_ui_manager_get_action(gtk2_menu_manager, "/MainMenu/ViewMenu/InverseText");
	gtk_toggle_action_set_active(toggle, inverse ? TRUE : FALSE);
}

static void cross_colour_changed_cb(int cc) {
	GtkRadioAction *radio = (GtkRadioAction *)gtk_ui_manager_get_action(gtk2_menu_manager, "/MainMenu/ViewMenu/CrossColourMenu/cc-none");
	gtk_radio_action_set_current_value(radio, cc);
}

/* Emulation callbacks */

static void machine_changed_cb(int machine_type) {
	GtkRadioAction *radio = (GtkRadioAction *)gtk_ui_manager_get_action(gtk2_menu_manager, "/MainMenu/MachineMenu/machine-dragon32");
	gtk_radio_action_set_current_value(radio, machine_type);
}

static void cart_changed_cb(int cart_index) {
	GtkRadioAction *radio = (GtkRadioAction *)gtk_ui_manager_get_action(gtk2_menu_manager, "/MainMenu/CartridgeMenu/cart-dragondos");
	gtk_radio_action_set_current_value(radio, cart_index);
}

static void keymap_changed_cb(int map) {
	GtkRadioAction *radio = (GtkRadioAction *)gtk_ui_manager_get_action(gtk2_menu_manager, "/MainMenu/MachineMenu/KeymapMenu/keymap_dragon");
	gtk_radio_action_set_current_value(radio, map);
}

/* Tool callbacks */

static void kbd_translate_changed_cb(_Bool kbd_translate) {
	GtkToggleAction *toggle = (GtkToggleAction *)gtk_ui_manager_get_action(gtk2_menu_manager, "/MainMenu/ToolMenu/TranslateKeyboard");
	gtk_toggle_action_set_active(toggle, kbd_translate ? TRUE : FALSE);
}

static void fast_sound_changed_cb(_Bool fast) {
	GtkToggleAction *toggle = (GtkToggleAction *)gtk_ui_manager_get_action(gtk2_menu_manager, "/MainMenu/ToolMenu/FastSound");
	gtk_toggle_action_set_active(toggle, fast ? TRUE : FALSE);
}

static char *escape_underscores(const char *str) {
	if (!str) return NULL;
	int len = strlen(str);
	const char *in;
	char *out;
	for (in = str; *in; in++) {
		if (*in == '_')
			len++;
	}
	char *ret_str = g_malloc(len + 1);
	for (in = str, out = ret_str; *in; in++) {
		*(out++) = *in;
		if (*in == '_') {
			*(out++) = '_';
		}
	}
	*out = 0;
	return ret_str;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static void update_mouse_state(void) {
	int x, y;
	GdkModifierType buttons;
	GdkWindow *window = gtk_widget_get_window(gtk2_drawing_area);
	gdk_window_get_pointer(window, &x, &y, &buttons);
	x = (x - gtk2_window_x) * 320;
	y = (y - gtk2_window_y) * 240;
	float xx = (float)x / (float)gtk2_window_w;
	float yy = (float)y / (float)gtk2_window_h;
	xx = (xx - mouse_xoffset) / mouse_xdiv;
	yy = (yy - mouse_yoffset) / mouse_ydiv;
	if (xx < 0.0) xx = 0.0;
	if (xx > 1.0) xx = 1.0;
	if (yy < 0.0) yy = 0.0;
	if (yy > 1.0) yy = 1.0;
	mouse_axis[0] = xx * 255.;
	mouse_axis[1] = yy * 255.;
	mouse_button[0] = buttons & GDK_BUTTON1_MASK;
	mouse_button[1] = buttons & GDK_BUTTON2_MASK;
	mouse_button[2] = buttons & GDK_BUTTON3_MASK;
}

static unsigned read_axis(unsigned *a) {
	update_mouse_state();
	return *a;
}

static _Bool read_button(_Bool *b) {
	update_mouse_state();
	return *b;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static struct joystick_axis *configure_axis(char *spec, unsigned jaxis) {
	jaxis %= 2;
	float off0 = (jaxis == 0) ? 2.0 : 1.5;
	float off1 = (jaxis == 0) ? 254.0 : 190.5;
	char *tmp = NULL;
	if (spec)
		tmp = strsep(&spec, ",");
	if (tmp && *tmp)
		off0 = strtof(tmp, NULL);
	if (spec && *spec)
		off1 = strtof(spec, NULL);
	if (jaxis == 0) {
		if (off0 < -32.0) off0 = -32.0;
		if (off1 > 288.0) off0 = 288.0;
		mouse_xoffset = off0 + 32.0;
		mouse_xdiv = off1 - off0;
	} else {
		if (off0 < -24.0) off0 = -24.0;
		if (off1 > 216.0) off0 = 216.0;
		mouse_yoffset = off0 + 24.0;
		mouse_ydiv = off1 - off0;
	}
	struct joystick_axis *axis = g_malloc(sizeof(*axis));
	axis->read = (js_read_axis_func)read_axis;
	axis->data = &mouse_axis[jaxis];
	return axis;
}

static struct joystick_button *configure_button(char *spec, unsigned jbutton) {
	jbutton %= 3;
	if (spec && *spec)
		jbutton = strtol(spec, NULL, 0) - 1;
	if (jbutton >= 3)
		return NULL;
	struct joystick_button *button = g_malloc(sizeof(*button));
	button->read = (js_read_button_func)read_button;
	button->data = &mouse_button[jbutton];
	return button;
}
