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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

#include "types.h"
#include "logging.h"
#include "cart.h"
#include "fs.h"
#include "keyboard.h"
#include "machine.h"
#include "module.h"
#include "sam.h"
#include "tape.h"
#include "vdg.h"
#include "vdrive.h"
#include "xroar.h"

#include "gtk2/top_window_glade.h"
#include "gtk2/tapecontrol_glade.h"
#include "gtk2/drivecontrol_glade.h"

static int init(void);
static void shutdown(void);
static void run(void);

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

/* Module callbacks */
static void cross_colour_changed_cb(int cc);
static void machine_changed_cb(int machine_type);
static void cart_changed_cb(int cart_index);
static void keymap_changed_cb(int keymap);
static void fast_sound_changed_cb(int fast);
static void input_tape_filename_cb(const char *filename);
static void output_tape_filename_cb(const char *filename);
static void update_tape_state(int flags);
static void update_drive_disk(int drive, struct vdisk *disk);
static void update_drive_write_enable(int drive, int write_enable);
static void update_drive_write_back(int drive, int write_back);
static void update_drive_cyl_head(int drive, int cyl, int head);

UIModule ui_gtk2_module = {
	.common = { .name = "gtk2", .description = "GTK+-2 user-interface",
	            .init = init, .shutdown = shutdown },
	.run = &run,
	.video_module_list = gtk2_video_module_list,
	.keyboard_module_list = gtk2_keyboard_module_list,
	.cross_colour_changed_cb = cross_colour_changed_cb,
	.machine_changed_cb = machine_changed_cb,
	.cart_changed_cb = cart_changed_cb,
	.keymap_changed_cb = keymap_changed_cb,
	.fast_sound_changed_cb = fast_sound_changed_cb,
	.input_tape_filename_cb = input_tape_filename_cb,
	.output_tape_filename_cb = output_tape_filename_cb,
	.update_tape_state = update_tape_state,
	.update_drive_disk = update_drive_disk,
	.update_drive_write_enable = update_drive_write_enable,
	.update_drive_write_back = update_drive_write_back,
};

/* UI events */
static void update_tape_counters(void);
static event_t update_tape_counters_event;

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
static void create_view_menu(void);
static void update_machine_menu(void);
static void update_cartridge_menu(void);

/* for hiding cursor: */
static int cursor_hidden = 0;
static GdkCursor *old_cursor, *blank_cursor;
static gboolean hide_cursor(GtkWidget *widget, GdkEventMotion *event, gpointer data);
static gboolean show_cursor(GtkWidget *widget, GdkEventMotion *event, gpointer data);

/* UI-specific callbacks */
static void fullscreen_changed_cb(int fullscreen);
static void kbd_translate_changed_cb(int kbd_translate);

static int run_cpu(void *data);

/* Helpers */
static char *escape_underscores(const char *str);

/* Drive control widgets */
static GtkWidget *dc_window = NULL;
static GtkWidget *dc_filename_drive[4] = { NULL, NULL, NULL, NULL };
static GtkToggleButton *dc_we_drive[4] = { NULL, NULL, NULL, NULL };
static GtkToggleButton *dc_wb_drive[4] = { NULL, NULL, NULL, NULL };
static GtkWidget *dc_drive_cyl_head = NULL;

static void create_dc_window(void);
static void toggle_dc_window(GtkToggleAction *current, gpointer user_data);
static void hide_dc_window(void);
static void dc_insert(GtkButton *button, gpointer user_data);
static void dc_eject(GtkButton *button, gpointer user_data);

/* Tape control widgets */
static GtkWidget *tc_window = NULL;
static GtkWidget *tc_input_filename = NULL;
static GtkWidget *tc_output_filename = NULL;
static GtkWidget *tc_input_time = NULL;
static GtkWidget *tc_output_time = NULL;
static GtkAdjustment *tc_input_adjustment = NULL;
static GtkAdjustment *tc_output_adjustment = NULL;
static GtkTreeView *tc_input_list = NULL;
static GtkListStore *tc_input_list_store = NULL;
static GtkScrollbar *tc_input_progress = NULL;
static GtkScrollbar *tc_output_progress = NULL;
static GtkToggleButton *tc_fast = NULL;
static GtkToggleButton *tc_pad = NULL;
static GtkToggleButton *tc_rewrite = NULL;

static void create_tc_window(void);
static void toggle_tc_window(GtkToggleAction *current, gpointer user_data);
static void hide_tc_window(void);
static void tc_input_rewind(GtkButton *button, gpointer user_data);
static void tc_output_rewind(GtkButton *button, gpointer user_data);
static void tc_input_insert(GtkButton *button, gpointer user_data);
static void tc_output_insert(GtkButton *button, gpointer user_data);
static void tc_input_eject(GtkButton *button, gpointer user_data);
static void tc_output_eject(GtkButton *button, gpointer user_data);

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
static void insert_disk1(void) { insert_disk(0); }
static void insert_disk2(void) { insert_disk(1); }
static void insert_disk3(void) { insert_disk(2); }
static void insert_disk4(void) { insert_disk(3); }

static void save_snapshot(void) {
	g_idle_remove_by_data(run_cpu);
	xroar_save_snapshot();
	g_idle_add(run_cpu, run_cpu);
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
		int scale2 = (int)((video_module->scale * 2.) + 0.5) + 1;
		if (scale2 < 1) scale2 = 1;
		video_module->resize(160 * scale2, 120 * scale2);
	}
}

static void zoom_out(void) {
	if (video_module && video_module->resize) {
		int scale2 = (int)((video_module->scale * 2.) + 0.5) - 1;
		if (scale2 < 1) scale2 = 1;
		video_module->resize(160 * scale2, 120 * scale2);
	}
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
	xroar_set_cart(val);
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
	gtk_about_dialog_set_copyright(dialog, "Copyright © 2003–2011 Ciaran Anscomb <xroar@6809.org.uk>");
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
"Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
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
	      "<menu name='CrossColourMenu' action='CrossColourMenuAction'/>"
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
	{ .name = "FullScreenAction", .label = "_Full Screen",
	  .stock_id = GTK_STOCK_FULLSCREEN,
	  .accelerator = "F11", .callback = G_CALLBACK(set_fullscreen) },
	/* Tool */
	{ .name = "TranslateKeyboardAction", .label = "_Keyboard Translation",
	  .accelerator = "<control>Z",
	  .callback = G_CALLBACK(toggle_keyboard_translation) },
	{ .name = "DriveControlAction", .label = "_Drive Control",
	  .accelerator = "<control>D",
	  .callback = G_CALLBACK(toggle_dc_window) },
	{ .name = "TapeControlAction", .label = "_Tape Control",
	  .accelerator = "<control>T",
	  .callback = G_CALLBACK(toggle_tc_window) },
	{ .name = "FastSoundAction", .label = "_Fast Sound",
	  .callback = G_CALLBACK(toggle_fast_sound) },
};
static guint ui_n_toggles = G_N_ELEMENTS(ui_toggles);

static GtkRadioActionEntry keymap_radio_entries[] = {
	{ .name = "keymap_dragon", .label = "Dragon Layout", .value = KEYMAP_DRAGON },
	{ .name = "keymap_coco", .label = "CoCo Layout", .value = KEYMAP_COCO },
};

static int init(void) {

	LOG_DEBUG(2, "Initialising GTK+2 UI\n");

	gtk_init(NULL, NULL);

	g_set_application_name("XRoar");

	GtkBuilder *builder;
	GError *error = NULL;
	builder = gtk_builder_new();
	if (!gtk_builder_add_from_string(builder, top_window_glade, -1, &error)) {
		g_warning("Couldn't create UI: %s", error->message);
		g_error_free(error);
		return 1;
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
	gtk_action_group_add_radio_actions(main_action_group, keymap_radio_entries, 2, 0, (GCallback)set_keymap, NULL);

	/* Menu merge points */
	merge_machines = gtk_ui_manager_new_merge_id(gtk2_menu_manager);
	merge_carts = gtk_ui_manager_new_merge_id(gtk2_menu_manager);

	/* Update all dynamic menus */
	create_view_menu();
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
	if (xroar_opt_geometry) {
		gtk_window_parse_geometry(GTK_WINDOW(gtk2_top_window), xroar_opt_geometry);
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
	create_dc_window();

	/* Create (hidden) tape control window */
	create_tc_window();

	/* UI events */
	event_init(&update_tape_counters_event);
	update_tape_counters_event.dispatch = update_tape_counters;
	update_tape_counters_event.at_cycle = current_cycle + OSCILLATOR_RATE / 2;
	event_queue(&UI_EVENT_LIST, &update_tape_counters_event);

	return 0;
}

static void shutdown(void) {
}

static int run_cpu(void *data) {
	(void)data;
	sam_run(VDG_LINE_DURATION * 8);
	while (EVENT_PENDING(UI_EVENT_LIST))
		DISPATCH_NEXT_EVENT(UI_EVENT_LIST);
	return 1;
}

static void run(void) {
	g_idle_add(run_cpu, run_cpu);
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

/* Dynamic elements of View menu */
static void create_view_menu(void) {
	/* Cross-colour */
	static guint merge_cc;
	int i;
	GtkActionGroup *cc_action_group;
	merge_cc = gtk_ui_manager_new_merge_id(gtk2_menu_manager);
	cc_action_group = gtk_action_group_new("View");
	gtk_ui_manager_insert_action_group(gtk2_menu_manager, cc_action_group, 0);

	GtkRadioActionEntry *radio_entries = malloc(NUM_CROSS_COLOUR_PHASES * sizeof(GtkRadioActionEntry));
	memset(radio_entries, 0, NUM_CROSS_COLOUR_PHASES * sizeof(GtkRadioActionEntry));
	/* add these to the ui in reverse order, as each will be
	 * inserted before the previous */
	for (i = NUM_CROSS_COLOUR_PHASES-1; i >= 0; i--) {
		radio_entries[i].name = g_strconcat("cc-", xroar_cross_colour_list[i].name, NULL);
		radio_entries[i].label = escape_underscores(xroar_cross_colour_list[i].description);
		radio_entries[i].value = i;
		gtk_ui_manager_add_ui(gtk2_menu_manager, merge_cc, "/MainMenu/ViewMenu/CrossColourMenu", radio_entries[i].name, radio_entries[i].name, GTK_UI_MANAGER_MENUITEM, TRUE);
	}
	gtk_action_group_add_radio_actions(cc_action_group, radio_entries, NUM_CROSS_COLOUR_PHASES, 0, (GCallback)set_cc, NULL);
	for (i = 0; i < NUM_CROSS_COLOUR_PHASES; i++) {
		g_free((gchar *)radio_entries[i].name);
		free((char *)radio_entries[i].label);
	}
	free(radio_entries);
}

/* Dynamic machine menu */
static void update_machine_menu(void) {
	int num_machines = machine_config_count();
	int i;
	int selected = -1;
	free_action_group(machine_action_group);
	gtk_ui_manager_remove_ui(gtk2_menu_manager, merge_machines);
	if (xroar_machine_config) selected = xroar_machine_config->index;
	GtkRadioActionEntry *radio_entries = malloc(num_machines * sizeof(GtkRadioActionEntry));
	memset(radio_entries, 0, num_machines * sizeof(GtkRadioActionEntry));
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
		g_free((gchar *)radio_entries[i].name);
		free((char *)radio_entries[i].label);
	}
	free(radio_entries);
}

/* Dynamic cartridge menu */
static void update_cartridge_menu(void) {
	int num_carts = cart_config_count();
	int i;
	int selected = 0;
	free_action_group(cart_action_group);
	gtk_ui_manager_remove_ui(gtk2_menu_manager, merge_carts);
	if (xroar_cart_config) selected = xroar_cart_config->index;
	GtkRadioActionEntry *radio_entries = malloc((num_carts+1) * sizeof(GtkRadioActionEntry));
	memset(radio_entries, 0, (num_carts+1) * sizeof(GtkRadioActionEntry));
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
		g_free((gchar *)radio_entries[i].name);
		free((char *)radio_entries[i].label);
	}
	free(radio_entries);
}

/* Cursor hiding */

static gboolean hide_cursor(GtkWidget *widget, GdkEventMotion *event, gpointer data) {
	(void)widget;
	(void)event;
	(void)data;
	if (cursor_hidden)
		return FALSE;
	GdkWindow *window = gtk_widget_get_window(gtk2_drawing_area);
	old_cursor = gdk_window_get_cursor(window);
	gdk_window_set_cursor(window, blank_cursor);
	cursor_hidden = 1;
	return FALSE;
}

static gboolean show_cursor(GtkWidget *widget, GdkEventMotion *event, gpointer data) {
	(void)widget;
	(void)event;
	(void)data;
	if (!cursor_hidden)
		return FALSE;
	GdkWindow *window = gtk_widget_get_window(gtk2_drawing_area);
	gdk_window_set_cursor(window, old_cursor);
	cursor_hidden = 0;
	return FALSE;
}

/* View callbacks */

static void fullscreen_changed_cb(int fullscreen) {
	GtkToggleAction *toggle = (GtkToggleAction *)gtk_ui_manager_get_action(gtk2_menu_manager, "/MainMenu/ViewMenu/FullScreen");
	gtk_toggle_action_set_active(toggle, fullscreen);
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

static void keymap_changed_cb(int keymap) {
	GtkRadioAction *radio = (GtkRadioAction *)gtk_ui_manager_get_action(gtk2_menu_manager, "/MainMenu/MachineMenu/KeymapMenu/keymap_dragon");
	gtk_radio_action_set_current_value(radio, keymap);
}

/* Tool callbacks */

static void kbd_translate_changed_cb(int kbd_translate) {
	GtkToggleAction *toggle = (GtkToggleAction *)gtk_ui_manager_get_action(gtk2_menu_manager, "/MainMenu/ToolMenu/TranslateKeyboard");
	gtk_toggle_action_set_active(toggle, kbd_translate);
}

static void fast_sound_changed_cb(int fast) {
	GtkToggleAction *toggle = (GtkToggleAction *)gtk_ui_manager_get_action(gtk2_menu_manager, "/MainMenu/ToolMenu/FastSound");
	gtk_toggle_action_set_active(toggle, fast);
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
	char *ret_str = malloc(len + 1);
	if (!ret_str) return NULL;
	for (in = str, out = ret_str; *in; in++) {
		*(out++) = *in;
		if (*in == '_') {
			*(out++) = '_';
		}
	}
	*out = 0;
	return ret_str;
}

/* Drive control */

static void create_dc_window(void) {
	GtkBuilder *builder;
	GtkWidget *widget;
	GError *error = NULL;
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
	g_signal_connect(dc_window, "delete-event", G_CALLBACK(hide_dc_window), NULL);
	widget = GTK_WIDGET(gtk_builder_get_object(builder, "eject_drive1"));
	g_signal_connect(widget, "clicked", G_CALLBACK(dc_eject), (gpointer)0);
	widget = GTK_WIDGET(gtk_builder_get_object(builder, "eject_drive2"));
	g_signal_connect(widget, "clicked", G_CALLBACK(dc_eject), (gpointer)0 + 1);
	widget = GTK_WIDGET(gtk_builder_get_object(builder, "eject_drive3"));
	g_signal_connect(widget, "clicked", G_CALLBACK(dc_eject), (gpointer)0 + 2);
	widget = GTK_WIDGET(gtk_builder_get_object(builder, "eject_drive4"));
	g_signal_connect(widget, "clicked", G_CALLBACK(dc_eject), (gpointer)0 + 3);
	widget = GTK_WIDGET(gtk_builder_get_object(builder, "insert_drive1"));
	g_signal_connect(widget, "clicked", G_CALLBACK(dc_insert), (gpointer)0 + 0);
	widget = GTK_WIDGET(gtk_builder_get_object(builder, "insert_drive2"));
	g_signal_connect(widget, "clicked", G_CALLBACK(dc_insert), (gpointer)0 + 1);
	widget = GTK_WIDGET(gtk_builder_get_object(builder, "insert_drive3"));
	g_signal_connect(widget, "clicked", G_CALLBACK(dc_insert), (gpointer)0 + 2);
	widget = GTK_WIDGET(gtk_builder_get_object(builder, "insert_drive4"));
	g_signal_connect(widget, "clicked", G_CALLBACK(dc_insert), (gpointer)0 + 3);

	/* In case any signals remain... */
	gtk_builder_connect_signals(builder, NULL);
	g_object_unref(builder);

	vdrive_update_drive_cyl_head = update_drive_cyl_head;
}

/* Drive Control - Signal Handlers */

static void toggle_dc_window(GtkToggleAction *current, gpointer user_data) {
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
	int drive = user_data - (gpointer)0;
	(void)button;
	xroar_insert_disk(drive);
}

static void dc_eject(GtkButton *button, gpointer user_data) {
	int drive = user_data - (gpointer)0;
	(void)button;
	xroar_eject_disk(drive);
}

/* Drive Control - UI callbacks */

static void update_drive_disk(int drive, struct vdisk *disk) {
	if (drive < 0 || drive > 3)
		return;
	char *filename = NULL;
	int we = 1, wb = 0;
	if (disk) {
		filename = disk->filename;
		we = (disk->write_protect == VDISK_WRITE_ENABLE);
		wb = (disk->file_write_protect == VDISK_WRITE_ENABLE);
	}
	gtk_label_set_text(GTK_LABEL(dc_filename_drive[drive]), filename);
	update_drive_write_enable(drive, we);
	update_drive_write_back(drive, wb);
}

static void update_drive_write_enable(int drive, int write_enable) {
	if (drive >= 0 && drive <= 3) {
		if (write_enable >= 0) {
			gtk_toggle_button_set_active(dc_we_drive[drive], write_enable ? TRUE : FALSE);
		}
	}
}

static void update_drive_write_back(int drive, int write_back) {
	if (drive >= 0 && drive <= 3) {
		if (write_back >= 0) {
			gtk_toggle_button_set_active(dc_wb_drive[drive], write_back ? TRUE : FALSE);
		}
	}
}

static void update_drive_cyl_head(int drive, int cyl, int head) {
	char string[16];
	snprintf(string, sizeof(string), "Dr %01d Tr %02d He %01d", drive + 1, cyl, head);
	gtk_label_set_text(GTK_LABEL(dc_drive_cyl_head), string);
}

/* Tape control window */

static gchar *ms_to_string(int ms);

enum {
	TC_FILENAME = 0,
	TC_POSITION,
	TC_FILE_POINTER,
	TC_MAX
};

static int have_input_list_store = 0;

static void input_file_selected(GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data) {
	GtkTreeIter iter;
	struct tape_file *file;
	(void)tree_view;
	(void)column;
	(void)user_data;
	gtk_tree_model_get_iter(GTK_TREE_MODEL(tc_input_list_store), &iter, path);
	gtk_tree_model_get(GTK_TREE_MODEL(tc_input_list_store), &iter, TC_FILE_POINTER, &file, -1);
	tape_seek_to_file(tape_input, file);
}

static void tc_toggled_fast(GtkToggleButton *togglebutton, gpointer user_data);
static void tc_toggled_pad(GtkToggleButton *togglebutton, gpointer user_data);
static void tc_toggled_rewrite(GtkToggleButton *togglebutton, gpointer user_data);
static gboolean tc_input_progress_change(GtkRange *range, GtkScrollType scroll, gdouble value, gpointer user_data);
static gboolean tc_output_progress_change(GtkRange *range, GtkScrollType scroll, gdouble value, gpointer user_data);

/* Tape control */

static void create_tc_window(void) {
	GtkBuilder *builder;
	GtkWidget *widget;
	GError *error = NULL;
	builder = gtk_builder_new();
	if (!gtk_builder_add_from_string(builder, tapecontrol_glade, -1, &error)) {
		g_warning("Couldn't create UI: %s", error->message);
		g_error_free(error);
		return;
	}

	/* Extract UI elements modified elsewhere */
	tc_window = GTK_WIDGET(gtk_builder_get_object(builder, "tc_window"));
	tc_input_filename = GTK_WIDGET(gtk_builder_get_object(builder, "input_filename"));
	tc_output_filename = GTK_WIDGET(gtk_builder_get_object(builder, "output_filename"));
	tc_input_time = GTK_WIDGET(gtk_builder_get_object(builder, "input_file_time"));
	tc_output_time = GTK_WIDGET(gtk_builder_get_object(builder, "output_file_time"));
	tc_input_adjustment = GTK_ADJUSTMENT(gtk_builder_get_object(builder, "input_file_adjustment"));
	tc_output_adjustment = GTK_ADJUSTMENT(gtk_builder_get_object(builder, "output_file_adjustment"));
	tc_input_list = GTK_TREE_VIEW(gtk_builder_get_object(builder, "input_file_list_view"));
	tc_input_list_store = GTK_LIST_STORE(gtk_builder_get_object(builder, "input_file_list_store"));
	tc_input_progress = GTK_SCROLLBAR(gtk_builder_get_object(builder, "input_file_progress"));
	tc_output_progress = GTK_SCROLLBAR(gtk_builder_get_object(builder, "output_file_progress"));
	tc_fast = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "fast"));
	tc_pad = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "pad"));
	tc_rewrite = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "rewrite"));

	/* Connect signals */
	g_signal_connect(tc_window, "delete-event", G_CALLBACK(hide_tc_window), NULL);
	g_signal_connect(tc_input_list, "row-activated", G_CALLBACK(input_file_selected), NULL);
	g_signal_connect(tc_input_progress, "change-value", G_CALLBACK(tc_input_progress_change), NULL);
	g_signal_connect(tc_output_progress, "change-value", G_CALLBACK(tc_output_progress_change), NULL);
	g_signal_connect(tc_fast, "toggled", G_CALLBACK(tc_toggled_fast), NULL);
	g_signal_connect(tc_pad, "toggled", G_CALLBACK(tc_toggled_pad), NULL);
	g_signal_connect(tc_rewrite, "toggled", G_CALLBACK(tc_toggled_rewrite), NULL);

	widget = GTK_WIDGET(gtk_builder_get_object(builder, "input_rewind"));
	g_signal_connect(widget, "clicked", G_CALLBACK(tc_input_rewind), NULL);
	widget = GTK_WIDGET(gtk_builder_get_object(builder, "input_insert"));
	g_signal_connect(widget, "clicked", G_CALLBACK(tc_input_insert), NULL);
	widget = GTK_WIDGET(gtk_builder_get_object(builder, "input_eject"));
	g_signal_connect(widget, "clicked", G_CALLBACK(tc_input_eject), NULL);
	widget = GTK_WIDGET(gtk_builder_get_object(builder, "output_rewind"));
	g_signal_connect(widget, "clicked", G_CALLBACK(tc_output_rewind), NULL);
	widget = GTK_WIDGET(gtk_builder_get_object(builder, "output_insert"));
	g_signal_connect(widget, "clicked", G_CALLBACK(tc_output_insert), NULL);
	widget = GTK_WIDGET(gtk_builder_get_object(builder, "output_eject"));
	g_signal_connect(widget, "clicked", G_CALLBACK(tc_output_eject), NULL);

	/* In case any signals remain... */
	gtk_builder_connect_signals(builder, NULL);
	g_object_unref(builder);
}

/* Tape Control - helper functions */

static void update_input_list_store(void) {
	if (have_input_list_store) return;
	if (!tape_input) return;
	have_input_list_store = 1;
	struct tape_file *file;
	long old_offset = tape_tell(tape_input);
	tape_rewind(tape_input);
	while ((file = tape_file_next(tape_input, 1))) {
		GtkTreeIter iter;
		int ms = tape_to_ms(tape_input, file->offset);
		gchar *timestr = ms_to_string(ms);
		gtk_list_store_append(tc_input_list_store, &iter);
		gtk_list_store_set(tc_input_list_store, &iter,
				   TC_FILENAME, file->name,
				   TC_POSITION, timestr,
				   TC_FILE_POINTER, file,
				   -1);
	}
	tape_seek(tape_input, old_offset, FS_SEEK_SET);
}

static gchar *ms_to_string(int ms) {
	static gchar timestr[9];
	int min, sec;
	sec = ms / 1000;
	min = sec / 60;
	sec -= min * 60;
	min %= 60;
	snprintf(timestr, sizeof(timestr), "%02d:%02d", min, sec);
	return timestr;
}

static void tc_seek(struct tape *tape, GtkScrollType scroll, gdouble value) {
	if (tape) {
		int seekms = 0;
		switch (scroll) {
			case GTK_SCROLL_STEP_BACKWARD:
				seekms = tape_to_ms(tape, tape->offset) - 1000;
				break;
			case GTK_SCROLL_STEP_FORWARD:
				seekms = tape_to_ms(tape, tape->offset) - 1000;
				break;
			case GTK_SCROLL_PAGE_BACKWARD:
				seekms = tape_to_ms(tape, tape->offset) - 5000;
				break;
			case GTK_SCROLL_PAGE_FORWARD:
				seekms = tape_to_ms(tape, tape->offset) + 5000;
				break;
			case GTK_SCROLL_JUMP:
				seekms = (int)value;
				break;
			default:
				return;
		}
		if (seekms < 0) return;
		long seek_to = tape_ms_to(tape, seekms);
		if (seek_to > tape->size) seek_to = tape->size;
		tape_seek(tape, seek_to, FS_SEEK_SET);
	}
}

/* Tape Control - scheduled event handlers */

static void update_tape_counters(void) {
	static long omax = -1, opos = -1;
	static long imax = -1, ipos = -1;
	long new_omax = 0, new_opos = 0;
	long new_imax = 0, new_ipos = 0;
	if (tape_input) {
		new_imax = tape_to_ms(tape_input, tape_input->size);
		new_ipos = tape_to_ms(tape_input, tape_input->offset);
	}
	if (tape_output) {
		new_omax = tape_to_ms(tape_output, tape_output->size);
		new_opos = tape_to_ms(tape_output, tape_output->offset);
	}
	if (imax != new_imax) {
		imax = new_imax;
		gtk_adjustment_set_upper(GTK_ADJUSTMENT(tc_input_adjustment), (gdouble)imax);
	}
	if (ipos != new_ipos) {
		ipos = new_ipos;
		gtk_adjustment_set_value(GTK_ADJUSTMENT(tc_input_adjustment), (gdouble)ipos);
		gtk_label_set_text(GTK_LABEL(tc_input_time), ms_to_string(new_ipos));
	}
	if (omax != new_omax) {
		omax = new_omax;
		gtk_adjustment_set_upper(GTK_ADJUSTMENT(tc_output_adjustment), (gdouble)omax);
	}
	if (opos != new_opos) {
		opos = new_opos;
		gtk_adjustment_set_value(GTK_ADJUSTMENT(tc_output_adjustment), (gdouble)opos);
		gtk_label_set_text(GTK_LABEL(tc_output_time), ms_to_string(new_opos));
	}
	update_tape_counters_event.at_cycle += OSCILLATOR_RATE / 2;
	event_queue(&UI_EVENT_LIST, &update_tape_counters_event);
}

/* Tape Control - UI callbacks */

static void input_tape_filename_cb(const char *filename) {
	GtkToggleAction *toggle = (GtkToggleAction *)gtk_ui_manager_get_action(gtk2_menu_manager, "/MainMenu/ToolMenu/TapeControl");
	gtk_label_set_text(GTK_LABEL(tc_input_filename), filename);
	GtkTreeIter iter;
	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(tc_input_list_store), &iter)) {
		do {
			struct tape_file *file;
			gtk_tree_model_get(GTK_TREE_MODEL(tc_input_list_store), &iter, TC_FILE_POINTER, &file, -1);
			free(file);
		} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(tc_input_list_store), &iter));
	}
	gtk_list_store_clear(tc_input_list_store);
	have_input_list_store = 0;
	if (gtk_toggle_action_get_active(toggle)) {
		update_input_list_store();
	}
}

static void output_tape_filename_cb(const char *filename) {
	gtk_label_set_text(GTK_LABEL(tc_output_filename), filename);
}

static void tc_toggled_fast(GtkToggleButton *togglebutton, gpointer user_data) {
	(void)user_data;
	int set = gtk_toggle_button_get_active(togglebutton) ? TAPE_FAST : 0;
	int flags = (tape_get_state() & ~TAPE_FAST) | set;
	tape_set_state(flags);
}

static void tc_toggled_pad(GtkToggleButton *togglebutton, gpointer user_data) {
	(void)user_data;
	int set = gtk_toggle_button_get_active(togglebutton) ? TAPE_PAD : 0;
	int flags = (tape_get_state() & ~TAPE_PAD) | set;
	tape_set_state(flags);
}

static void tc_toggled_rewrite(GtkToggleButton *togglebutton, gpointer user_data) {
	(void)user_data;
	int set = gtk_toggle_button_get_active(togglebutton) ? TAPE_REWRITE : 0;
	int flags = (tape_get_state() & ~TAPE_REWRITE) | set;
	tape_set_state(flags);
}

static void update_tape_state(int flags) {
	gtk_toggle_button_set_active(tc_fast, (flags & TAPE_FAST) ? TRUE : FALSE);
	gtk_toggle_button_set_active(tc_pad, (flags & TAPE_PAD) ? TRUE : FALSE);
	gtk_toggle_button_set_active(tc_rewrite, (flags & TAPE_REWRITE) ? TRUE : FALSE);
}

/* Tape Control - Signal Handlers */

static void toggle_tc_window(GtkToggleAction *current, gpointer user_data) {
	gboolean val = gtk_toggle_action_get_active(current);
	(void)user_data;
	if (val) {
		gtk_widget_show(tc_window);
		update_input_list_store();
	} else {
		gtk_widget_hide(tc_window);
	}
}

static void hide_tc_window(void) {
	GtkToggleAction *toggle = (GtkToggleAction *)gtk_ui_manager_get_action(gtk2_menu_manager, "/MainMenu/ToolMenu/TapeControl");
	gtk_toggle_action_set_active(toggle, 0);
}

/* Tape Control - Signal Handlers - Input Tab */

static gboolean tc_input_progress_change(GtkRange *range, GtkScrollType scroll, gdouble value, gpointer user_data) {
	(void)range;
	(void)user_data;
	tc_seek(tape_input, scroll, value);
	return TRUE;
}

static void tc_input_rewind(GtkButton *button, gpointer user_data) {
	(void)button;
	(void)user_data;
	if (tape_input) {
		tape_seek(tape_input, 0, FS_SEEK_SET);
	}
}

static void tc_input_insert(GtkButton *button, gpointer user_data) {
	(void)button;
	(void)user_data;
	xroar_select_tape_input();
}

static void tc_input_eject(GtkButton *button, gpointer user_data) {
	(void)button;
	(void)user_data;
	xroar_eject_tape_input();
}

static gboolean tc_output_progress_change(GtkRange *range, GtkScrollType scroll, gdouble value, gpointer user_data) {
	(void)range;
	(void)user_data;
	tc_seek(tape_output, scroll, value);
	return TRUE;
}

static void tc_output_rewind(GtkButton *button, gpointer user_data) {
	(void)button;
	(void)user_data;
	if (tape_output) {
		tape_seek(tape_output, 0, FS_SEEK_SET);
	}
}

static void tc_output_insert(GtkButton *button, gpointer user_data) {
	(void)button;
	(void)user_data;
	xroar_select_tape_output();
}

static void tc_output_eject(GtkButton *button, gpointer user_data) {
	(void)button;
	(void)user_data;
	xroar_eject_tape_output();
}
