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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

#include "types.h"
#include "logging.h"
#include "cart.h"
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

/* Module callbacks */
static void machine_changed_cb(int machine_type);
static void cart_changed_cb(int cart_index);
static void keymap_changed_cb(int keymap);

UIModule ui_gtk2_module = {
	.common = { .name = "gtk2", .description = "GTK+-2 user-interface",
	            .init = init, .shutdown = shutdown },
	.run = &run,
	.video_module_list = gtk2_video_module_list,
	.keyboard_module_list = gtk2_keyboard_module_list,
	.machine_changed_cb = machine_changed_cb,
	.cart_changed_cb = cart_changed_cb,
	.keymap_changed_cb = keymap_changed_cb,
};

GtkWidget *gtk2_top_window = NULL;
static GtkWidget *vbox;
GtkUIManager *gtk2_menu_manager;
GtkWidget *gtk2_menubar;
GtkWidget *gtk2_drawing_area;

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

static void set_cart(GtkRadioAction *action, GtkRadioAction *current, gpointer user_data) {
	gint val = gtk_radio_action_get_current_value(current);
	(void)action;
	(void)user_data;
	xroar_set_cart(val);
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
	xroar_set_kbd_translate(val);
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
	    "<menu name='EmulationMenu' action='EmulationMenuAction'>"
	      "<menu name='MachineMenu' action='MachineMenuAction'>"
	      "</menu>"
	      "<menu name='CartridgeMenu' action='CartridgeMenuAction'>"
	      "</menu>"
	      "<menu name='KeyboardMenu' action='KeyboardMenuAction'>"
	        "<menuitem action='keymap_dragon'/>"
	        "<menuitem action='keymap_coco'/>"
	      "</menu>"
	      "<separator/>"
	      "<menuitem name='SoftReset' action='SoftResetAction'/>"
	      "<menuitem name='HardReset' action='HardResetAction'/>"
	    "</menu>"
	    "<menu name='InterfaceMenu' action='InterfaceMenuAction'>"
	      "<menuitem name='FullScreen' action='FullScreenAction'/>"
	      "<menuitem name='TranslateKeyboard' action='TranslateKeyboardAction'/>"
	    "</menu>"
	    "<menu name='HelpMenu' action='HelpMenuAction'>"
	      "<menuitem name='About' action='AboutAction'/>"
	    "</menu>"
	  "</menubar>"
	"</ui>";

static GtkActionEntry ui_entries[] = {
	/* Top level */
	{ .name = "FileMenuAction", .label = "_File" },
	{ .name = "EmulationMenuAction", .label = "_Emulation" },
	{ .name = "InterfaceMenuAction", .label = "_Interface" },
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
	{ .name = "SaveSnapshotAction", .stock_id = GTK_STOCK_SAVE_AS, .label = "_Save Snapshot",
	  .accelerator = "<control>S",
	  .callback = G_CALLBACK(save_snapshot) },
	{ .name = "QuitAction", .stock_id = GTK_STOCK_QUIT, .label = "_Quit",
	  .accelerator = "<control>Q",
	  .tooltip = "Quit",
	  .callback = G_CALLBACK(xroar_quit) },
	/* Emulation */
	{ .name = "MachineMenuAction", .label = "_Machine" },
	{ .name = "CartridgeMenuAction", .label = "_Cartridge" },
	{ .name = "KeyboardMenuAction", .label = "_Keyboard" },
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
	/* Interface */
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

	/* Create a UI from XML */
	gtk2_menu_manager = gtk_ui_manager_new();

	GError *error = NULL;
	gtk_ui_manager_add_ui_from_string(gtk2_menu_manager, ui, -1, &error);
	if (error) {
		g_message("building menus failed: %s", error->message);
		g_error_free(error);
	}

	/* Set up main action group */
	GtkActionGroup *action_group = gtk_action_group_new("Actions");
	gtk_action_group_add_actions(action_group, ui_entries, ui_n_entries, NULL);
	gtk_action_group_add_toggle_actions(action_group, ui_toggles, ui_n_toggles, NULL);
	gtk_action_group_add_radio_actions(action_group, keymap_radio_entries, 2, 0, (GCallback)set_keymap, NULL);

	/* Menu merge points */
	guint merge_machines = gtk_ui_manager_new_merge_id(gtk2_menu_manager);
	guint merge_carts = gtk_ui_manager_new_merge_id(gtk2_menu_manager);

	/* Add machines to menu */
	{
		int num_machines = machine_config_count();
		int i;
		int selected = -1;
		if (xroar_machine_config) selected = xroar_machine_config->index;
		GtkRadioActionEntry *machine_radio_entries = malloc(num_machines * sizeof(GtkRadioActionEntry));
		memset(machine_radio_entries, 0, num_machines * sizeof(GtkRadioActionEntry));
		/* add these to the ui in reverse order, as each will be
		 * inserted before the previous */
		for (i = num_machines-1; i >= 0; i--) {
			struct machine_config *mc = machine_config_index(i);
			machine_radio_entries[i].name = g_strconcat("machine-", mc->name, NULL);
			machine_radio_entries[i].label = escape_underscores(mc->description);
			machine_radio_entries[i].value = i;
			gtk_ui_manager_add_ui(gtk2_menu_manager, merge_machines, "/MainMenu/EmulationMenu/MachineMenu", machine_radio_entries[i].name, machine_radio_entries[i].name, GTK_UI_MANAGER_MENUITEM, TRUE);
		}
		gtk_action_group_add_radio_actions(action_group, machine_radio_entries, num_machines, selected, (GCallback)set_machine, NULL);
		for (i = 0; i < num_machines; i++) {
			g_free((gchar *)machine_radio_entries[i].name);
			free((char *)machine_radio_entries[i].label);
		}
		free(machine_radio_entries);
	}

	/* Add cartridges to menu */
	{
		int num_carts = cart_config_count();
		int i;
		int selected = 0;
		if (xroar_cart_config) selected = xroar_cart_config->index;
		GtkRadioActionEntry *cart_radio_entries = malloc((num_carts+1) * sizeof(GtkRadioActionEntry));
		memset(cart_radio_entries, 0, (num_carts+1) * sizeof(GtkRadioActionEntry));
		/* add these to the ui in reverse order, as each will be
		   inserted before the previous */
		for (i = num_carts-1; i >= 0; i--) {
			struct cart_config *cc = cart_config_index(i);
			cart_radio_entries[i].name = g_strconcat("cart-", cc->name, NULL);
			cart_radio_entries[i].label = escape_underscores(cc->description);
			cart_radio_entries[i].value = i;
			gtk_ui_manager_add_ui(gtk2_menu_manager, merge_carts, "/MainMenu/EmulationMenu/CartridgeMenu", cart_radio_entries[i].name, cart_radio_entries[i].name, GTK_UI_MANAGER_MENUITEM, TRUE);
		}
		cart_radio_entries[num_carts].name = "none";
		cart_radio_entries[num_carts].label = "None";
		cart_radio_entries[num_carts].value = -1;
		gtk_ui_manager_add_ui(gtk2_menu_manager, merge_carts, "/MainMenu/EmulationMenu/CartridgeMenu", cart_radio_entries[num_carts].name, cart_radio_entries[num_carts].name, GTK_UI_MANAGER_MENUITEM, TRUE);
		gtk_action_group_add_radio_actions(action_group, cart_radio_entries, num_carts+1, selected, (GCallback)set_cart, NULL);
		/* don't need to free last label */
		for (i = 0; i < num_carts; i++) {
			g_free((gchar *)cart_radio_entries[i].name);
			free((char *)cart_radio_entries[i].label);
		}
		free(cart_radio_entries);
	}

	gtk_ui_manager_insert_action_group(gtk2_menu_manager, action_group, 0);

	/* Extract menubar widget and add to vbox */
	gtk2_menubar = gtk_ui_manager_get_widget(gtk2_menu_manager, "/MainMenu");
	gtk_box_pack_start(GTK_BOX(vbox), gtk2_menubar, FALSE, FALSE, 0);
	gtk_window_add_accel_group(GTK_WINDOW(gtk2_top_window), gtk_ui_manager_get_accel_group(gtk2_menu_manager));

	/* Create drawing_area widget, add to vbox */
	gtk2_drawing_area = gtk_drawing_area_new();
	GdkGeometry hints = {
		.min_width = 320, .min_height = 240,
		.base_width = 0, .base_height = 0,
	};
	gtk_window_set_geometry_hints(GTK_WINDOW(gtk2_top_window), GTK_WIDGET(gtk2_drawing_area), &hints, GDK_HINT_MIN_SIZE | GDK_HINT_BASE_SIZE);
	gtk_box_pack_start(GTK_BOX(vbox), gtk2_drawing_area, TRUE, TRUE, 0);
	gtk_widget_show(gtk2_drawing_area);

	/* Now up to video module to do something with this drawing_area */

	xroar_fullscreen_changed_cb = fullscreen_changed_cb;
	xroar_kbd_translate_changed_cb = kbd_translate_changed_cb;

	/* Cursor hiding */
	blank_cursor = gdk_cursor_new(GDK_BLANK_CURSOR);
	gtk_widget_add_events(gtk2_drawing_area, GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);
	g_signal_connect(G_OBJECT(gtk2_top_window), "key-press-event", G_CALLBACK(hide_cursor), NULL);
	g_signal_connect(G_OBJECT(gtk2_drawing_area), "motion-notify-event", G_CALLBACK(show_cursor), NULL);

	/* Show everything created underneath gtk2_top_window.  Video module
	 * init() is responsible for showing gtk2_top_window.  */
	gtk_widget_show_all(vbox);

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
	g_idle_add(run_cpu, run_cpu);
	gtk_main();
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

/* Emulation callbacks */

static void machine_changed_cb(int machine_type) {
	GtkRadioAction *radio = (GtkRadioAction *)gtk_ui_manager_get_action(gtk2_menu_manager, "/MainMenu/EmulationMenu/MachineMenu/machine-dragon32");
	gtk_radio_action_set_current_value(radio, machine_type);
}

static void cart_changed_cb(int cart_index) {
	GtkRadioAction *radio = (GtkRadioAction *)gtk_ui_manager_get_action(gtk2_menu_manager, "/MainMenu/EmulationMenu/CartridgeMenu/cart-dragondos");
	gtk_radio_action_set_current_value(radio, cart_index);
}

static void keymap_changed_cb(int keymap) {
	GtkRadioAction *radio = (GtkRadioAction *)gtk_ui_manager_get_action(gtk2_menu_manager, "/MainMenu/EmulationMenu/KeyboardMenu/keymap_dragon");
	gtk_radio_action_set_current_value(radio, keymap);
}

/* Interface callbacks */

static void fullscreen_changed_cb(int fullscreen) {
	GtkToggleAction *toggle = (GtkToggleAction *)gtk_ui_manager_get_action(gtk2_menu_manager, "/MainMenu/InterfaceMenu/FullScreen");
	gtk_toggle_action_set_active(toggle, fullscreen);
}

static void kbd_translate_changed_cb(int kbd_translate) {
	GtkToggleAction *toggle = (GtkToggleAction *)gtk_ui_manager_get_action(gtk2_menu_manager, "/MainMenu/InterfaceMenu/TranslateKeyboard");
	gtk_toggle_action_set_active(toggle, kbd_translate);
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
