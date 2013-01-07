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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "portalib/glib.h"
#include "portalib/strings.h"

#include "cart.h"
#include "crclist.h"
#include "events.h"
#include "fs.h"
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

/**************************************************************************/
/* Command line arguments */

/* Emulated machine */
static void set_machine(const char *name);
static char *opt_machine_desc = NULL;
static int opt_machine_arch = ANY_AUTO;
static int opt_machine_cpu = CPU_MC6809;
static char *opt_machine_palette = NULL;
static char *opt_bas = NULL;
static char *opt_extbas = NULL;
static char *opt_altbas = NULL;
static _Bool opt_nobas = 0;
static _Bool opt_noextbas = 0;
static _Bool opt_noaltbas = 0;
static int opt_tv = ANY_AUTO;
static void set_pal(void);
static void set_ntsc(void);
static int opt_ram = 0;

/* Emulated cartridge */
static void set_cart(const char *name);
static char *opt_cart_desc = NULL;
static int opt_cart_type = ANY_AUTO;
static char *opt_cart_rom = NULL;
static char *opt_cart_rom2 = NULL;
static int opt_cart_becker = ANY_AUTO;
static int opt_cart_autorun = ANY_AUTO;
static int opt_nodos = 0;

/* Attach files */
_Bool xroar_opt_force_crc_match = 0;
static char *opt_load = NULL;
static char *opt_run = NULL;
static char *opt_tape_write = NULL;
static char *opt_lp_file = NULL;
static char *opt_lp_pipe = NULL;
char *xroar_opt_becker_ip = NULL;
char *xroar_opt_becker_port = NULL;

/* Automatic actions */
static void type_command(char *string);

/* Emulator interface */
static char *opt_ui = NULL;
static char *opt_filereq = NULL;
static char *opt_vo = NULL;
char *xroar_opt_geometry = NULL;
int xroar_opt_gl_filter = ANY_AUTO;
static char *opt_ao = NULL;
int xroar_opt_ao_rate = 0;
int xroar_opt_ao_buffer_ms = 0;
int xroar_opt_ao_buffer_samples = 0;
static int xroar_opt_volume = 100;
#ifndef FAST_SOUND
_Bool xroar_fast_sound = 0;
#endif
_Bool xroar_opt_fullscreen = 0;
int xroar_opt_ccr = CROSS_COLOUR_5BIT;
int xroar_opt_frameskip = 0;
char *xroar_opt_keymap = NULL;
_Bool xroar_kbd_translate = 0;
char *xroar_opt_joy_left = NULL;
char *xroar_opt_joy_right = NULL;
static int xroar_opt_tape_fast = 1;
static int xroar_opt_tape_pad = -1;
static int xroar_opt_tape_pad_auto = 1;
static int xroar_opt_tape_rewrite = 0;
#ifdef TRACE
int xroar_trace_enabled = 0;
#else
# define xroar_trace_enabled (0)
#endif
_Bool xroar_opt_becker = 0;
_Bool xroar_opt_disk_write_back = 0;

static GSList *type_command_list = NULL;

/* Help text */
static void helptext(void);
static void versiontext(void);

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
	XC_ENUM_END()
};

static struct xconfig_enum cart_type_list[] = {
	{ .value = CART_ROM, .name = "rom", .description = "ROM cartridge" },
	{ .value = CART_DRAGONDOS, .name = "dragondos", .description = "DragonDOS" },
	{ .value = CART_DELTADOS, .name = "delta", .description = "Delta System" },
	{ .value = CART_RSDOS, .name = "rsdos", .description = "RS-DOS" },
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

/* CLI information to hand off to config reader */
static struct xconfig_option xroar_options[] = {
	/* Emulated machine */
	XC_CALL_STRING("machine", &set_machine),
	XC_SET_STRING("machine-desc", &opt_machine_desc),
	XC_SET_ENUM("machine-arch", &opt_machine_arch, arch_list),
	XC_SET_ENUM("machine-cpu", &opt_machine_cpu, cpu_list),
	XC_SET_STRING("machine-palette", &opt_machine_palette),
	XC_SET_STRING("bas", &opt_bas),
	XC_SET_STRING("extbas", &opt_extbas),
	XC_SET_STRING("altbas", &opt_altbas),
	XC_SET_BOOL("nobas", &opt_nobas),
	XC_SET_BOOL("noextbas", &opt_noextbas),
	XC_SET_BOOL("noaltbas", &opt_noaltbas),
	XC_SET_ENUM("tv-type", &opt_tv, tv_type_list),
	XC_SET_INT("ram", &opt_ram),
	/* Backwards-compatibility options: */
	XC_CALL_NULL("pal", &set_pal),
	XC_CALL_NULL("ntsc", &set_ntsc),

	/* Emulated cartridge */
	XC_CALL_STRING("cart", &set_cart),
	XC_SET_STRING("cart-desc", &opt_cart_desc),
	XC_SET_ENUM("cart-type", &opt_cart_type, cart_type_list),
	XC_SET_STRING("cart-rom", &opt_cart_rom),
	XC_SET_STRING("cart-rom2", &opt_cart_rom2),
	XC_SET_INT1("cart-becker", &opt_cart_becker),
	XC_SET_INT1("cart-autorun", &opt_cart_autorun),
	XC_SET_INT1("nodos", &opt_nodos),
	/* Backwards-compatibility options: */
	XC_SET_ENUM("dostype", &opt_cart_type, cart_type_list),
	XC_SET_STRING("dos", &opt_cart_rom),

	/* Attach files */
	XC_CALL_STRING("romlist", &romlist_assign),
	XC_CALL_NULL("romlist-print", &romlist_print),
	XC_CALL_STRING("crclist", &crclist_assign),
	XC_CALL_NULL("crclist-print", &crclist_print),
	XC_SET_BOOL("force-crc-match", &xroar_opt_force_crc_match),
	XC_SET_STRING("load", &opt_load),
	XC_SET_STRING("cartna", &opt_load),
	XC_SET_STRING("snap", &opt_load),
	XC_SET_STRING("run", &opt_run),
	XC_SET_STRING("tape-write", &opt_tape_write),
	XC_SET_STRING("cart", &opt_run),
	XC_SET_STRING("lp-file", &opt_lp_file),
	XC_SET_STRING("lp-pipe", &opt_lp_pipe),
	XC_SET_STRING("becker-ip", &xroar_opt_becker_ip),
	XC_SET_STRING("becker-port", &xroar_opt_becker_port),
	XC_SET_STRING("dw4-ip", &xroar_opt_becker_ip),
	XC_SET_STRING("dw4-port", &xroar_opt_becker_port),

	/* Automatic actions */
	XC_CALL_STRING("type", &type_command),

	/* Emulator interface */
	XC_SET_STRING("ui", &opt_ui),
	XC_SET_STRING("filereq", &opt_filereq),
	XC_SET_STRING("vo", &opt_vo),
	XC_SET_STRING("geometry", &xroar_opt_geometry),
	XC_SET_STRING("g", &xroar_opt_geometry),
	XC_SET_ENUM("gl-filter", &xroar_opt_gl_filter, gl_filter_list),
	XC_SET_STRING("ao", &opt_ao),
	XC_SET_INT("ao-rate", &xroar_opt_ao_rate),
	XC_SET_INT("ao-buffer-ms", &xroar_opt_ao_buffer_ms),
	XC_SET_INT("ao-buffer-samples", &xroar_opt_ao_buffer_samples),
	XC_SET_INT("volume", &xroar_opt_volume),
#ifndef FAST_SOUND
	XC_SET_BOOL("fast-sound", &xroar_fast_sound),
#endif
	XC_SET_BOOL("fs", &xroar_opt_fullscreen),
	XC_SET_ENUM("ccr", &xroar_opt_ccr, ccr_list),
	XC_SET_INT("fskip", &xroar_opt_frameskip),
	XC_SET_STRING("keymap", &xroar_opt_keymap),
	XC_SET_BOOL("kbd-translate", &xroar_kbd_translate),
	XC_SET_STRING("joy-left", &xroar_opt_joy_left),
	XC_SET_STRING("joy-right", &xroar_opt_joy_right),
	XC_SET_INT1("tape-fast", &xroar_opt_tape_fast),
	XC_SET_INT1("tape-pad", &xroar_opt_tape_pad),
	XC_SET_INT1("tape-pad-auto", &xroar_opt_tape_pad_auto),
	XC_SET_INT1("tape-rewrite", &xroar_opt_tape_rewrite),
	XC_SET_INT1("tapehack", &xroar_opt_tape_rewrite),
	XC_SET_BOOL("becker", &xroar_opt_becker),
	XC_SET_BOOL0("disk-write-back", &xroar_opt_disk_write_back),
#ifdef TRACE
	XC_SET_INT1("trace", &xroar_trace_enabled),
#endif

	XC_CALL_NULL("help", &helptext),
	XC_CALL_NULL("h", &helptext),
	XC_CALL_NULL("version", &versiontext),
	XC_OPT_END()
};

/**************************************************************************/
/* Global flags */

_Bool xroar_noratelimit = 0;
int xroar_frameskip = 0;

struct machine_config *xroar_machine_config;
struct cart_config *xroar_cart_config;
struct vdg_palette *xroar_vdg_palette;

/**************************************************************************/

const char *xroar_rom_path;
const char *xroar_conf_path;

struct event *xroar_ui_events = NULL;
struct event *xroar_machine_events = NULL;

static struct event load_file_event;
static void do_load_file(void *);
static char *load_file = NULL;
static int load_file_type = FILETYPE_UNKNOWN;
static int autorun_loaded_file = 0;

struct cart_status {
	struct cart_config *config;
	int enabled;
};
/* track cart_status for each machine */
static struct cart_status *cart_status_list = NULL;
static int cart_status_count = 0;

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
	{ "BAS", FILETYPE_ASC },
	{ "ASC", FILETYPE_ASC },
	{ NULL, FILETYPE_UNKNOWN }
};

void (*xroar_fullscreen_changed_cb)(_Bool fullscreen) = NULL;
void (*xroar_kbd_translate_changed_cb)(_Bool kbd_translate) = NULL;
static void alloc_cart_status(void);
static struct cart_config *get_machine_cart(void);
static struct vdg_palette *get_machine_palette(void);

/**************************************************************************/

static void set_pal(void) {
	opt_tv = TV_PAL;
}

static void set_ntsc(void) {
	opt_tv = TV_NTSC;
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
		if (opt_machine_arch != ANY_AUTO) {
			xroar_machine_config->architecture = opt_machine_arch;
			opt_machine_arch = ANY_AUTO;
		}
		xroar_machine_config->cpu = opt_machine_cpu;
		if (opt_machine_cpu == CPU_HD6309) {
			LOG_WARN("Hitachi HD6309 support is UNVERIFIED!\n");
		}
		if (opt_machine_desc) {
			xroar_machine_config->description = opt_machine_desc;
			opt_machine_desc = NULL;
		}
#ifdef LOGGING
		if (opt_machine_palette && 0 == strcmp(opt_machine_palette, "help")) {
			int count = vdg_palette_count();
			int i;
			for (i = 0; i < count; i++) {
				struct vdg_palette *vp = vdg_palette_index(i);
				printf("\t%-10s %s\n", vp->name, vp->description);
			}
			exit(EXIT_SUCCESS);
		}
#endif
		if (opt_machine_palette) {
			xroar_machine_config->vdg_palette = opt_machine_palette;
			opt_machine_palette = NULL;
		}
		if (opt_tv != ANY_AUTO) {
			xroar_machine_config->tv_standard = opt_tv;
			opt_tv = ANY_AUTO;
		}
		if (opt_ram > 0) {
			xroar_machine_config->ram = opt_ram;
			opt_ram = 0;
		}
		xroar_machine_config->nobas = opt_nobas;
		xroar_machine_config->noextbas = opt_noextbas;
		xroar_machine_config->noaltbas = opt_noaltbas;
		opt_nobas = opt_noextbas = opt_noaltbas = 0;
		if (opt_bas) {
			xroar_machine_config->bas_rom = opt_bas;
			opt_bas = NULL;
		}
		if (opt_extbas) {
			xroar_machine_config->extbas_rom = opt_extbas;
			opt_extbas = NULL;
		}
		if (opt_altbas) {
			xroar_machine_config->altbas_rom = opt_altbas;
			opt_altbas = NULL;
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
	if (xroar_cart_config) {
		if (opt_cart_desc) {
			xroar_cart_config->description = opt_cart_desc;
			opt_cart_desc = NULL;
		}
		if (opt_cart_type != ANY_AUTO) {
			xroar_cart_config->type = opt_cart_type;
			opt_cart_type = ANY_AUTO;
		}
		if (opt_cart_rom) {
			xroar_cart_config->rom = opt_cart_rom;
			opt_cart_rom = NULL;
		}
		if (opt_cart_rom2) {
			xroar_cart_config->rom2 = opt_cart_rom2;
			opt_cart_rom2 = NULL;
		}
		if (opt_cart_becker != ANY_AUTO) {
			xroar_cart_config->becker_port = opt_cart_becker;
			opt_cart_becker = ANY_AUTO;
		}
		if (opt_cart_autorun != ANY_AUTO) {
			xroar_cart_config->autorun = opt_cart_autorun;
			opt_cart_autorun = ANY_AUTO;
		}
		cart_config_complete(xroar_cart_config);
	}
	if (name) {
		xroar_cart_config = cart_config_by_name(name);
		if (!xroar_cart_config) {
			xroar_cart_config = cart_config_new();
			xroar_cart_config->name = g_strdup(name);
		}
	}
}

static void type_command(char *string) {
	type_command_list = g_slist_append(type_command_list, string);
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

static void helptext(void) {
#ifdef LOGGING
	puts(
"Usage: xroar [OPTION]...\n"
"XRoar is a Dragon emulator.  Due to hardware similarities, XRoar also\n"
"emulates the Tandy Colour Computer (CoCo) models 1 & 2.\n"

"\n Emulated machine:\n"
"  -machine NAME           select/configure machine (-machine help for list)\n"
"  -machine-desc TEXT      machine description\n"
"  -machine-arch ARCH      machine architecture (-machine-arch help for list)\n"
"  -machine-cpu CPU        machine CPU (-machine-cpu help for list)\n"
"  -machine-palette NAME   VDG palette (-machine-palette help for list)\n"
"  -bas NAME               BASIC ROM to use (CoCo only)\n"
"  -extbas NAME            Extended BASIC ROM to use\n"
"  -altbas NAME            alternate BASIC ROM (Dragon 64)\n"
"  -noextbas               disable Extended BASIC\n"
"  -tv-type TYPE           set TV type (-tv-type help for list)\n"
"  -ram KBYTES             specify amount of RAM in K\n"

"\n Emulated cartridge:\n"
"  -cart NAME            select/configure cartridge (-cart help for list)\n"
"  -cart-desc TEXT       set cartridge description\n"
"  -cart-type TYPE       set cartridge type (-cart-type help for list)\n"
"  -cart-rom NAME        ROM image to load ($C000-)\n"
"  -cart-rom2 NAME       second ROM image to load ($E000-)\n"
"  -cart-becker          enable becker port where supported\n"
"  -cart-autorun         autorun cartridge\n"
"  -nodos                don't automatically pick a DOS cartridge\n"

"\n Attach files:\n"
"  -romlist NAME=LIST    define a ROM list\n"
"  -romlist-print        print defined ROM lists\n"
"  -crclist NAME=LIST    define a ROM CRC list\n"
"  -crclist-print        print defined ROM CRC lists\n"
"  -force-crc-match      force per-architecture CRC matches\n"
"  -load FILENAME        load or attach FILENAME\n"
"  -run FILENAME         load or attach FILENAME and attempt autorun\n"
"  -tape-write FILENAME  open FILENAME for tape writing\n"
"  -lp-file FILENAME     append Dragon printer output to FILENAME\n"
"  -lp-pipe COMMAND      pipe Dragon printer output to COMMAND\n"
"  -becker-ip            IP address of DriveWire server [127.0.0.1]\n"
"  -becker-port          port of DriveWire server [65504]\n"

"\n Automatic actions:\n"
"  -type STRING          intercept ROM calls to type STRING into BASIC\n"

"\n Emulator interface:\n"
"  -ui MODULE            user-interface module (-ui help for list)\n"
"  -vo MODULE            video module (-vo help for list)\n"
"  -geometry WxH+X+Y     initial emulator geometry\n"
#ifdef HAVE_SDLGL
"  -gl-filter FILTER     OpenGL texture filter (-gl-filter help for list)\n"
#endif
"  -ao MODULE            audio module (-ao help for list)\n"
"  -ao-rate HZ           set audio sample rate (if supported by module)\n"
"  -ao-buffer-ms MS      set audio buffer size in ms (if supported)\n"
"  -ao-buffer-samples N  set audio buffer size in samples (if supported)\n"
"  -volume VOLUME        audio volume (0 - 100)\n"
#ifndef FAST_SOUND
"  -fast-sound           faster but less accurate sound\n"
#endif
"  -fs                   start emulator full-screen if possible\n"
"  -ccr RENDERER         cross-colour renderer (-ccr help for list)\n"
"  -fskip FRAMES         frameskip (default: 0)\n"
"  -keymap CODE          host keyboard type (uk, us, fr, fr_CA, de)\n"
"  -kbd-translate        enable keyboard translation\n"
"  -joy-left JOYSPEC     map left joystick\n"
"  -joy-right JOYSPEC    map right joystick\n"
"  -tape-fast            enable fast tape loading\n"
"  -tape-pad             enable tape leader padding\n"
"  -tape-pad-auto        detect need for leader padding automatically\n"
"  -tape-rewrite         enable tape rewriting\n"
"  -becker               default to becker-enabled DOS\n"
"  -disk-write-back      default to enabling write-back for disk images\n"
#ifdef TRACE
"  -trace                start with trace mode on\n"
#endif

"\n Other options:\n"
"  -h, --help            display this help and exit\n"
"      --version         output version information and exit\n"

"\nA JOYSPEC maps physical joystick axes and buttons to one of the emulated\n"
"machine's joysticks.  JOYSPEC is of the form 'DEV,AXIS:DEV,AXIS:DEV,BUTTON',\n"
"mapping two axes and a button to the X, Y and firebutton on the emulated\n"
"joystick respectively."
	);
#endif
	exit(EXIT_SUCCESS);
}

#ifndef ROMPATH
# define ROMPATH "."
#endif
#ifndef CONFPATH
# define CONFPATH "."
#endif

int xroar_init(int argc, char **argv) {
	int argn = 1, ret;
	char *conffile;

	xroar_rom_path = getenv("XROAR_ROM_PATH");
	if (!xroar_rom_path)
		xroar_rom_path = ROMPATH;

	xroar_conf_path = getenv("XROAR_CONF_PATH");
	if (!xroar_conf_path)
		xroar_conf_path = CONFPATH;

	/* If a configuration file is found, parse it */
	conffile = find_in_path(xroar_conf_path, "xroar.conf");
	if (conffile) {
		/* ignore bad lines in config file */
		(void)xconfig_parse_file(xroar_options, conffile);
		g_free(conffile);
	}
	/* Finish any machine or cart config in config file */
	set_machine(NULL);
	set_cart(NULL);
	/* Don't auto-select last machine or cart in config file */
	xroar_machine_config = NULL;
	xroar_cart_config = NULL;

	/* Parse command line options */
	ret = xconfig_parse_cli(xroar_options, argc, argv, &argn);
	if (ret != XCONFIG_OK) {
		exit(EXIT_FAILURE);
	}
	/* Determine initial machine configuration */
	if (!xroar_machine_config) {
		xroar_machine_config = machine_config_first_working();
	}
	/* Finish any machine or cart config on command line */
	set_machine(NULL);
	if (!xroar_cart_config) {
		xroar_cart_config = cart_find_working_dos(xroar_machine_config);
	}
	set_cart(NULL);

	/* Select a UI module then, possibly using lists specified in that
	 * module, select all other modules */
	ui_module = (UIModule *)module_select_by_arg((struct module **)ui_module_list, opt_ui);
	if (ui_module == NULL) {
		LOG_DEBUG(0, "%s: ui module `%s' not found\n", argv[0], opt_ui);
		exit(EXIT_FAILURE);
	}
	/* Select file requester module */
	if (ui_module->filereq_module_list != NULL)
		filereq_module_list = ui_module->filereq_module_list;
	filereq_module = (FileReqModule *)module_select_by_arg((struct module **)filereq_module_list, opt_filereq);
	/* Select video module */
	if (ui_module->video_module_list != NULL)
		video_module_list = ui_module->video_module_list;
	video_module = (VideoModule *)module_select_by_arg((struct module **)video_module_list, opt_vo);
	/* Select sound module */
	if (ui_module->sound_module_list != NULL)
		sound_module_list = ui_module->sound_module_list;
	sound_module = (SoundModule *)module_select_by_arg((struct module **)sound_module_list, opt_ao);
	/* Select keyboard module */
	if (ui_module->keyboard_module_list != NULL)
		keyboard_module_list = ui_module->keyboard_module_list;
	keyboard_module = NULL;
	/* Select joystick module */
	if (ui_module->joystick_module_list != NULL)
		joystick_module_list = ui_module->joystick_module_list;
	joystick_module = NULL;

	/* Check other command-line options */
	if (xroar_opt_frameskip < 0)
		xroar_opt_frameskip = 0;
	xroar_frameskip = xroar_opt_frameskip;
	if (opt_load) {
		load_file = opt_load;
		autorun_loaded_file = 0;
	} else if (opt_run) {
		load_file = opt_run;
		autorun_loaded_file = 1;
	} else if (argn < argc) {
		load_file = g_strdup(argv[argn]);
		autorun_loaded_file = 1;
	}
	sound_set_volume(xroar_opt_volume);
	/* turn off tape_pad_auto if any tape_pad specified */
	if (xroar_opt_tape_pad >= 0)
		xroar_opt_tape_pad_auto = 0;
	xroar_opt_tape_fast = xroar_opt_tape_fast ? TAPE_FAST : 0;
	xroar_opt_tape_pad = (xroar_opt_tape_pad > 0) ? TAPE_PAD : 0;
	xroar_opt_tape_pad_auto = xroar_opt_tape_pad_auto ? TAPE_PAD_AUTO : 0;
	xroar_opt_tape_rewrite = xroar_opt_tape_rewrite ? TAPE_REWRITE : 0;

	alloc_cart_status();

	/* Override DOS cart if autoloading a cassette, or replace with ROM
	 * cart if appropriate. */
	if (load_file) {
		load_file_type = xroar_filetype_by_ext(load_file);
		switch (load_file_type) {
			case FILETYPE_CAS:
			case FILETYPE_ASC:
			case FILETYPE_WAV:
			case FILETYPE_UNKNOWN:
				cart_status_list[xroar_machine_config->index].enabled = 0;
				/* if user specified commands to type, override
				 * automatic CLOAD[M] */
				if (type_command_list) {
					autorun_loaded_file = 0;
				}
				break;
			case FILETYPE_ROM:
				cart_status_list[xroar_machine_config->index].enabled = 1;
				xroar_cart_config = cart_config_by_name(load_file);
				xroar_cart_config->autorun = autorun_loaded_file;
				g_free((char *)load_file);
				load_file = NULL;
				break;
			default:
				break;
		}
	}

	/* Record initial cart configuration */
	cart_status_list[xroar_machine_config->index].config = xroar_cart_config;
	/* Re-read cart config - will find a DOS if appropriate */
	xroar_cart_config = get_machine_cart();

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
		return 1;
	}
	sound_module = (SoundModule *)module_init_from_list((struct module **)sound_module_list, (struct module *)sound_module);
	if (sound_module == NULL && sound_module_list != NULL) {
		LOG_ERROR("No sound module initialised.\n");
		return 1;
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

	/* Notify UI of starting options: */
	xroar_set_kbd_translate(xroar_kbd_translate);

	/* Configure machine */
	machine_configure(xroar_machine_config);
	machine_insert_cart(xroar_cart_config);
	/* Reset everything */
	machine_reset(RESET_HARD);
	printer_reset();
	tape_select_state(xroar_opt_tape_fast | xroar_opt_tape_pad | xroar_opt_tape_pad_auto | xroar_opt_tape_rewrite);

	if (load_file) {
		switch (load_file_type) {
		/* most things can be loaded/run straight off */
		case FILETYPE_VDK:
		case FILETYPE_JVC:
		case FILETYPE_DMK:
		case FILETYPE_CAS:
		case FILETYPE_WAV:
		case FILETYPE_SNA:
		case FILETYPE_ASC:
			xroar_load_file_by_type(load_file, autorun_loaded_file);
			break;
		/* delay loading everything else by 2s */
		default:
			/* For everything else, defer loading the file */
			event_init(&load_file_event, do_load_file, NULL);
			load_file_event.at_tick = event_current_tick + OSCILLATOR_RATE * 2;
			event_queue(&UI_EVENT_LIST, &load_file_event);
			break;
		}
	}

	if (opt_tape_write) {
		int write_file_type = xroar_filetype_by_ext(opt_tape_write);
		switch (write_file_type) {
			case FILETYPE_CAS:
			case FILETYPE_WAV:
				tape_open_writing(opt_tape_write);
				if (ui_module->output_tape_filename_cb) {
					ui_module->output_tape_filename_cb(opt_tape_write);
				}
				break;
			default:
				break;
		}
	}

	while (type_command_list) {
		keyboard_queue_basic(type_command_list->data);
		type_command_list = g_slist_remove(type_command_list, type_command_list->data);
	}
	if (opt_lp_file) {
		printer_open_file(opt_lp_file);
	} else if (opt_lp_pipe) {
		printer_open_pipe(opt_lp_pipe);
	}
	return 0;
}

void xroar_shutdown(void) {
	machine_shutdown();
	module_shutdown((struct module *)joystick_module);
	module_shutdown((struct module *)keyboard_module);
	module_shutdown((struct module *)sound_module);
	module_shutdown((struct module *)video_module);
	module_shutdown((struct module *)filereq_module);
	module_shutdown((struct module *)ui_module);
}

static void alloc_cart_status(void) {
	int count = xroar_machine_config->index;
	if (count >= cart_status_count) {
		struct cart_status *new_list = g_realloc(cart_status_list, (count + 1) * sizeof(struct cart_status));
		int i;
		/* clear new entries */
		cart_status_list = new_list;
		for (i = cart_status_count; i <= count; i++) {
			cart_status_list[i].config = NULL;
			cart_status_list[i].enabled = !opt_nodos;
		}
		cart_status_count = count + 1;
	}
}

static struct cart_config *get_machine_cart(void) {
	int mindex;
	assert(xroar_machine_config != NULL);
	mindex = xroar_machine_config->index;
	alloc_cart_status();
	if (!cart_status_list) {
		return NULL;
	}
	if (!cart_status_list[mindex].enabled) {
		return NULL;
	}
	if (!cart_status_list[mindex].config) {
		cart_status_list[mindex].config = cart_find_working_dos(xroar_machine_config);
	}
	return cart_status_list[mindex].config;
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

void xroar_run(void) {
	CPU0->interrupt_hook = NULL;
	CPU0->instruction_posthook = NULL;

#ifdef TRACE
	xroar_set_trace(xroar_trace_enabled);
#endif

	/* If UI module has its own idea of a main loop, delegate to that */
	if (ui_module->run) {
		ui_module->run();
		return;
	}

	while (1) {
		machine_run(VDG_LINE_DURATION * 8);
		event_run_queue(UI_EVENT_LIST);
	}
}

int xroar_filetype_by_ext(const char *filename) {
	char *ext;
	int i;
	ext = strrchr(filename, '.');
	if (ext == NULL)
		return FILETYPE_UNKNOWN;
	ext++;
	for (i = 0; filetypes[i].ext; i++) {
		if (!strncasecmp(ext, filetypes[i].ext, strlen(filetypes[i].ext)))
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
			xroar_insert_disk_file(0, filename);
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
			return intel_hex_read(filename);
		case FILETYPE_SNA:
			return read_snapshot(filename);
		case FILETYPE_ROM: {
			struct cart_config *cc;
			machine_remove_cart();
			cc = cart_config_by_name(filename);
			if (cc) {
				cc->autorun = autorun;
				xroar_set_cart(cc->index);
				if (autorun) {
					machine_reset(RESET_HARD);
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
	(void)data;
	xroar_load_file_by_type(load_file, autorun_loaded_file);
}

/* Helper functions */

#ifdef TRACE
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
			set_to = !xroar_trace_enabled;
			break;
	}
	xroar_trace_enabled = set_to;
	if (xroar_trace_enabled) {
		switch (xroar_machine_config->cpu) {
		case CPU_MC6809: default:
			CPU0->interrupt_hook = mc6809_trace_irq;
			CPU0->instruction_posthook = mc6809_trace_print;
			break;
		case CPU_HD6309:
			CPU0->interrupt_hook = hd6309_trace_irq;
			CPU0->instruction_posthook = hd6309_trace_print;
			break;
		}
	} else {
		CPU0->interrupt_hook = NULL;
		CPU0->instruction_posthook = NULL;
	}
#endif
}
#endif

void xroar_new_disk(int drive) {
	char *filename = filereq_module->save_filename(xroar_disk_exts);

	if (filename == NULL)
		return;
	int filetype = xroar_filetype_by_ext(filename);
	xroar_eject_disk(drive);
	/* Default to 40T 1H.  XXX: need interface to select */
	struct vdisk *new_disk = vdisk_blank_disk(1, 40, VDISK_LENGTH_5_25);
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
	new_disk->file_write_protect = 0;
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
			xroar_kbd_translate = !xroar_kbd_translate;
			break;
		default:
			xroar_kbd_translate = kbd_translate;
			break;
	}
	if (xroar_kbd_translate_changed_cb) {
		xroar_kbd_translate_changed_cb(xroar_kbd_translate);
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
		struct cart_config *cc;
		int cart_index = -1;
		xroar_machine_config = machine_config_index(new);
		machine_configure(xroar_machine_config);
		cc = get_machine_cart();
		if (cc) {
			cart_index = cc->index;
		}
		xroar_set_cart(cart_index);
		xroar_vdg_palette = get_machine_palette();
		if (video_module->update_palette) {
			video_module->update_palette();
		}
		machine_reset(RESET_HARD);
		if (ui_module->machine_changed_cb) {
			ui_module->machine_changed_cb(new);
		}
	}
	lock = 0;
}

void xroar_set_cart(int cart_index) {
	static int lock = 0;
	if (lock) return;
	lock = 1;
	int old = -1;
	int mindex;
	int count = cart_config_count();

	assert(xroar_machine_config != NULL);
	alloc_cart_status();
	mindex = xroar_machine_config->index;
	assert(cart_status_list != NULL);
	assert(mindex < cart_status_count);

	if (xroar_cart_config) old = xroar_cart_config->index;

	switch (cart_index) {
		case XROAR_TOGGLE:
			cart_status_list[mindex].enabled = !cart_status_list[mindex].enabled;
			break;
		case XROAR_CYCLE:
			cart_status_list[mindex].config = cart_config_index((old + 1) % count);
			break;
		case -1:
			cart_status_list[mindex].enabled = 0;
			break;
		default:
			cart_status_list[mindex].config = cart_config_index(cart_index);
			cart_status_list[mindex].enabled = 1;
			break;
	}

	xroar_cart_config = get_machine_cart();
	machine_insert_cart(xroar_cart_config);
	if (ui_module->cart_changed_cb) {
		if (xroar_cart_config) {
			ui_module->cart_changed_cb(xroar_cart_config->index);
		} else {
			ui_module->cart_changed_cb(-1);
		}
	}
	lock = 0;
}

/* For old snapshots */
void xroar_set_dos(int dos_type) {
	struct cart_config *cc = NULL;
	int i = -1;
	switch (dos_type) {
	case DOS_DRAGONDOS:
		cc = cart_config_by_name("dragondos");
		break;
	case DOS_RSDOS:
		cc = cart_config_by_name("rsdos");
		break;
	case DOS_DELTADOS:
		cc = cart_config_by_name("delta");
		break;
	default:
		break;
	}
	if (cc) {
		i = cc->index;
	}
	xroar_set_cart(i);
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
	machine_reset(RESET_HARD);
}
