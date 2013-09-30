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

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#ifdef WANT_GDB_TARGET
#include <pthread.h>
#endif

#include "pl_glib.h"
#include "pl_string.h"

#include "cart.h"
#include "crclist.h"
#include "events.h"
#include "fs.h"
#include "gdb.h"
#include "hd6309_trace.h"
#include "hexs19.h"
#include "joystick.h"
#include "keyboard.h"
#include "logging.h"
#include "machine.h"
#include "mc6809_trace.h"
#include "module.h"
#include "path.h"
#include "printer.h"
#include "romlist.h"
#include "sam.h"
#include "snapshot.h"
#include "sound.h"
#include "tape.h"
#include "vdg.h"
#include "vdg_palette.h"
#include "vdisk.h"
#include "vdrive.h"
#include "xconfig.h"
#include "xroar.h"

#ifdef WINDOWS32
#include "windows32/common_windows32.h"
#endif

/* Configuration directives */

// Public

struct xroar_cfg xroar_cfg = {
	.gl_filter = ANY_AUTO,
	.ccr = CROSS_COLOUR_5BIT,
};

// Private

struct private_cfg {
	/* Emulated machine */
	char *default_machine;
	char *machine_desc;
	int machine_arch;
	int machine_cpu;
	char *machine_palette;
	char *bas;
	char *extbas;
	char *altbas;
	int nobas;
	int noextbas;
	int noaltbas;
	int tv;
	int vdg_type;
	char *machine_cart;
	int ram;
	int nodos;

	/* Emulated cartridge */
	char *cart_desc;
	int cart_type;
	char *cart_rom;
	char *cart_rom2;
	int cart_becker;
	int cart_autorun;

	/* Attach files */
	char *run;
	char *tape_write;
	char *lp_file;
	char *lp_pipe;
	GSList *load_list;
	GSList *type_list;

	/* Emulator interface */
	char *ui;
	char *filereq;
	char *vo;
	char *ao;
	int volume;
	char *joy_right;
	char *joy_left;
	char *joy_virtual;
	char *joy_desc;
	int tape_fast;
	int tape_pad;
	int tape_pad_auto;
	int tape_rewrite;
	_Bool gdb;

	char *joy_axis[JOYSTICK_NUM_AXES];
	char *joy_button[JOYSTICK_NUM_BUTTONS];

	_Bool config_print;
	char *timeout;
};

static struct private_cfg private_cfg = {
	.machine_arch = ANY_AUTO,
	.machine_cpu = CPU_MC6809,
	.nobas = -1,
	.noextbas = -1,
	.noaltbas = -1,
	.tv = ANY_AUTO,
	.vdg_type = -1,
	.nodos = -1,
	.cart_type = ANY_AUTO,
	.cart_becker = ANY_AUTO,
	.cart_autorun = ANY_AUTO,
	.volume = 100,
	.tape_fast = 1,
	.tape_pad = -1,
	.tape_pad_auto = 1,
	.gdb = 0,
};

/* Helper functions used by configuration */
static void set_machine(const char *name);
static void set_pal(void);
static void set_ntsc(void);
static void set_cart(const char *name);
static void add_load(char *string);
static void type_command(char *string);
static void set_joystick(const char *name);
static void set_joystick_axis(const char *spec);
static void set_joystick_button(const char *spec);

/* Help texts */
static void helptext(void);
static void versiontext(void);
static void config_print_all(void);

static int load_disk_to_drive = 0;

static int timeout_seconds;
static int timeout_cycles;

static struct joystick_config *cur_joy_config = NULL;

static struct xconfig_option xroar_options[];

/**************************************************************************/
/* Global flags */

_Bool xroar_noratelimit = 0;
int xroar_frameskip = 0;

struct machine_config *xroar_machine_config;
static struct cart_config *selected_cart_config;
struct cart *xroar_cart;
struct vdg_palette *xroar_vdg_palette;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

/* Default configuration */

static const char *default_config[] = {
	// Dragon 32
	"machine dragon32",
	"machine-desc Dragon 32",
	"machine-arch dragon32",
	"tv-type pal",
	"ram 32",
	// Dragon 64
	"machine dragon64",
	"machine-desc Dragon 64",
	"machine-arch dragon64",
	"tv-type pal",
	"ram 64",
	// Tano Dragon
	"machine tano",
	"machine-desc Tano Dragon (NTSC)",
	"machine-arch dragon64",
	"tv-type ntsc",
	"ram 64",
	// CoCo
	"machine coco",
	"machine-desc Tandy CoCo (PAL)",
	"machine-arch coco",
	"tv-type pal",
	"ram 64",
	// CoCo (US)
	"machine cocous",
	"machine-desc Tandy CoCo (NTSC)",
	"machine-arch coco",
	"tv-type ntsc",
	"ram 64",
	// CoCo 2B
	"machine coco2b",
	"machine-desc Tandy CoCo 2B (PAL,T1)",
	"machine-arch coco",
	"tv-type pal",
	"vdg-type 6847t1",
	"ram 64",
	// CoCo 2B (US)
	"machine coco2bus",
	"machine-desc Tandy CoCo 2B (NTSC,T1)",
	"machine-arch coco",
	"tv-type ntsc",
	"ram 64",
	// Dynacom MX-1600
	"machine mx1600",
	"machine-desc Dynacom MX-1600",
	"machine-arch coco",
	"bas @mx1600",
	"extbas @mx1600ext",
	"tv-type pal-m",
	"ram 64",

	// DragonDOS
	"cart dragondos",
	"cart-desc DragonDOS",
	"cart-type dragondos",
	"cart-rom @dragondos_compat",
	// RSDOS
	"cart rsdos",
	"cart-desc RS-DOS",
	"cart-type rsdos",
	"cart-rom @rsdos",
	// Delta
	"cart delta",
	"cart-desc Delta System",
	"cart-type delta",
	"cart-rom @delta",
	// RSDOS w/ Becker port
	"cart becker",
	"cart-desc RS-DOS with becker port",
	"cart-type rsdos",
	"cart-rom @rsdos_becker",
	"cart-becker",
	// Orchestra 90
	"cart orch90",
	"cart-desc Orchestra-90 CC",
	"cart-type orch90",
	"cart-rom orch90",

	// ROM lists

	// Fallback Dragon BASIC
	"romlist dragon=dragon",
	"romlist d64_1=d64_1,d64rom1,Dragon Data Ltd - Dragon 64 - IC17,Dragon Data Ltd - TANO IC18,Eurohard S.A. - Dragon 200 IC18,dragrom",
	"romlist d64_2=d64_2,d64rom2,Dragon Data Ltd - Dragon 64 - IC18,Dragon Data Ltd - TANO IC17,Eurohard S.A. - Dragon 200 IC17",
	"romlist d32=d32,dragon32,d32rom,Dragon Data Ltd - Dragon 32 - IC17",
	// Specific Dragon BASIC
	"romlist dragon64=@d64_1,@dragon",
	"romlist dragon64_alt=@d64_2",
	"romlist dragon32=@d32,@dragon",
	// Fallback CoCo BASIC
	"romlist coco=bas13,bas12,bas11,bas10",
	"romlist coco_ext=extbas11,extbas10",
	// Specific CoCo BASIC
	"romlist coco1=bas10,@coco",
	"romlist coco1e=bas11,@coco",
	"romlist coco1e_ext=extbas10,@coco_ext",
	"romlist coco2=bas12,@coco",
	"romlist coco2_ext=extbas11,@coco_ext",
	"romlist coco2b=bas13,@coco",
	// MX-1600 and zephyr-patched version
	"romlist mx1600=mx1600bas,mx1600bas_zephyr",
	"romlist mx1600ext=mx1600extbas",
	// DragonDOS
	"romlist dragondos=ddos40,ddos15,ddos10,Dragon Data Ltd - DragonDOS 1.0",
	"romlist dosplus=dplus49b,dplus48,dosplus-4.8,DOSPLUS",
	"romlist superdos=sdose6,PNP - SuperDOS E6,sdose5,sdose4",
	"romlist cumana=cdos20,CDOS20",
	"romlist dragondos_compat=@dosplus,@superdos,@dragondos,@cumana",
	// RSDOS
	"romlist rsdos=disk11,disk10",
	// Delta
	"romlist delta=delta,deltados,Premier Micros - DeltaDOS",
	// RSDOS with becker port
	"romlist rsdos_becker=hdbdw3bck",

	// CRC lists

	// Dragon BASIC
	"crclist d64_1=0x84f68bf9,0x60a4634c,@woolham_d64_1",
	"crclist d64_2=0x17893a42,@woolham_d64_2",
	"crclist d32=0xe3879310,@woolham_d32",
	"crclist dragon=@d64_1,@d32",
	"crclist woolham_d64_1=0xee33ae92",
	"crclist woolham_d64_2=0x1660ae35",
	"crclist woolham_d32=0xff7bf41e,0x9c7eed69",
	// CoCo BASIC
	"crclist bas10=0x00b50aaa",
	"crclist bas11=0x6270955a",
	"crclist bas12=0x54368805",
	"crclist bas13=0xd8f4d15e",
	"crclist mx1600=0xd918156e,0xd11b1c96",  // 2nd is zephyr-patched
	"crclist coco=@bas13,@bas12,@bas11,@bas10,@mx1600",
	"crclist extbas10=0xe031d076,0x6111a086",  // 2nd is corrupt dump
	"crclist extbas11=0xa82a6254",
	"crclist mx1600ext=0x322a3d58",
	"crclist cocoext=@extbas11,@extbas10,@mx1600ext",
	"crclist coco_combined=@mx1600",

	// Joysticks
	"joy joy0",
	"joy-desc Physical joystick 0",
	"joy-axis 0=physical:0,0",
	"joy-axis 1=physical:0,1",
	"joy-button 0=physical:0,0",
	"joy-button 1=physical:0,1",
	"joy joy1",
	"joy-desc Physical joystick 1",
	"joy-axis 0=physical:1,0",
	"joy-axis 1=physical:1,1",
	"joy-button 0=physical:1,0",
	"joy-button 1=physical:1,1",
	"joy kjoy0",
	"joy-desc Virtual joystick 0",
	"joy-axis 0=keyboard:",
	"joy-axis 1=keyboard:",
	"joy-button 0=keyboard:",
	"joy-button 1=keyboard:",
	"joy mjoy0",
	"joy-desc Mouse-joystick 0",
	"joy-axis 0=mouse:",
	"joy-axis 1=mouse:",
	"joy-button 0=mouse:",
	"joy-button 1=mouse:",
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

const char *xroar_conf_path = NULL;
const char *xroar_rom_path = NULL;

struct event *xroar_ui_events = NULL;
struct event *xroar_machine_events = NULL;

static struct event load_file_event;
static void do_load_file(void *);
//static char *load_file = NULL;
static int autorun_loaded_file = 0;

static struct event timeout_event;
static void handle_timeout_event(void *);

const char *xroar_disk_exts[] = { "DMK", "JVC", "VDK", "DSK", NULL };
const char *xroar_tape_exts[] = { "CAS", NULL };
const char *xroar_snap_exts[] = { "SNA", NULL };
const char *xroar_cart_exts[] = { "ROM", NULL };

static struct {
	const char *ext;
	int filetype;
} filetypes[] = {
	{ "VDK", FILETYPE_VDK },
	{ "JVC", FILETYPE_JVC },
	{ "DSK", FILETYPE_JVC },
	{ "DMK", FILETYPE_DMK },
	{ "BIN", FILETYPE_BIN },
	{ "HEX", FILETYPE_HEX },
	{ "CAS", FILETYPE_CAS },
	{ "WAV", FILETYPE_WAV },
	{ "SN",  FILETYPE_SNA },
	{ "ROM", FILETYPE_ROM },
	{ "CCC", FILETYPE_ROM },
	{ "BAS", FILETYPE_ASC },
	{ "ASC", FILETYPE_ASC },
	{ NULL, FILETYPE_UNKNOWN }
};

void (*xroar_fullscreen_changed_cb)(_Bool fullscreen) = NULL;
void (*xroar_kbd_translate_changed_cb)(_Bool kbd_translate) = NULL;
static struct vdg_palette *get_machine_palette(void);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

#ifdef WANT_GDB_TARGET
static pthread_cond_t run_state_cv;
static pthread_mutex_t run_state_mt;
#endif

enum xroar_run_state xroar_run_state = xroar_run_state_running;
int xroar_last_signal;

/**************************************************************************/

#ifndef ROMPATH
# define ROMPATH "."
#endif
#ifndef CONFPATH
# define CONFPATH "."
#endif

_Bool xroar_init(int argc, char **argv) {
	int argn = 1, ret;
	char *conffile = NULL;

	fprintf(stderr, "XRoar " VERSION "\n");

	// If the very first argument is -c, override conffile.
	if ((argn + 1) <= argc && 0 == strcmp(argv[argn], "-c")) {
		conffile = g_strdup(argv[argn+1]);
		argn += 2;
	}

#ifdef WINDOWS32
	windows32_init();
#endif

	xroar_conf_path = getenv("XROAR_CONF_PATH");
	if (!xroar_conf_path)
		xroar_conf_path = CONFPATH;

	for (unsigned i = 0; i < JOYSTICK_NUM_AXES; i++)
		private_cfg.joy_axis[i] = NULL;
	for (unsigned i = 0; i < JOYSTICK_NUM_BUTTONS; i++)
		private_cfg.joy_button[i] = NULL;

	// Default configuration.
	for (unsigned i = 0; i < G_N_ELEMENTS(default_config); i++) {
		xconfig_parse_line(xroar_options, default_config[i]);
	}
	// Finish any machine or cart config in defaults.
	set_machine(NULL);
	set_cart(NULL);
	set_joystick(NULL);
	xroar_machine_config = NULL;
	selected_cart_config = NULL;
	cur_joy_config = NULL;

	// If a configuration file is found, parse it.
	if (!conffile)
		conffile = find_in_path(xroar_conf_path, "xroar.conf");
	if (conffile) {
		(void)xconfig_parse_file(xroar_options, conffile);
		g_free(conffile);
	}
	// Finish any machine or cart config in config file.
	set_machine(NULL);
	set_cart(NULL);
	set_joystick(NULL);
	// Don't auto-select last machine or cart in config file.
	xroar_machine_config = NULL;
	selected_cart_config = NULL;
	cur_joy_config = NULL;

	// Parse command line options.
	ret = xconfig_parse_cli(xroar_options, argc, argv, &argn);
	if (ret != XCONFIG_OK) {
		exit(EXIT_FAILURE);
	}
	// Set a default ROM search path if required.
	if (!xroar_rom_path)
		xroar_rom_path = getenv("XROAR_ROM_PATH");
	if (!xroar_rom_path)
		xroar_rom_path = ROMPATH;
	// If no machine specified on command line, get default.
	if (!xroar_machine_config && private_cfg.default_machine) {
		xroar_machine_config = machine_config_by_name(private_cfg.default_machine);
	}
	// If that didn't work, just find the first one that will work.
	if (!xroar_machine_config) {
		xroar_machine_config = machine_config_first_working();
	}
	assert(xroar_machine_config != NULL);
	// Finish any machine or cart config on command line.
	set_machine(NULL);
	set_cart(NULL);
	set_joystick(NULL);

	if (private_cfg.config_print) {
		config_print_all();
		exit(EXIT_SUCCESS);
	}

	// Select a UI module.
	ui_module = (UIModule *)module_select_by_arg((struct module **)ui_module_list, private_cfg.ui);
	if (ui_module == NULL) {
		LOG_ERROR("%s: ui module `%s' not found\n", argv[0], private_cfg.ui);
		exit(EXIT_FAILURE);
	}
	// Override other module lists if UI has an entry.
	if (ui_module->filereq_module_list != NULL)
		filereq_module_list = ui_module->filereq_module_list;
	if (ui_module->video_module_list != NULL)
		video_module_list = ui_module->video_module_list;
	if (ui_module->sound_module_list != NULL)
		sound_module_list = ui_module->sound_module_list;
	if (ui_module->keyboard_module_list != NULL)
		keyboard_module_list = ui_module->keyboard_module_list;
	/*
	if (ui_module->joystick_module_list != NULL)
		joystick_module_list = ui_module->joystick_module_list;
	*/
	// Select file requester, video & sound modules
	filereq_module = (FileReqModule *)module_select_by_arg((struct module **)filereq_module_list, private_cfg.filereq);
	video_module = (VideoModule *)module_select_by_arg((struct module **)video_module_list, private_cfg.vo);
	sound_module = (SoundModule *)module_select_by_arg((struct module **)sound_module_list, private_cfg.ao);
	// Keyboard & joystick modules
	keyboard_module = NULL;
	joystick_module = NULL;

	/* Check other command-line options */
	if (xroar_cfg.frameskip < 0)
		xroar_cfg.frameskip = 0;
	xroar_frameskip = xroar_cfg.frameskip;

	// Remaining command line arguments are files.
	while (argn < argc) {
		if ((argn+1) < argc) {
			add_load(argv[argn]);
		} else {
			// Autorun last file given.
			private_cfg.run = argv[argn];
		}
		argn++;
	}
	if (private_cfg.run) {
		add_load(private_cfg.run);
	}

	sound_set_volume(private_cfg.volume);
	/* turn off tape_pad_auto if any tape_pad specified */
	if (private_cfg.tape_pad >= 0)
		private_cfg.tape_pad_auto = 0;
	private_cfg.tape_fast = private_cfg.tape_fast ? TAPE_FAST : 0;
	private_cfg.tape_pad = (private_cfg.tape_pad > 0) ? TAPE_PAD : 0;
	private_cfg.tape_pad_auto = private_cfg.tape_pad_auto ? TAPE_PAD_AUTO : 0;
	private_cfg.tape_rewrite = private_cfg.tape_rewrite ? TAPE_REWRITE : 0;

	_Bool no_auto_dos = xroar_machine_config->nodos;
	_Bool definitely_dos = 0;
	for (GSList *tmp_list = private_cfg.load_list; tmp_list; tmp_list = tmp_list->next) {
		char *load_file = tmp_list->data;
		int load_file_type = xroar_filetype_by_ext(load_file);
		_Bool autorun = (load_file == private_cfg.run);
		switch (load_file_type) {
		// tapes - flag that DOS shouldn't be automatically found
		case FILETYPE_CAS:
		case FILETYPE_WAV:
		case FILETYPE_ASC:
		case FILETYPE_UNKNOWN:
			no_auto_dos = 1;
			break;
		// disks - flag that DOS should *definitely* be attempted
		case FILETYPE_VDK:
		case FILETYPE_JVC:
		case FILETYPE_DMK:
			// unless explicitly disabled
			if (!xroar_machine_config->nodos)
				definitely_dos = 1;
			break;
		// for cartridge ROMs, create a cart as machine default
		case FILETYPE_ROM:
			selected_cart_config = cart_config_by_name(load_file);
			selected_cart_config->autorun = autorun;
			break;
		// for the rest, wait until later
		default:
			break;
		}
	}
	if (definitely_dos) no_auto_dos = 0;

	// Disable cart if necessary.
	if (!selected_cart_config && no_auto_dos) {
		xroar_machine_config->cart_enabled = 0;
	}
	// If any cart still configured, make it default for machine.
	if (selected_cart_config) {
		if (xroar_machine_config->default_cart)
			g_free(xroar_machine_config->default_cart);
		xroar_machine_config->default_cart = g_strdup(selected_cart_config->name);
	}

	/* Initial palette */
	xroar_vdg_palette = get_machine_palette();

	/* Initialise everything */
	event_current_tick = 0;
	/* ... modules */
	module_init((struct module *)ui_module);
	filereq_module = (FileReqModule *)module_init_from_list((struct module **)filereq_module_list, (struct module *)filereq_module);
	if (filereq_module == NULL && filereq_module_list != NULL) {
		LOG_WARN("No file requester module initialised.\n");
	}
	video_module = (VideoModule *)module_init_from_list((struct module **)video_module_list, (struct module *)video_module);
	if (video_module == NULL && video_module_list != NULL) {
		LOG_ERROR("No video module initialised.\n");
		return 0;
	}
	sound_module = (SoundModule *)module_init_from_list((struct module **)sound_module_list, (struct module *)sound_module);
	if (sound_module == NULL && sound_module_list != NULL) {
		LOG_ERROR("No sound module initialised.\n");
		return 0;
	}
	keyboard_module = (KeyboardModule *)module_init_from_list((struct module **)keyboard_module_list, (struct module *)keyboard_module);
	if (keyboard_module == NULL && keyboard_module_list != NULL) {
		LOG_WARN("No keyboard module initialised.\n");
	}
	joystick_module = (JoystickModule *)module_init_from_list((struct module **)joystick_module_list, (struct module *)joystick_module);
	if (joystick_module == NULL && joystick_module_list != NULL) {
		LOG_WARN("No joystick module initialised.\n");
	}
	/* ... subsystems */
	keyboard_init();
	joystick_init();
	machine_init();
	printer_init();

	// Default joystick mapping
	if (private_cfg.joy_right) {
		joystick_map(joystick_config_by_name(private_cfg.joy_right), 0);
	} else {
		joystick_map(joystick_config_by_name("joy0"), 0);
	}
	if (private_cfg.joy_left) {
		joystick_map(joystick_config_by_name(private_cfg.joy_left), 1);
	} else {
		joystick_map(joystick_config_by_name("joy1"), 1);
	}
	if (private_cfg.joy_virtual) {
		joystick_set_virtual(joystick_config_by_name(private_cfg.joy_virtual));
	} else {
		joystick_set_virtual(joystick_config_by_name("kjoy0"));
	}

	/* Notify UI of starting options: */
	xroar_set_kbd_translate(xroar_cfg.kbd_translate);

	/* Configure machine */
	machine_configure(xroar_machine_config);
	if (xroar_machine_config->cart_enabled) {
		xroar_set_cart(xroar_machine_config->default_cart);
	} else {
		xroar_set_cart(NULL);
	}
	/* Reset everything */
	xroar_hard_reset();
	tape_select_state(private_cfg.tape_fast | private_cfg.tape_pad | private_cfg.tape_pad_auto | private_cfg.tape_rewrite);

	load_disk_to_drive = 0;
	while (private_cfg.load_list) {
		char *load_file = private_cfg.load_list->data;
		int load_file_type = xroar_filetype_by_ext(load_file);
		// inhibit autorun if a -type option was given
		_Bool autorun = !private_cfg.type_list && (load_file == private_cfg.run);
		switch (load_file_type) {
		// cart will already be loaded (will autorun even with -type)
		case FILETYPE_ROM:
			break;
		// delay loading binary files by 2s
		case FILETYPE_BIN:
		case FILETYPE_HEX:
			event_init(&load_file_event, do_load_file, load_file);
			load_file_event.at_tick = event_current_tick + OSCILLATOR_RATE * 2;
			event_queue(&UI_EVENT_LIST, &load_file_event);
			autorun_loaded_file = autorun;
			break;
		// load disks then advice drive number
		case FILETYPE_VDK:
		case FILETYPE_JVC:
		case FILETYPE_DMK:
			xroar_load_file_by_type(load_file, autorun);
			load_disk_to_drive++;
			if (load_disk_to_drive > 3)
				load_disk_to_drive = 3;
			break;
		// the rest can be loaded straight off
		default:
			xroar_load_file_by_type(load_file, autorun);
			break;
		}
		private_cfg.load_list = g_slist_remove(private_cfg.load_list, private_cfg.load_list->data);
	}
	load_disk_to_drive = 0;

	if (private_cfg.tape_write) {
		int write_file_type = xroar_filetype_by_ext(private_cfg.tape_write);
		switch (write_file_type) {
			case FILETYPE_CAS:
			case FILETYPE_WAV:
				tape_open_writing(private_cfg.tape_write);
				if (ui_module->output_tape_filename_cb) {
					ui_module->output_tape_filename_cb(private_cfg.tape_write);
				}
				break;
			default:
				break;
		}
	}

	xroar_set_trace(xroar_cfg.trace_enabled);

#ifdef WANT_GDB_TARGET
	pthread_mutex_init(&run_state_mt, NULL);
	pthread_cond_init(&run_state_cv, NULL);
	if (private_cfg.gdb)
		gdb_init();
#endif

	if (private_cfg.timeout) {
		double t = strtod(private_cfg.timeout, NULL);
		if (t >= 0.0) {
			timeout_seconds = (int)t;
			timeout_cycles = OSCILLATOR_RATE * (t - timeout_seconds);
			event_init(&timeout_event, handle_timeout_event, NULL);
			/* handler can set up the first call for us... */
			timeout_seconds++;
			handle_timeout_event(NULL);
		}
	}

	while (private_cfg.type_list) {
		keyboard_queue_basic(private_cfg.type_list->data);
		private_cfg.type_list = g_slist_remove(private_cfg.type_list, private_cfg.type_list->data);
	}
	if (private_cfg.lp_file) {
		printer_open_file(private_cfg.lp_file);
	} else if (private_cfg.lp_pipe) {
		printer_open_pipe(private_cfg.lp_pipe);
	}
	return 1;
}

void xroar_shutdown(void) {
	static _Bool shutting_down = 0;
	if (shutting_down)
		return;
	shutting_down = 1;
#ifdef WANT_GDB_TARGET
	if (private_cfg.gdb)
		gdb_shutdown();
	pthread_mutex_destroy(&run_state_mt);
	pthread_cond_destroy(&run_state_cv);
#endif
	machine_shutdown();
	module_shutdown((struct module *)joystick_module);
	module_shutdown((struct module *)keyboard_module);
	module_shutdown((struct module *)sound_module);
	module_shutdown((struct module *)video_module);
	module_shutdown((struct module *)filereq_module);
	module_shutdown((struct module *)ui_module);
#ifdef WINDOWS32
	windows32_shutdown();
#endif
}

static struct vdg_palette *get_machine_palette(void) {
	struct vdg_palette *vp;
	vp = vdg_palette_by_name(xroar_machine_config->vdg_palette);
	if (!vp) {
		vp = vdg_palette_by_name("ideal");
		if (!vp) {
			vp = vdg_palette_index(0);
		}
	}
	return vp;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

/*
 * Called either by main() in a loop, or by a UI module's run() member.
 * Returns 1 for as long as the machine is active.
 */

_Bool xroar_run(void) {

#ifdef WANT_GDB_TARGET
	pthread_mutex_lock(&run_state_mt);
	if (xroar_run_state == xroar_run_state_stopped) {
		struct timeval tv;
		gettimeofday(&tv, NULL);
		tv.tv_usec += 20000;
		tv.tv_sec += (tv.tv_usec / 1000000);
		tv.tv_usec %= 1000000;
		struct timespec ts;
		ts.tv_sec = tv.tv_sec;
		ts.tv_nsec = tv.tv_usec * 1000;
		if (pthread_cond_timedwait(&run_state_cv, &run_state_mt, &ts) == ETIMEDOUT) {
			if (video_module->refresh)
				video_module->refresh();
			pthread_mutex_unlock(&run_state_mt);
			return 1;
		}
	}
	if (xroar_run_state == xroar_run_state_running) {
#endif

		int sig = machine_run(VDG_LINE_DURATION * 32);
		(void)sig;

#ifdef WANT_GDB_TARGET
		if (sig != 0) {
			xroar_run_state = xroar_run_state_stopped;
			gdb_handle_signal(sig);
		}
	} else if (xroar_run_state == xroar_run_state_single_step) {
		machine_single_step();
		xroar_run_state = xroar_run_state_stopped;
		gdb_handle_signal(XROAR_SIGTRAP);
		pthread_cond_signal(&run_state_cv);
	}
	pthread_mutex_unlock(&run_state_mt);
#endif

	event_run_queue(&UI_EVENT_LIST);
	return 1;
}

#ifdef WANT_GDB_TARGET
void xroar_machine_continue(void) {
	pthread_mutex_lock(&run_state_mt);
	if (xroar_run_state == xroar_run_state_stopped) {
		xroar_run_state = xroar_run_state_running;
		pthread_cond_signal(&run_state_cv);
	}
	pthread_mutex_unlock(&run_state_mt);
}

void xroar_machine_signal(int sig) {
	pthread_mutex_lock(&run_state_mt);
	if (xroar_run_state == xroar_run_state_running) {
		machine_signal(sig);
		xroar_run_state = xroar_run_state_stopped;
		gdb_handle_signal(sig);
	}
	pthread_mutex_unlock(&run_state_mt);
}

void xroar_machine_single_step(void) {
	pthread_mutex_lock(&run_state_mt);
	if (xroar_run_state == xroar_run_state_stopped) {
		xroar_run_state = xroar_run_state_single_step;
		pthread_cond_wait(&run_state_cv, &run_state_mt);
	}
	pthread_mutex_unlock(&run_state_mt);
}
#endif  /* WANT_GDB_TARGET */

void xroar_machine_trap(void *data) {
	(void)data;
	machine_signal(XROAR_SIGTRAP);
}

int xroar_filetype_by_ext(const char *filename) {
	char *ext;
	int i;
	ext = strrchr(filename, '.');
	if (ext == NULL)
		return FILETYPE_UNKNOWN;
	ext++;
	for (i = 0; filetypes[i].ext; i++) {
		if (!g_ascii_strncasecmp(ext, filetypes[i].ext, strlen(filetypes[i].ext)))
			return filetypes[i].filetype;
	}
	return FILETYPE_UNKNOWN;
}

int xroar_load_file_by_type(const char *filename, int autorun) {
	int filetype;
	if (filename == NULL)
		return 1;
	int ret;
	filetype = xroar_filetype_by_ext(filename);
	switch (filetype) {
		case FILETYPE_VDK:
		case FILETYPE_JVC:
		case FILETYPE_DMK:
			xroar_insert_disk_file(load_disk_to_drive, filename);
			if (autorun && vdrive_disk_in_drive(0)) {
				if (IS_DRAGON) {
					keyboard_queue_basic("\033BOOT\r");
				} else {
					keyboard_queue_basic("\033DOS\r");
				}
				return 0;
			}
			return 1;
		case FILETYPE_BIN:
			return bin_load(filename, autorun);
		case FILETYPE_HEX:
			return intel_hex_read(filename, autorun);
		case FILETYPE_SNA:
			return read_snapshot(filename);
		case FILETYPE_ROM: {
			struct cart_config *cc;
			machine_remove_cart();
			cc = cart_config_by_name(filename);
			if (cc) {
				cc->autorun = autorun;
				xroar_set_cart(cc->name);
				if (autorun) {
					xroar_hard_reset();
				}
			}
			}
			return 0;
		case FILETYPE_CAS:
		case FILETYPE_ASC:
		case FILETYPE_WAV:
		default:
			if (autorun) {
				ret = tape_autorun(filename);
			} else {
				ret = tape_open_reading(filename);
			}
			if (ret == 0 && ui_module->input_tape_filename_cb) {
				ui_module->input_tape_filename_cb(filename);
			}
			break;
	}
	return ret;
}

static void do_load_file(void *data) {
	char *load_file = data;
	xroar_load_file_by_type(load_file, autorun_loaded_file);
}

static void handle_timeout_event(void *dptr) {
	(void)dptr;
	if (timeout_seconds == 0) {
		xroar_quit();
		return;
	}
	timeout_seconds--;
	if (timeout_seconds) {
		timeout_event.at_tick = event_current_tick + OSCILLATOR_RATE;
	} else {
		if (timeout_cycles == 0) {
			xroar_quit();
			return;
		}
		timeout_event.at_tick = event_current_tick + timeout_cycles;
	}
	event_queue(&MACHINE_EVENT_LIST, &timeout_event);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

/* Helper functions */

void xroar_set_trace(int mode) {
#ifdef TRACE
	int set_to;
	switch (mode) {
		case XROAR_OFF: default:
			set_to = 0;
			break;
		case XROAR_ON:
			set_to = 1;
			break;
		case XROAR_TOGGLE: case XROAR_CYCLE:
			set_to = !xroar_cfg.trace_enabled;
			break;
	}
	xroar_cfg.trace_enabled = set_to;
	machine_set_trace(xroar_cfg.trace_enabled);
	struct MC6809 *cpu = machine_get_cpu(0);
	if (xroar_cfg.trace_enabled) {
		switch (xroar_machine_config->cpu) {
		case CPU_MC6809: default:
			cpu->interrupt_hook = mc6809_trace_irq;
			break;
		case CPU_HD6309:
			cpu->interrupt_hook = hd6309_trace_irq;
			break;
		}
	} else {
		cpu->interrupt_hook = NULL;
	}
#else
	cpu->interrupt_hook = NULL;
#endif
}

void xroar_new_disk(int drive) {
	char *filename = filereq_module->save_filename(xroar_disk_exts);

	if (filename == NULL)
		return;
	int filetype = xroar_filetype_by_ext(filename);
	xroar_eject_disk(drive);
	// Default to 34T 1H.  Will be auto-expanded as necessary.
	struct vdisk *new_disk = vdisk_blank_disk(34, 1, VDISK_LENGTH_5_25);
	if (new_disk == NULL)
		return;
	LOG_DEBUG(4, "Creating blank disk in drive %d\n", 1 + drive);
	switch (filetype) {
		case FILETYPE_VDK:
		case FILETYPE_JVC:
		case FILETYPE_DMK:
			break;
		default:
			filetype = FILETYPE_DMK;
			break;
	}
	new_disk->filetype = filetype;
	new_disk->filename = g_strdup(filename);
	new_disk->write_back = 1;
	vdrive_insert_disk(drive, new_disk);
	if (ui_module && ui_module->update_drive_disk) {
		ui_module->update_drive_disk(drive, new_disk);
	}
}

void xroar_insert_disk_file(int drive, const char *filename) {
	if (!filename) return;
	struct vdisk *disk = vdisk_load(filename);
	vdrive_insert_disk(drive, disk);
	if (ui_module && ui_module->update_drive_disk) {
		ui_module->update_drive_disk(drive, disk);
	}
}

void xroar_insert_disk(int drive) {
	char *filename = filereq_module->load_filename(xroar_disk_exts);
	xroar_insert_disk_file(drive, filename);
}

void xroar_eject_disk(int drive) {
	vdrive_eject_disk(drive);
	if (ui_module && ui_module->update_drive_disk) {
		ui_module->update_drive_disk(drive, NULL);
	}
}

_Bool xroar_set_write_enable(int drive, int action) {
	_Bool we = vdrive_set_write_enable(drive, action);
	if (we) {
		LOG_DEBUG(2, "Disk in drive %d write enabled.\n", drive);
	} else {
		LOG_DEBUG(2, "Disk in drive %d write protected.\n", drive);
	}
	return we;
}

void xroar_select_write_enable(int drive, int action) {
	_Bool we = xroar_set_write_enable(drive, action);
	if (ui_module && ui_module->update_drive_write_enable) {
		ui_module->update_drive_write_enable(drive, we);
	}
}

_Bool xroar_set_write_back(int drive, int action) {
	_Bool wb = vdrive_set_write_back(drive, action);
	if (wb) {
		LOG_DEBUG(2, "Write back enabled for disk in drive %d.\n", drive);
	} else {
		LOG_DEBUG(2, "Write back disabled for disk in drive %d.\n", drive);
	}
	return wb;
}

void xroar_select_write_back(int drive, int action) {
	_Bool wb = xroar_set_write_back(drive, action);
	if (ui_module && ui_module->update_drive_write_back) {
		ui_module->update_drive_write_back(drive, wb);
	}
}

void xroar_set_cross_colour(int action) {
	switch (action) {
	case XROAR_CYCLE:
		xroar_machine_config->cross_colour_phase++;
		xroar_machine_config->cross_colour_phase %= NUM_CROSS_COLOUR_PHASES;
		break;
	default:
		xroar_machine_config->cross_colour_phase = action;
		break;
	}
	if (video_module->update_cross_colour_phase) {
		video_module->update_cross_colour_phase();
	}
}

void xroar_select_cross_colour(int action) {
	xroar_set_cross_colour(action);
	if (ui_module->cross_colour_changed_cb) {
		ui_module->cross_colour_changed_cb(xroar_machine_config->cross_colour_phase);
	}
}

void xroar_quit(void) {
	xroar_shutdown();
	exit(EXIT_SUCCESS);
}

void xroar_fullscreen(int action) {
	static int lock = 0;
	if (lock) return;
	lock = 1;
	_Bool set_to;
	switch (action) {
		case XROAR_OFF:
			set_to = 0;
			break;
		case XROAR_ON:
			set_to = 1;
			break;
		case XROAR_TOGGLE:
		default:
			set_to = !video_module->is_fullscreen;
			break;
	}
	video_module->set_fullscreen(set_to);
	if (xroar_fullscreen_changed_cb) {
		xroar_fullscreen_changed_cb(set_to);
	}
	lock = 0;
}

void xroar_load_file(const char **exts) {
	char *filename = filereq_module->load_filename(exts);
	if (filename) {
		xroar_load_file_by_type(filename, 0);
	}
}

void xroar_run_file(const char **exts) {
	char *filename = filereq_module->load_filename(exts);
	if (filename) {
		xroar_load_file_by_type(filename, 1);
	}
}

void xroar_set_keymap(int map) {
	static int lock = 0;
	if (lock) return;
	lock = 1;
	int new;
	switch (map) {
		case XROAR_CYCLE:
			new = (xroar_machine_config->keymap + 1) % NUM_KEYMAPS;
			break;
		default:
			new = map;
			break;
	}
	if (new >= 0 && new < NUM_KEYMAPS) {
		keyboard_set_keymap(new);
		if (ui_module->keymap_changed_cb) {
			ui_module->keymap_changed_cb(new);
		}
	}
	lock = 0;
}

void xroar_set_kbd_translate(int kbd_translate) {
	static int lock = 0;
	if (lock) return;
	lock = 1;
	switch (kbd_translate) {
		case XROAR_TOGGLE: case XROAR_CYCLE:
			xroar_cfg.kbd_translate = !xroar_cfg.kbd_translate;
			break;
		default:
			xroar_cfg.kbd_translate = kbd_translate;
			break;
	}
	if (xroar_kbd_translate_changed_cb) {
		xroar_kbd_translate_changed_cb(xroar_cfg.kbd_translate);
	}
	if (keyboard_module->update_kbd_translate) {
		keyboard_module->update_kbd_translate();
	}
	lock = 0;
}

void xroar_set_machine(int machine_type) {
	static int lock = 0;
	if (lock) return;
	lock = 1;
	int new = xroar_machine_config->index;
	int num_machine_types = machine_config_count();
	switch (machine_type) {
		case XROAR_TOGGLE:
			break;
		case XROAR_CYCLE:
			new = (new + 1) % num_machine_types;
			break;
		default:
			new = machine_type;
			break;
	}
	if (new >= 0 && new < num_machine_types) {
		machine_remove_cart();
		xroar_machine_config = machine_config_index(new);
		machine_configure(xroar_machine_config);
		if (xroar_machine_config->cart_enabled) {
			xroar_set_cart(xroar_machine_config->default_cart);
		} else {
			xroar_set_cart(NULL);
		}
		xroar_vdg_palette = get_machine_palette();
		if (video_module->update_palette) {
			video_module->update_palette();
		}
		xroar_hard_reset();
		if (ui_module->machine_changed_cb) {
			ui_module->machine_changed_cb(new);
		}
	}
	lock = 0;
}

void xroar_toggle_cart(void) {
	assert(xroar_machine_config != NULL);
	xroar_machine_config->cart_enabled = !xroar_machine_config->cart_enabled;
	if (xroar_machine_config->cart_enabled) {
		xroar_set_cart(xroar_machine_config->default_cart);
	} else {
		xroar_set_cart(NULL);
	}
}

void xroar_set_cart(const char *cc_name) {
	static int lock = 0;
	if (lock) return;
	lock = 1;

	assert(xroar_machine_config != NULL);
	machine_remove_cart();

	if (!cc_name) {
		xroar_machine_config->cart_enabled = 0;
	} else {
		if (xroar_machine_config->default_cart != cc_name) {
			g_free(xroar_machine_config->default_cart);
			xroar_machine_config->default_cart = g_strdup(cc_name);
		}
		xroar_machine_config->cart_enabled = 1;
		machine_insert_cart(cart_new_named(cc_name));
	}

	if (ui_module->cart_changed_cb) {
		if (machine_cart) {
			ui_module->cart_changed_cb(machine_cart->config->index);
		} else {
			ui_module->cart_changed_cb(-1);
		}
	}
	lock = 0;
}

/* For old snapshots */
void xroar_set_dos(int dos_type) {
	switch (dos_type) {
	case DOS_DRAGONDOS:
		xroar_set_cart("dragondos");
		break;
	case DOS_RSDOS:
		xroar_set_cart("rsdos");
		break;
	case DOS_DELTADOS:
		xroar_set_cart("delta");
		break;
	default:
		break;
	}
}

void xroar_save_snapshot(void) {
	char *filename = filereq_module->save_filename(xroar_snap_exts);
	if (filename) {
		write_snapshot(filename);
	}
}

void xroar_select_tape_input(void) {
	char *filename = filereq_module->load_filename(xroar_tape_exts);
	if (filename) {
		tape_open_reading(filename);
		if (ui_module->input_tape_filename_cb) {
			ui_module->input_tape_filename_cb(filename);
		}
	}
}

void xroar_eject_tape_input(void) {
	tape_close_reading();
	if (ui_module->input_tape_filename_cb) {
		ui_module->input_tape_filename_cb(NULL);
	}
}

void xroar_select_tape_output(void) {
	char *filename = filereq_module->save_filename(xroar_tape_exts);
	if (filename) {
		tape_open_writing(filename);
		if (ui_module->output_tape_filename_cb) {
			ui_module->output_tape_filename_cb(filename);
		}
	}
}

void xroar_eject_tape_output(void) {
	tape_close_writing();
	if (ui_module->output_tape_filename_cb) {
		ui_module->output_tape_filename_cb(NULL);
	}
}

void xroar_soft_reset(void) {
	machine_reset(RESET_SOFT);
}

void xroar_hard_reset(void) {
	printer_reset();
	machine_reset(RESET_HARD);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

/* Helper functions used by configuration */

static void set_pal(void) {
	private_cfg.tv = TV_PAL;
}

static void set_ntsc(void) {
	private_cfg.tv = TV_NTSC;
}

/* Called when a "-machine" option is encountered.  If an existing machine
 * config was in progress, copies any machine-related options into it and
 * clears those options.  Starts a new config. */
static void set_machine(const char *name) {
#ifdef LOGGING
	if (name && 0 == strcmp(name, "help")) {
		int count = machine_config_count();
		int i;
		for (i = 0; i < count; i++) {
			struct machine_config *mc = machine_config_index(i);
			printf("\t%-10s %s\n", mc->name, mc->description);
		}
		exit(EXIT_SUCCESS);
	}
#endif

	if (xroar_machine_config) {
		if (private_cfg.machine_arch != ANY_AUTO) {
			xroar_machine_config->architecture = private_cfg.machine_arch;
			private_cfg.machine_arch = ANY_AUTO;
		}
		xroar_machine_config->cpu = private_cfg.machine_cpu;
		if (private_cfg.machine_cpu == CPU_HD6309) {
			LOG_WARN("Hitachi HD6309 support is UNVERIFIED!\n");
		}
		if (private_cfg.machine_desc) {
			xroar_machine_config->description = private_cfg.machine_desc;
			private_cfg.machine_desc = NULL;
		}
#ifdef LOGGING
		if (private_cfg.machine_palette && 0 == strcmp(private_cfg.machine_palette, "help")) {
			int count = vdg_palette_count();
			int i;
			for (i = 0; i < count; i++) {
				struct vdg_palette *vp = vdg_palette_index(i);
				printf("\t%-10s %s\n", vp->name, vp->description);
			}
			exit(EXIT_SUCCESS);
		}
#endif
		if (private_cfg.machine_palette) {
			xroar_machine_config->vdg_palette = private_cfg.machine_palette;
			private_cfg.machine_palette = NULL;
		}
		if (private_cfg.tv != ANY_AUTO) {
			xroar_machine_config->tv_standard = private_cfg.tv;
			private_cfg.tv = ANY_AUTO;
		}
		if (private_cfg.vdg_type != -1) {
			xroar_machine_config->vdg_type = private_cfg.vdg_type;
			private_cfg.vdg_type = -1;
		}
		if (private_cfg.ram > 0) {
			xroar_machine_config->ram = private_cfg.ram;
			private_cfg.ram = 0;
		}
		if (private_cfg.nobas != -1)
			xroar_machine_config->nobas = private_cfg.nobas;
		if (private_cfg.noextbas != -1)
			xroar_machine_config->noextbas = private_cfg.noextbas;
		if (private_cfg.noaltbas != -1)
			xroar_machine_config->noaltbas = private_cfg.noaltbas;
		private_cfg.nobas = private_cfg.noextbas = private_cfg.noaltbas = -1;
		if (private_cfg.bas) {
			xroar_machine_config->bas_rom = private_cfg.bas;
			xroar_machine_config->nobas = 0;
			private_cfg.bas = NULL;
		}
		if (private_cfg.extbas) {
			xroar_machine_config->extbas_rom = private_cfg.extbas;
			xroar_machine_config->noextbas = 0;
			private_cfg.extbas = NULL;
		}
		if (private_cfg.altbas) {
			xroar_machine_config->altbas_rom = private_cfg.altbas;
			xroar_machine_config->noaltbas = 0;
			private_cfg.altbas = NULL;
		}
		if (private_cfg.machine_cart) {
			if (xroar_machine_config->default_cart)
				g_free(xroar_machine_config->default_cart);
			xroar_machine_config->default_cart = g_strdup(private_cfg.machine_cart);
			private_cfg.machine_cart = NULL;
		}
		if (private_cfg.nodos != -1) {
			xroar_machine_config->nodos = private_cfg.nodos;
			private_cfg.nodos = -1;
		}
		machine_config_complete(xroar_machine_config);
	}
	if (name) {
		xroar_machine_config = machine_config_by_name(name);
		if (!xroar_machine_config) {
			xroar_machine_config = machine_config_new();
			xroar_machine_config->name = g_strdup(name);
		}
	}
}

/* Called when a "-cart" option is encountered.  If an existing cart config was
* in progress, copies any cart-related options into it and clears those
* options.  Starts a new config.  */
static void set_cart(const char *name) {
#ifdef LOGGING
	if (name && 0 == strcmp(name, "help")) {
		int count = cart_config_count();
		int i;
		for (i = 0; i < count; i++) {
			struct cart_config *cc = cart_config_index(i);
			printf("\t%-10s %s\n", cc->name, cc->description);
		}
		exit(EXIT_SUCCESS);
	}
#endif
	// Apply any unassigned config to either the current cart config or the
	// current machine's default cart config.
	struct cart_config *cc = NULL;
	if (selected_cart_config) {
		cc = selected_cart_config;
	} else if (xroar_machine_config) {
		cc = cart_config_by_name(xroar_machine_config->default_cart);
	}
	if (cc) {
		if (private_cfg.cart_desc) {
			cc->description = private_cfg.cart_desc;
			private_cfg.cart_desc = NULL;
		}
		if (private_cfg.cart_type != ANY_AUTO) {
			cc->type = private_cfg.cart_type;
			private_cfg.cart_type = ANY_AUTO;
		}
		if (private_cfg.cart_rom) {
			cc->rom = private_cfg.cart_rom;
			private_cfg.cart_rom = NULL;
		}
		if (private_cfg.cart_rom2) {
			cc->rom2 = private_cfg.cart_rom2;
			private_cfg.cart_rom2 = NULL;
		}
		if (private_cfg.cart_becker != ANY_AUTO) {
			cc->becker_port = private_cfg.cart_becker;
			private_cfg.cart_becker = ANY_AUTO;
		}
		if (private_cfg.cart_autorun != ANY_AUTO) {
			cc->autorun = private_cfg.cart_autorun;
			private_cfg.cart_autorun = ANY_AUTO;
		}
		cart_config_complete(cc);
	}
	if (name) {
		selected_cart_config = cart_config_by_name(name);
		if (!selected_cart_config) {
			selected_cart_config = cart_config_new();
			selected_cart_config->name = g_strdup(name);
		}
	}
}

/* Called when a "-joystick" option is encountered. */
static void set_joystick(const char *name) {
	// Apply any config to the current joystick config.
	if (cur_joy_config) {
		if (private_cfg.joy_desc) {
			cur_joy_config->description = private_cfg.joy_desc;
			private_cfg.joy_desc = NULL;
		}
		for (unsigned i = 0; i < JOYSTICK_NUM_AXES; i++) {
			if (private_cfg.joy_axis[i]) {
				if (cur_joy_config->axis_specs[i])
					g_free(cur_joy_config->axis_specs[i]);
				cur_joy_config->axis_specs[i] = private_cfg.joy_axis[i];
				private_cfg.joy_axis[i] = NULL;
			}
		}
		for (unsigned i = 0; i < JOYSTICK_NUM_BUTTONS; i++) {
			if (private_cfg.joy_button[i]) {
				if (cur_joy_config->button_specs[i])
					g_free(cur_joy_config->button_specs[i]);
				cur_joy_config->button_specs[i] = private_cfg.joy_button[i];
				private_cfg.joy_button[i] = NULL;
			}
		}
	}
#ifdef LOGGING
	if (name && 0 == strcmp(name, "help")) {
		int count = joystick_config_count();
		int i;
		for (i = 0; i < count; i++) {
			struct joystick_config *jc = joystick_config_index(i);
			printf("\t%-10s %s\n", jc->name, jc->description);
		}
		exit(EXIT_SUCCESS);
	}
#endif
	if (name) {
		cur_joy_config = joystick_config_by_name(name);
		if (!cur_joy_config) {
			cur_joy_config = joystick_config_new();
			cur_joy_config->name = g_strdup(name);
		}
	}
}

static void set_joystick_axis(const char *spec) {
	char *spec_copy = g_strdup(spec);
	char *cspec = spec_copy;
	unsigned axis = 0;
	char *tmp = strsep(&cspec, "=");
	if (cspec) {
		if (toupper(*tmp) == 'X') {
			axis = 0;
		} else if (toupper(*tmp) == 'Y') {
			axis = 1;
		} else {
			axis = strtol(tmp, NULL, 0);
		}
		if (axis > JOYSTICK_NUM_AXES) {
			LOG_WARN("Invalid axis number '%d'\n", axis);
			axis = 0;
		}
		tmp = cspec;
	}
	private_cfg.joy_axis[axis] = g_strdup(tmp);
	g_free(spec_copy);
}

static void set_joystick_button(const char *spec) {
	char *spec_copy = g_strdup(spec);
	char *cspec = spec_copy;
	unsigned button = 0;
	char *tmp = strsep(&cspec, "=");
	if (cspec) {
		button = strtol(tmp, NULL, 0);
		if (button > JOYSTICK_NUM_AXES) {
			LOG_WARN("Invalid button number '%d'\n", button);
			button = 0;
		}
		tmp = cspec;
	}
	private_cfg.joy_button[button] = g_strdup(tmp);
	g_free(spec_copy);
}

static void type_command(char *string) {
	private_cfg.type_list = g_slist_append(private_cfg.type_list, string);
}

static void add_load(char *string) {
	private_cfg.load_list = g_slist_append(private_cfg.load_list, string);
}

/* Enumeration lists used by configuration directives */

static struct xconfig_enum arch_list[] = {
	{ .value = ARCH_DRAGON64, .name = "dragon64", .description = "Dragon 64" },
	{ .value = ARCH_DRAGON32, .name = "dragon32", .description = "Dragon 32" },
	{ .value = ARCH_COCO, .name = "coco", .description = "Tandy CoCo" },
	XC_ENUM_END()
};

static struct xconfig_enum cpu_list[] = {
	{ .value = CPU_MC6809, .name = "6809", .description = "Motorola 6809" },
	{ .value = CPU_HD6309, .name = "6309", .description = "Hitachi 6309 - UNVERIFIED" },
	XC_ENUM_END()
};

static struct xconfig_enum tv_type_list[] = {
	{ .value = TV_PAL,  .name = "pal",  .description = "PAL (50Hz)" },
	{ .value = TV_NTSC, .name = "ntsc", .description = "NTSC (60Hz)" },
	{ .value = TV_NTSC, .name = "pal-m", .description = "PAL-M (60Hz)" },
	XC_ENUM_END()
};

static struct xconfig_enum vdg_type_list[] = {
	{ .value = VDG_6847, .name = "6847", .description = "Original 6847" },
	{ .value = VDG_6847T1, .name = "6847t1", .description = "6847T1 with lowercase" },
	XC_ENUM_END()
};

static struct xconfig_enum cart_type_list[] = {
	{ .value = CART_ROM, .name = "rom", .description = "ROM cartridge" },
	{ .value = CART_DRAGONDOS, .name = "dragondos", .description = "DragonDOS" },
	{ .value = CART_DELTADOS, .name = "delta", .description = "Delta System" },
	{ .value = CART_RSDOS, .name = "rsdos", .description = "RS-DOS" },
	{ .value = CART_ORCH90, .name = "orch90", .description = "Orchestra 90-CC" },
	XC_ENUM_END()
};

static struct xconfig_enum gl_filter_list[] = {
	{ .value = ANY_AUTO, .name = "auto", .description = "Automatic" },
	{ .value = XROAR_GL_FILTER_NEAREST, .name = "nearest", .description = "Nearest-neighbour filtering" },
	{ .value = XROAR_GL_FILTER_LINEAR, .name = "linear", .description = "Linear filter" },
	XC_ENUM_END()
};

static struct xconfig_enum ccr_list[] = {
	{ .value = CROSS_COLOUR_SIMPLE, .name = "simple", .description = "four colour palette" },
	{ .value = CROSS_COLOUR_5BIT, .name = "5bit", .description = "5-bit lookup table" },
	XC_ENUM_END()
};

struct xconfig_enum xroar_cross_colour_list[] = {
	{ .value = CROSS_COLOUR_OFF, .name = "none", .description = "None" },
	{ .value = CROSS_COLOUR_KBRW, .name = "blue-red", .description = "Blue-red" },
	{ .value = CROSS_COLOUR_KRBW, .name = "red-blue", .description = "Red-blue" },
	XC_ENUM_END()
};

/* Configuration directives */

static struct xconfig_option xroar_options[] = {
	/* Machines: */
	XC_SET_STRING("default-machine", &private_cfg.default_machine),
	XC_CALL_STRING("machine", &set_machine),
	XC_SET_STRING("machine-desc", &private_cfg.machine_desc),
	XC_SET_ENUM("machine-arch", &private_cfg.machine_arch, arch_list),
	XC_SET_ENUM("machine-cpu", &private_cfg.machine_cpu, cpu_list),
	XC_SET_STRING("bas", &private_cfg.bas),
	XC_SET_STRING("extbas", &private_cfg.extbas),
	XC_SET_STRING("altbas", &private_cfg.altbas),
	XC_SET_INT1("nobas", &private_cfg.nobas),
	XC_SET_INT1("noextbas", &private_cfg.noextbas),
	XC_SET_INT1("noaltbas", &private_cfg.noaltbas),
	XC_SET_ENUM("tv-type", &private_cfg.tv, tv_type_list),
	XC_SET_ENUM("vdg-type", &private_cfg.vdg_type, vdg_type_list),
	XC_SET_INT("ram", &private_cfg.ram),
	XC_SET_STRING("machine-cart", &private_cfg.machine_cart),
	XC_SET_INT1("nodos", &private_cfg.nodos),
	/* Deliberately undocumented: */
	XC_SET_STRING("machine-palette", &private_cfg.machine_palette),
	/* Backwards-compatibility: */
	XC_CALL_NULL("pal", &set_pal),
	XC_CALL_NULL("ntsc", &set_ntsc),

	/* Cartridges: */
	XC_CALL_STRING("cart", &set_cart),
	XC_SET_STRING("cart-desc", &private_cfg.cart_desc),
	XC_SET_ENUM("cart-type", &private_cfg.cart_type, cart_type_list),
	XC_SET_STRING("cart-rom", &private_cfg.cart_rom),
	XC_SET_STRING("cart-rom2", &private_cfg.cart_rom2),
	XC_SET_INT1("cart-autorun", &private_cfg.cart_autorun),
	XC_SET_INT1("cart-becker", &private_cfg.cart_becker),
	/* Backwards compatibility: */
	XC_SET_ENUM("dostype", &private_cfg.cart_type, cart_type_list),
	XC_SET_STRING("dos", &private_cfg.cart_rom),

	/* Becker port: */
	XC_SET_BOOL("becker", &xroar_cfg.becker),
	XC_SET_STRING("becker-ip", &xroar_cfg.becker_ip),
	XC_SET_STRING("becker-port", &xroar_cfg.becker_port),
	/* Backwards-compatibility: */
	XC_SET_STRING("dw4-ip", &xroar_cfg.becker_ip),
	XC_SET_STRING("dw4-port", &xroar_cfg.becker_port),

	/* Files: */
	XC_CALL_STRING("load", &add_load),
	XC_SET_STRING("run", &private_cfg.run),
	/* Backwards-compatibility: */
	XC_CALL_STRING("cartna", &add_load),
	XC_CALL_STRING("snap", &add_load),

	/* Cassettes: */
	XC_SET_STRING("tape-write", &private_cfg.tape_write),
	XC_SET_INT1("tape-fast", &private_cfg.tape_fast),
	XC_SET_INT1("tape-pad", &private_cfg.tape_pad),
	XC_SET_INT1("tape-pad-auto", &private_cfg.tape_pad_auto),
	XC_SET_INT1("tape-rewrite", &private_cfg.tape_rewrite),
	/* Backwards-compatibility: */
	XC_SET_INT1("tapehack", &private_cfg.tape_rewrite),

	/* Disks: */
	XC_SET_BOOL("disk-write-back", &xroar_cfg.disk_write_back),
	XC_SET_BOOL("disk-jvc-hack", &xroar_cfg.disk_jvc_hack),

	/* Firmware ROM images: */
	XC_SET_STRING("rompath", &xroar_rom_path),
	XC_CALL_STRING("romlist", &romlist_assign),
	XC_CALL_NULL("romlist-print", &romlist_print),
	XC_CALL_STRING("crclist", &crclist_assign),
	XC_CALL_NULL("crclist-print", &crclist_print),
	XC_SET_BOOL("force-crc-match", &xroar_cfg.force_crc_match),

	/* User interface: */
	XC_SET_STRING("ui", &private_cfg.ui),
	/* Deliberately undocumented: */
	XC_SET_STRING("filereq", &private_cfg.filereq),

	/* Video: */
	XC_SET_STRING("vo", &private_cfg.vo),
	XC_SET_BOOL("fs", &xroar_cfg.fullscreen),
	XC_SET_INT("fskip", &xroar_cfg.frameskip),
	XC_SET_ENUM("ccr", &xroar_cfg.ccr, ccr_list),
	XC_SET_ENUM("gl-filter", &xroar_cfg.gl_filter, gl_filter_list),
	XC_SET_STRING("geometry", &xroar_cfg.geometry),
	XC_SET_STRING("g", &xroar_cfg.geometry),

	/* Audio: */
	XC_SET_STRING("ao", &private_cfg.ao),
	XC_SET_STRING("ao-device", &xroar_cfg.ao_device),
	XC_SET_INT("ao-rate", &xroar_cfg.ao_rate),
	XC_SET_INT("ao-channels", &xroar_cfg.ao_channels),
	XC_SET_INT("ao-buffer-ms", &xroar_cfg.ao_buffer_ms),
	XC_SET_INT("ao-buffer-samples", &xroar_cfg.ao_buffer_samples),
	XC_SET_INT("volume", &private_cfg.volume),
#ifndef FAST_SOUND
	XC_SET_BOOL("fast-sound", &xroar_cfg.fast_sound),
#endif

	/* Keyboard: */
	XC_SET_STRING("keymap", &xroar_cfg.keymap),
	XC_SET_BOOL("kbd-translate", &xroar_cfg.kbd_translate),
	XC_CALL_STRING("type", &type_command),

	/* Joysticks: */
	XC_CALL_STRING("joy", &set_joystick),
	XC_SET_STRING("joy-desc", &private_cfg.joy_desc),
	XC_CALL_STRING("joy-axis", &set_joystick_axis),
	XC_CALL_STRING("joy-button", &set_joystick_button),
	XC_SET_STRING("joy-right", &private_cfg.joy_right),
	XC_SET_STRING("joy-left", &private_cfg.joy_left),
	XC_SET_STRING("joy-virtual", &private_cfg.joy_virtual),

	/* Printing: */
	XC_SET_STRING("lp-file", &private_cfg.lp_file),
	XC_SET_STRING("lp-pipe", &private_cfg.lp_pipe),

	/* Debugging: */
#ifdef WANT_GDB_TARGET
	XC_SET_BOOL("gdb", &private_cfg.gdb),
	XC_SET_STRING("gdb-ip", &xroar_cfg.gdb_ip),
	XC_SET_STRING("gdb-port", &xroar_cfg.gdb_port),
#endif
#ifdef TRACE
	XC_SET_INT1("trace", &xroar_cfg.trace_enabled),
#endif
	XC_SET_INT("debug-file", &xroar_cfg.debug_file),
	XC_SET_INT("debug-fdc", &xroar_cfg.debug_fdc),
#ifdef WANT_GDB_TARGET
	XC_SET_INT("debug-gdb", &xroar_cfg.debug_gdb),
#endif
	XC_SET_STRING("timeout", &private_cfg.timeout),

	/* Other options: */
	XC_SET_BOOL("config-print", &private_cfg.config_print),
	XC_CALL_NULL("help", &helptext),
	XC_CALL_NULL("h", &helptext),
	XC_CALL_NULL("version", &versiontext),
	XC_OPT_END()
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

/* Help texts */

static void helptext(void) {
#ifdef LOGGING
	puts(
"Usage: xroar [-c CONFFILE] [OPTION]...\n"
"XRoar is a Dragon emulator.  Due to hardware similarities, XRoar also\n"
"emulates the Tandy Colour Computer (CoCo) models 1 & 2.\n"

"\n  -c CONFFILE     specify a configuration file\n"

"\n Machines:\n"
"  -default-machine NAME   default machine on startup\n"
"  -machine NAME           configure named machine (-machine help for list)\n"
"    -machine-desc TEXT      machine description\n"
"    -machine-arch ARCH      machine architecture (-machine-arch help for list)\n"
"    -machine-cpu CPU        machine CPU (-machine-cpu help for list)\n"
"    -bas NAME               BASIC ROM to use (CoCo only)\n"
"    -extbas NAME            Extended BASIC ROM to use\n"
"    -altbas NAME            64K mode Extended BASIC ROM (Dragon 64)\n"
"    -nobas                  disable BASIC\n"
"    -noextbas               disable Extended BASIC\n"
"    -noaltbas               disable 64K mode Extended BASIC\n"
"    -tv-type TYPE           TV type (-tv-type help for list)\n"
"    -vdg-type TYPE          VDG type (6847 or 6847t1)\n"
"    -ram KBYTES             amount of RAM in K\n"
"    -machine-cart NAME      default cartridge for selected machine\n"
"    -nodos                  don't automatically pick a DOS cartridge\n"

"\n Cartridges:\n"
"  -cart NAME            configure named cartridge (-cart help for list)\n"
"    -cart-desc TEXT       cartridge description\n"
"    -cart-type TYPE       cartridge base type (-cart-type help for list)\n"
"    -cart-rom NAME        ROM image to load ($C000-)\n"
"    -cart-rom2 NAME       second ROM image to load ($E000-)\n"
"    -cart-autorun         autorun cartridge\n"
"    -cart-becker          enable becker port where supported\n"

"\n Becker port:\n"
"  -becker               prefer becker-enabled DOS (when picked automatically)\n"
"  -becker-ip ADDRESS    address or hostname of DriveWire server [127.0.0.1]\n"
"  -becker-port PORT     port of DriveWire server [65504]\n"

"\n Files:\n"
"  -load FILENAME        load or attach FILENAME\n"
"  -run FILENAME         load or attach FILENAME and attempt autorun\n"

"\n Cassettes:\n"
"  -tape-write FILENAME  open FILENAME for tape writing\n"
"  -no-tape-fast         disable fast tape loading\n"
"  -tape-pad             force tape leader padding\n"
"  -no-tape-pad-auto     disable automatic leader padding\n"
"  -tape-rewrite         enable tape rewriting\n"

"\n Disks:\n"
"  -disk-write-back      default to enabling write-back for disk images\n"
"  -disk-jvc-hack        autodetect headerless double-sided JVC images\n"

"\n Firmware ROM images:\n"
"  -rompath PATH         ROM search path (colon-separated list)\n"
"  -romlist NAME=LIST    define a ROM list\n"
"  -romlist-print        print defined ROM lists\n"
"  -crclist NAME=LIST    define a ROM CRC list\n"
"  -crclist-print        print defined ROM CRC lists\n"
"  -force-crc-match      force per-architecture CRC matches\n"

"\n User interface:\n"
"  -ui MODULE            user-interface module (-ui help for list)\n"

"\n Video:\n"
"  -vo MODULE            video module (-vo help for list)\n"
"  -fs                   start emulator full-screen if possible\n"
"  -fskip FRAMES         frameskip (default: 0)\n"
"  -ccr RENDERER         cross-colour renderer (-ccr help for list)\n"
#ifdef HAVE_SDLGL
"  -gl-filter FILTER     OpenGL texture filter (-gl-filter help for list)\n"
#endif
"  -geometry WxH+X+Y     initial emulator geometry\n"

"\n Audio:\n"
"  -ao MODULE            audio module (-ao help for list)\n"
"  -ao-device STRING     device to use for audio module\n"
"  -ao-rate HZ           set audio sample rate (if supported by module)\n"
"  -ao-channels N        set number of audio channels, 1 or 2\n"
"  -ao-buffer-ms MS      set audio buffer size in ms (if supported)\n"
"  -ao-buffer-samples N  set audio buffer size in samples (if supported)\n"
"  -volume VOLUME        audio volume (0 - 100)\n"
#ifndef FAST_SOUND
"  -fast-sound           faster but less accurate sound\n"
#endif

"\n Keyboard:\n"
"  -keymap CODE          host keyboard type (uk, us, fr, fr_CA, de)\n"
"  -kbd-translate        enable keyboard translation\n"
"  -type STRING          intercept ROM calls to type STRING into BASIC\n"

"\n Joysticks:\n"
"  -joy NAME             configure named joystick (-joy help for list)\n"
"    -joy-desc TEXT        joystick description\n"
"    -joy-axis AXIS=SPEC   configure joystick axis\n"
"    -joy-button BTN=SPEC  configure joystick button\n"
"  -joy-right NAME       map right joystick\n"
"  -joy-left NAME        map left joystick\n"
"  -joy-virtual NAME     specify the 'virtual' joystick to cycle [kjoy0]\n"

"\n Printing:\n"
"  -lp-file FILENAME     append Dragon printer output to FILENAME\n"
"  -lp-pipe COMMAND      pipe Dragon printer output to COMMAND\n"

"\n Debugging:\n"
#ifdef WANT_GDB_TARGET
"  -gdb                  disable GDB target\n"
"  -gdb-ip               address of interface for GDB target [localhost]\n"
"  -gdb-port             port for GDB target to listen on [65520]\n"
#endif
#ifdef TRACE
"  -trace                start with trace mode on\n"
#endif
"  -debug-file FLAGS     file debugging (see manual, or -1 for all)\n"
"  -debug-fdc FLAGS      FDC debugging (see manual, or -1 for all)\n"
"  -debug-gdb FLAGS      GDB target debugging (see manual, or -1 for all)\n"
"  -timeout SECONDS      run for SECONDS then quit\n"

"\n Other options:\n"
"  -config-print         print full configuration to standard output\n"
"  -h, --help            display this help and exit\n"
"      --version         output version information and exit\n"

"\nJoystick SPECs are of the form [INTERFACE:][ARG[,ARG]...], from:\n"

"\nINTERFACE       Axis ARGs                       Button ARGs\n"
"physical        joystick-index,[-]axis-index    joystick-index,button-index\n"
"keyboard        key-name0,key-name1             key-name\n"
"mouse           screen-offset0,screen-offset1   button-number\n"

"\nFor physical joysticks a '-' before the axis index inverts the axis.  AXIS 0 is\n"
"the X-axis, and AXIS 1 the Y-axis.  BTN 0 is the only one used so far, but in\n"
"the future BTN 1 will be the second button on certain CoCo joysticks."

	);
#endif
	exit(EXIT_SUCCESS);
}

static void versiontext(void) {
#ifdef LOGGING
	puts(
"XRoar " VERSION "\n"
"Copyright (C) 2003-2013 Ciaran Anscomb\n"
"This is free software.  You may redistribute copies of it under the terms of\n"
"the GNU General Public License <http://www.gnu.org/licenses/gpl.html>.\n"
"There is NO WARRANTY, to the extent permitted by law."
	    );
#endif
	exit(EXIT_SUCCESS);
}

/* Dump all known config to stdout */

/*
 * The plan is to have proper introspection of the configuration, allowing
 * dynamic updates from a console or remotely.  Dumping of the current config
 * would then become pretty easy.
 *
 * Until then, this is a pretty awful stopgap measure.  It's liable to break if
 * a default changes or new options are added.  Be careful!
 */

static void config_print_all(void) {
	puts("# Machines\n");
	machine_config_print_all();

	puts("# Cartridges\n");
	cart_config_print_all();
	puts("# Becker port");
	if (xroar_cfg.becker) puts("becker");
	if (xroar_cfg.becker_ip) printf("becker-ip %s\n", xroar_cfg.becker_ip);
	if (xroar_cfg.becker_port) printf("becker-port %s\n", xroar_cfg.becker_port);
	putchar('\n');

	puts("# Files");
	for (GSList *l = private_cfg.load_list; l; l = l->next) {
		const char *s = l->data;
		printf("load %s\n", s);
	}
	if (private_cfg.run) printf("run %s\n", private_cfg.run);
	putchar('\n');

	puts("# Cassettes");
	if (private_cfg.tape_write) printf("tape-write %s\n", private_cfg.tape_write);
	if (private_cfg.tape_fast == 0) puts("no-tape-fast");
	if (private_cfg.tape_fast == 1) puts("tape-fast");
	if (private_cfg.tape_pad == 0) puts("no-tape-pad");
	if (private_cfg.tape_pad == 1) puts("tape-pad");
	if (private_cfg.tape_pad_auto == 0) puts("no-tape-pad-auto");
	if (private_cfg.tape_pad_auto == 1) puts("tape-pad-auto");
	if (private_cfg.tape_rewrite == 0) puts("no-tape-rewrite");
	if (private_cfg.tape_rewrite == 1) puts("tape-rewrite");
	puts("");

	puts("# Disks");
	if (xroar_cfg.disk_write_back) puts("disk-write-back");
	if (xroar_cfg.disk_jvc_hack) puts("disk-jvc-hack");
	puts("");

	puts("# Firmware ROM images");
	if (xroar_rom_path) printf("rompath %s\n", xroar_rom_path);
	romlist_print_all();
	crclist_print_all();
	if (xroar_cfg.force_crc_match) puts("force-crc-match");
	putchar('\n');

	puts("# User interface");
	if (private_cfg.ui) printf("ui %s\n", private_cfg.ui);
	if (private_cfg.filereq) printf("filereq %s\n", private_cfg.filereq);
	putchar('\n');

	puts("# Video");
	if (private_cfg.vo) printf("vo %s\n", private_cfg.vo);
	if (xroar_cfg.fullscreen) puts("fs");
	if (xroar_frameskip > 0) printf("fskip %d\n", xroar_frameskip);
	switch (xroar_cfg.ccr) {
	case CROSS_COLOUR_SIMPLE: puts("ccr simple"); break;
	// case CROSS_COLOUR_5BIT: puts("ccr 5bit"); break;
	default: break;
	}
	switch (xroar_cfg.gl_filter) {
	// case ANY_AUTO: puts("gl-filter auto"); break;
	case XROAR_GL_FILTER_NEAREST: puts("gl-filter nearest"); break;
	case XROAR_GL_FILTER_LINEAR: puts("gl-filter linear"); break;
	default: break;
	}
	if (xroar_cfg.geometry) printf("geometry %s\n", xroar_cfg.geometry);
	putchar('\n');

	puts("# Audio");
	if (private_cfg.ao) printf("ao %s\n", private_cfg.ao);
	if (xroar_cfg.ao_device) printf("ao-device %s\n", xroar_cfg.ao_device);
	if (xroar_cfg.ao_rate != 0) printf("ao-rate %d\n", xroar_cfg.ao_rate);
	if (xroar_cfg.ao_channels != 0) printf("ao-channels %d\n", xroar_cfg.ao_channels);
	if (xroar_cfg.ao_buffer_ms != 0) printf("ao-buffer-ms %d\n", xroar_cfg.ao_buffer_ms);
	if (xroar_cfg.ao_buffer_samples != 0) printf("ao-buffer-samples %d\n", xroar_cfg.ao_buffer_samples);
	if (private_cfg.volume != 100) printf("volume %d\n", private_cfg.volume);
#ifndef FAST_SOUND
	if (xroar_cfg.fast_sound) puts("fast-sound");
#endif
	putchar('\n');

	puts("# Keyboard");
	if (xroar_cfg.keymap) printf("keymap %s\n", xroar_cfg.keymap);
	if (xroar_cfg.kbd_translate) puts("kbd-translate");
	for (GSList *l = private_cfg.type_list; l; l = l->next) {
		const char *s = l->data;
		printf("type %s\n", s);
	}
	putchar('\n');

	puts("# Joysticks");
	joystick_config_print_all();

	puts("# Printing");
	if (private_cfg.lp_file) printf("lp-file %s\n", private_cfg.lp_file);
	if (private_cfg.lp_pipe) printf("lp-pipe %s\n", private_cfg.lp_pipe);
	putchar('\n');

	puts("# Debugging");
#ifdef WANT_GDB_TARGET
	if (private_cfg.gdb) puts("gdb");
	if (xroar_cfg.gdb_ip) printf("gdb-ip %s\n", xroar_cfg.gdb_ip);
	if (xroar_cfg.gdb_port) printf("gdb-port %s\n", xroar_cfg.gdb_port);
#endif
#ifdef TRACE
	if (xroar_cfg.trace_enabled == 0) puts("no-trace");
	if (xroar_cfg.trace_enabled == 1) puts("trace");
#endif
	if (xroar_cfg.debug_file != 0) printf("debug-file 0x%x\n", xroar_cfg.debug_file);
	if (xroar_cfg.debug_fdc != 0) printf("debug-fdc 0x%x\n", xroar_cfg.debug_fdc);
#ifdef WANT_GDB_TARGET
	if (xroar_cfg.debug_gdb != 0) printf("debug-gdb 0x%x\n", xroar_cfg.debug_gdb);
#endif
	if (private_cfg.timeout) printf("timeout %s\n", private_cfg.timeout);
	putchar('\n');
}
