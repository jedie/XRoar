/*  SDLMain.m - main entry point for our Cocoa-ized SDL app
 *
 *  Initial Version: Darrell Walisser <dwaliss1@purdue.edu>
 *  Non-NIB-Code & other changes: Max Horn <max@quendi.de>
 *
 *  Feel free to customize this file to suit your needs
 */

#include "config.h"

#include <SDL.h>
#include <sys/param.h> /* for MAXPATHLEN */
#include <unistd.h>

#import <Cocoa/Cocoa.h>

#include "logging.h"
#include "cart.h"
#include "joystick.h"
#include "keyboard.h"
#include "machine.h"
#include "module.h"
#include "tape.h"
#include "xroar.h"
#include "sdl/common.h"

#define TAG_TYPE_MASK (0x7f << 24)
#define TAG_VALUE_MASK (0xffffff)

#define TAG_SIMPLE_ACTION (1 << 24)

#define TAG_MACHINE (2 << 24)

#define TAG_CARTRIDGE (3 << 24)

#define TAG_TAPE_FLAGS (4 << 24)

#define TAG_INSERT_DISK (5 << 24)
#define TAG_NEW_DISK (6 << 24)
#define TAG_WRITE_ENABLE (7 << 24)
#define TAG_WRITE_BACK (8 << 24)

#define TAG_FULLSCREEN (9 << 24)
#define TAG_VDG_INVERSE (16 << 24)
#define TAG_CROSS_COLOUR (10 << 24)

#define TAG_FAST_SOUND (11 << 24)

#define TAG_KEYMAP (12 << 24)
#define TAG_KBD_TRANSLATE (13 << 24)
#define TAG_JOY_RIGHT (14 << 24)
#define TAG_JOY_LEFT (15 << 24)

enum {
	TAG_QUIT,
	TAG_RESET_SOFT,
	TAG_RESET_HARD,
	TAG_FILE_LOAD,
	TAG_FILE_RUN,
	TAG_FILE_SAVE_SNAPSHOT,
	TAG_TAPE_INPUT,
	TAG_TAPE_OUTPUT,
	TAG_TAPE_INPUT_REWIND,
	TAG_ZOOM_IN,
	TAG_ZOOM_OUT,
	TAG_JOY_SWAP,
};

@interface SDLMain : NSObject
@end

/* For some reaon, Apple removed setAppleMenu from the headers in 10.4, but the
 * method still is there and works.  To avoid warnings, we declare it ourselves
 * here. */
@interface NSApplication(SDL_Missing_Methods)
- (void)setAppleMenu:(NSMenu *)menu;
@end

/* Use this flag to determine whether we use CPS (docking) or not */
#define SDL_USE_CPS 1

#ifdef SDL_USE_CPS

/* Portions of CPS.h */
typedef struct CPSProcessSerNum {
	UInt32 lo;
	UInt32 hi;
} CPSProcessSerNum;

extern OSErr CPSGetCurrentProcess(CPSProcessSerNum *psn);
extern OSErr CPSEnableForegroundOperation(CPSProcessSerNum *psn, UInt32 _arg2, UInt32 _arg3, UInt32 _arg4, UInt32 _arg5);
extern OSErr CPSSetFrontProcess(CPSProcessSerNum *psn);

#endif  /* SDL_USE_CPS */

static int gArgc;
static char **gArgv;
static BOOL gFinderLaunch;
static BOOL gCalledAppMainline = FALSE;

static NSString *get_application_name(void) {
	const NSDictionary *dict;
	NSString *app_name = 0;

	/* Determine the application name */
	dict = (const NSDictionary *)CFBundleGetInfoDictionary(CFBundleGetMainBundle());
	if (dict)
		app_name = [dict objectForKey: @"CFBundleName"];

	if (![app_name length])
		app_name = [[NSProcessInfo processInfo] processName];

	return app_name;
}

static int current_cc = 0;
static int current_machine = 0;
static int current_cartridge = 0;
static int current_keymap = 0;
static int is_fullscreen = 0;
static int is_kbd_translate = 0;
static _Bool is_fast_sound = 0;
static _Bool disk_write_enable[4] = { 1, 1, 1, 1 };
static _Bool disk_write_back[4] = { 0, 0, 0, 0 };

static struct {
	const char *name;
	NSString *description;
} joystick_names[] = {
	{ NULL, @"None" },
	{ "joy0", @"Joystick 0" },
	{ "joy1", @"Joystick 1" },
	{ "kjoy0", @"Keyboard" },
	{ "mjoy0", @"Mouse" },
};

/* Hacky way to discover which joystick is mapped - for determining which radio
 * button is visible in joystick menus. */
static int selected_joystick(unsigned port) {
	int i;
	if (port > 1)
		return 0;
	if (!joystick_port_config[port])
		return 0;
	if (!joystick_port_config[port]->name)
		return 0;
	for (i = 1; i < 5; i++)
		if (0 == strcmp(joystick_port_config[port]->name, joystick_names[i].name))
			return i;
	return 0;
}

/* Setting this to true is a massive hack so that cocoa file dialogues receive
 * keypresses.  Ideally, need to sort SDL out or turn this into a regular
 * OpenGL application. */
int cocoa_super_all_keys = 0;

@interface SDLApplication : NSApplication
@end

@implementation SDLApplication

- (void)sendEvent:(NSEvent *)anEvent {
	if (NSKeyDown == [anEvent type] || NSKeyUp == [anEvent type]) {
		if (cocoa_super_all_keys || ([anEvent modifierFlags] & NSCommandKeyMask))
			[super sendEvent:anEvent];
	} else {
		[super sendEvent:anEvent];
	}
}

- (void)do_set_state:(id)sender {
	int tag = [sender tag];
	int tag_type = tag & TAG_TYPE_MASK;
	int tag_value = tag & TAG_VALUE_MASK;
	switch (tag_type) {

	/* Simple actions: */
	case TAG_SIMPLE_ACTION:
		switch (tag_value) {
		case TAG_QUIT:
			{
				SDL_Event event;
				event.type = SDL_QUIT;
				SDL_PushEvent(&event);
			}
			break;
		case TAG_RESET_SOFT:
			xroar_soft_reset();
			break;
		case TAG_RESET_HARD:
			xroar_hard_reset();
			break;
		case TAG_FILE_RUN:
			xroar_run_file(NULL);
			break;
		case TAG_FILE_LOAD:
			xroar_load_file(NULL);
			break;
		case TAG_FILE_SAVE_SNAPSHOT:
			xroar_save_snapshot();
			break;
		case TAG_TAPE_INPUT:
			xroar_select_tape_input();
			break;
		case TAG_TAPE_OUTPUT:
			xroar_select_tape_output();
			break;
		case TAG_TAPE_INPUT_REWIND:
			if (tape_input)
				tape_rewind(tape_input);
			break;
		case TAG_ZOOM_IN:
			sdl_zoom_in();
			break;
		case TAG_ZOOM_OUT:
			sdl_zoom_out();
			break;
		case TAG_JOY_SWAP:
			joystick_swap();
			break;
		default:
			break;
		}
		break;

	/* Machines: */
	case TAG_MACHINE:
		current_machine = tag;
		xroar_set_machine(tag_value);
		break;

	/* Cartridges: */
	case TAG_CARTRIDGE:
		{
			if (tag_value & (1 << 23))
				tag_value = -1;
			struct cart_config *cc = cart_config_index(tag_value);
			xroar_set_cart(cc ? cc->name : NULL);
		}
		break;

	/* Cassettes: */
	case TAG_TAPE_FLAGS:
		tape_set_state(tape_get_state() ^ tag_value);
		break;

	/* Disks: */
	case TAG_INSERT_DISK:
		xroar_insert_disk(tag_value);
		break;
	case TAG_NEW_DISK:
		xroar_new_disk(tag_value);
		break;
	case TAG_WRITE_ENABLE:
		disk_write_enable[tag_value] = !disk_write_enable[tag_value];
		xroar_set_write_enable(1, tag_value, disk_write_enable[tag_value]);
		break;
	case TAG_WRITE_BACK:
		disk_write_back[tag_value] = !disk_write_back[tag_value];
		xroar_set_write_back(1, tag_value, disk_write_back[tag_value]);
		break;

	/* Video: */
	case TAG_FULLSCREEN:
		is_fullscreen = !is_fullscreen;
		xroar_set_fullscreen(0, is_fullscreen);
		break;
	case TAG_CROSS_COLOUR:
		current_cc = tag;
		xroar_set_cross_colour(0, tag_value);
		break;
	case TAG_VDG_INVERSE:
		xroar_set_vdg_inverted_text(0, XROAR_TOGGLE);
		break;

	/* Audio: */
	case TAG_FAST_SOUND:
		is_fast_sound = !is_fast_sound;
		machine_set_fast_sound(is_fast_sound);
		break;

	/* Keyboard: */
	case TAG_KEYMAP:
		current_keymap = tag;
		xroar_set_keymap(tag_value);
		break;
	case TAG_KBD_TRANSLATE:
		is_kbd_translate = !is_kbd_translate;
		xroar_set_kbd_translate(0, is_kbd_translate);
		break;

	/* Joysticks: */
	case TAG_JOY_RIGHT:
		if (tag_value >= 1 && tag_value <= 4) {
			joystick_map(joystick_config_by_name(joystick_names[tag_value].name), 0);
		} else {
			joystick_unmap(0);
		}
		break;
	case TAG_JOY_LEFT:
		if (tag_value >= 1 && tag_value <= 4) {
			joystick_map(joystick_config_by_name(joystick_names[tag_value].name), 1);
		} else {
			joystick_unmap(1);
		}
		break;

	default:
		break;
	}
}

- (BOOL)validateMenuItem:(NSMenuItem *)item {
	int tag = [item tag];
	int tag_type = tag & TAG_TYPE_MASK;
	int tag_value = tag & TAG_VALUE_MASK;
	switch (tag_type) {

	case TAG_MACHINE:
		[item setState:((tag == current_machine) ? NSOnState : NSOffState)];
		break;

	case TAG_CARTRIDGE:
		[item setState:((tag == current_cartridge) ? NSOnState : NSOffState)];
		break;

	case TAG_TAPE_FLAGS:
		[item setState:((tape_get_state() & tag_value) ? NSOnState : NSOffState)];
		break;

	case TAG_WRITE_ENABLE:
		[item setState:(disk_write_enable[tag_value] ? NSOnState : NSOffState)];
		break;
	case TAG_WRITE_BACK:
		[item setState:(disk_write_back[tag_value] ? NSOnState : NSOffState)];
		break;

	case TAG_FULLSCREEN:
		[item setState:(is_fullscreen ? NSOnState : NSOffState)];
		break;
	case TAG_VDG_INVERSE:
		[item setState:(xroar_cfg.vdg_inverted_text ? NSOnState : NSOffState)];
		break;
	case TAG_CROSS_COLOUR:
		[item setState:((tag == current_cc) ? NSOnState : NSOffState)];
		break;

	case TAG_FAST_SOUND:
		[item setState:(is_fast_sound ? NSOnState : NSOffState)];
		break;

	case TAG_KEYMAP:
		[item setState:((tag == current_keymap) ? NSOnState : NSOffState)];
		break;
	case TAG_KBD_TRANSLATE:
		[item setState:(is_kbd_translate ? NSOnState : NSOffState)];
		break;

	case TAG_JOY_RIGHT:
		[item setState:((tag_value == selected_joystick(0)) ? NSOnState : NSOffState)];
		break;
	case TAG_JOY_LEFT:
		[item setState:((tag_value == selected_joystick(1)) ? NSOnState : NSOffState)];
		break;

	}
	return YES;
}

@end

/* The main class of the application, the application's delegate */
@implementation SDLMain

/* Set the working directory to the .app's parent directory */
- (void)setupWorkingDirectory:(BOOL)shouldChdir {
	if (shouldChdir) {
		char parentdir[MAXPATHLEN];
		CFURLRef url = CFBundleCopyBundleURL(CFBundleGetMainBundle());
		CFURLRef url2 = CFURLCreateCopyDeletingLastPathComponent(0, url);
		if (CFURLGetFileSystemRepresentation(url2, 1, (UInt8 *)parentdir, MAXPATHLEN)) {
			chdir(parentdir);   /* chdir to the binary app's parent */
		}
		CFRelease(url);
		CFRelease(url2);
	}
}

static void setApplicationMenu(void) {
	/* warning: this code is very odd */
	NSMenu *apple_menu;
	NSMenuItem *item;
	NSString *title;
	NSString *app_name;

	app_name = get_application_name();
	apple_menu = [[NSMenu alloc] initWithTitle:@""];

	/* Add menu items */
	title = [@"About " stringByAppendingString:app_name];
	[apple_menu addItemWithTitle:title action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""];

	[apple_menu addItem:[NSMenuItem separatorItem]];

	title = [@"Hide " stringByAppendingString:app_name];
	[apple_menu addItemWithTitle:title action:@selector(hide:) keyEquivalent:@"h"];

	item = (NSMenuItem *)[apple_menu addItemWithTitle:@"Hide Others" action:@selector(hideOtherApplications:) keyEquivalent:@"h"];
	[item setKeyEquivalentModifierMask:(NSAlternateKeyMask|NSCommandKeyMask)];

	[apple_menu addItemWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];

	[apple_menu addItem:[NSMenuItem separatorItem]];

	title = [@"Quit " stringByAppendingString:app_name];
	item = [[NSMenuItem alloc] initWithTitle:title action:@selector(do_set_state:) keyEquivalent:@"q"];
	[item setTag:(TAG_SIMPLE_ACTION | TAG_QUIT)];
	[apple_menu addItem:item];

	/* Put menu into the menubar */
	item = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
	[item setSubmenu:apple_menu];
	[[NSApp mainMenu] addItem:item];

	/* Tell the application object that this is now the application menu */
	[NSApp setAppleMenu:apple_menu];

	/* Finally give up our references to the objects */
	[apple_menu release];
	[item release];
}

/* Create File menu */
static void setup_file_menu(void) {
	NSMenu *file_menu;
	NSMenuItem *file_menu_item;
	NSMenuItem *item;
	NSMenu *submenu;
	NSString *tmp;

	file_menu = [[NSMenu alloc] initWithTitle:@"File"];

	tmp = [NSString stringWithFormat:@"Run%C", 0x2026];
	item = [[NSMenuItem alloc] initWithTitle:tmp action:@selector(do_set_state:) keyEquivalent:@"L"];
	[item setTag:(TAG_SIMPLE_ACTION | TAG_FILE_RUN)];
	[file_menu addItem:item];
	[item release];
	[tmp release];

	tmp = [NSString stringWithFormat:@"Load%C", 0x2026];
	item = [[NSMenuItem alloc] initWithTitle:tmp action:@selector(do_set_state:) keyEquivalent:@"l"];
	[item setTag:(TAG_SIMPLE_ACTION | TAG_FILE_LOAD)];
	[file_menu addItem:item];
	[item release];
	[tmp release];

	[file_menu addItem:[NSMenuItem separatorItem]];

	submenu = [[NSMenu alloc] initWithTitle:@"Cassette"];

	tmp = [NSString stringWithFormat:@"Input Tape%C", 0x2026];
	item = [[NSMenuItem alloc] initWithTitle:tmp action:@selector(do_set_state:) keyEquivalent:@""];
	[item setTag:(TAG_SIMPLE_ACTION | TAG_TAPE_INPUT)];
	[submenu addItem:item];
	[item release];
	[tmp release];

	tmp = [NSString stringWithFormat:@"Output Tape%C", 0x2026];
	item = [[NSMenuItem alloc] initWithTitle:tmp action:@selector(do_set_state:) keyEquivalent:@"w"];
	[item setTag:(TAG_SIMPLE_ACTION | TAG_TAPE_OUTPUT)];
	[submenu addItem:item];
	[item release];
	[tmp release];

	[submenu addItem:[NSMenuItem separatorItem]];

	item = [[NSMenuItem alloc] initWithTitle:@"Rewind Input Tape" action:@selector(do_set_state:) keyEquivalent:@""];
	[item setTag:(TAG_SIMPLE_ACTION | TAG_TAPE_INPUT_REWIND)];
	[submenu addItem:item];
	[item release];

	[submenu addItem:[NSMenuItem separatorItem]];

	item = [[NSMenuItem alloc] initWithTitle:@"Fast Loading" action:@selector(do_set_state:) keyEquivalent:@""];
	[item setTag:(TAG_TAPE_FLAGS | TAPE_FAST)];
	[submenu addItem:item];
	[item release];

	item = [[NSMenuItem alloc] initWithTitle:@"Leader Padding" action:@selector(do_set_state:) keyEquivalent:@""];
	[item setTag:(TAG_TAPE_FLAGS | TAPE_PAD)];
	[submenu addItem:item];
	[item release];

	item = [[NSMenuItem alloc] initWithTitle:@"Automatic Padding" action:@selector(do_set_state:) keyEquivalent:@""];
	[item setTag:(TAG_TAPE_FLAGS | TAPE_PAD_AUTO)];
	[submenu addItem:item];
	[item release];

	item = [[NSMenuItem alloc] initWithTitle:@"Rewrite" action:@selector(do_set_state:) keyEquivalent:@""];
	[item setTag:(TAG_TAPE_FLAGS | TAPE_REWRITE)];
	[submenu addItem:item];
	[item release];

	item = [[NSMenuItem alloc] initWithTitle:@"Cassette" action:nil keyEquivalent:@""];
	[item setSubmenu:submenu];
	[file_menu addItem:item];
	[item release];

	[file_menu addItem:[NSMenuItem separatorItem]];

	int drive;
	for (drive = 0; drive < 4; drive++) {
		NSString *title = [NSString stringWithFormat:@"Drive %d", drive+1];
		NSString *key1 = [NSString stringWithFormat:@"%d", drive+1];
		NSString *key2 = [NSString stringWithFormat:@"%d", drive+5];
		NSString *tmp;

		submenu = [[NSMenu alloc] initWithTitle:title];

		tmp = [NSString stringWithFormat:@"Insert Disk%C", 0x2026];
		item = [[NSMenuItem alloc] initWithTitle:tmp action:@selector(do_set_state:) keyEquivalent:key1];
		[item setTag:(TAG_INSERT_DISK | drive)];
		[submenu addItem:item];
		[item release];
		[tmp release];

		tmp = [NSString stringWithFormat:@"New Disk%C", 0x2026];
		item = [[NSMenuItem alloc] initWithTitle:tmp action:@selector(do_set_state:) keyEquivalent:key1];
		[item setKeyEquivalentModifierMask:NSCommandKeyMask|NSShiftKeyMask];
		[item setTag:(TAG_NEW_DISK | drive)];
		[submenu addItem:item];
		[item release];
		[tmp release];

		[submenu addItem:[NSMenuItem separatorItem]];

		item = [[NSMenuItem alloc] initWithTitle:@"Write Enable" action:@selector(do_set_state:) keyEquivalent:key2];
		[item setTag:(TAG_WRITE_ENABLE | drive)];
		[submenu addItem:item];
		[item release];

		item = [[NSMenuItem alloc] initWithTitle:@"Write Back" action:@selector(do_set_state:) keyEquivalent:key2];
		[item setKeyEquivalentModifierMask:NSCommandKeyMask|NSShiftKeyMask];
		[item setTag:(TAG_WRITE_BACK | drive)];
		[submenu addItem:item];
		[item release];

		item = [[NSMenuItem alloc] initWithTitle:title action:nil keyEquivalent:@""];
		[item setSubmenu:submenu];
		[file_menu addItem:item];
		[item release];

		[key2 release];
		[key1 release];
		[title release];
	}

	[file_menu addItem:[NSMenuItem separatorItem]];

	tmp = [NSString stringWithFormat:@"Save Snapshot%C", 0x2026];
	item = [[NSMenuItem alloc] initWithTitle:tmp action:@selector(do_set_state:) keyEquivalent:@"s"];
	[item setTag:(TAG_SIMPLE_ACTION | TAG_FILE_SAVE_SNAPSHOT)];
	[file_menu addItem:item];
	[item release];
	[tmp release];

	file_menu_item = [[NSMenuItem alloc] initWithTitle:@"File" action:nil keyEquivalent:@""];
	[file_menu_item setSubmenu:file_menu];
	[[NSApp mainMenu] addItem:file_menu_item];
	[file_menu_item release];
}

static NSMenu *view_menu;

/* Create View menu */
static void setup_view_menu(void) {
	NSMenuItem *view_menu_item;
	NSMenuItem *item;
	NSMenu *submenu;
	int i;

	view_menu = [[NSMenu alloc] initWithTitle:@"View"];

	submenu = [[NSMenu alloc] initWithTitle:@"Zoom"];

	item = [[NSMenuItem alloc] initWithTitle:@"Zoom In" action:@selector(do_set_state:) keyEquivalent:@"+"];
	[item setTag:(TAG_SIMPLE_ACTION | TAG_ZOOM_IN)];
	[submenu addItem:item];
	[item release];

	item = [[NSMenuItem alloc] initWithTitle:@"Zoom Out" action:@selector(do_set_state:) keyEquivalent:@"-"];
	[item setTag:(TAG_SIMPLE_ACTION | TAG_ZOOM_OUT)];
	[submenu addItem:item];
	[item release];

	item = [[NSMenuItem alloc] initWithTitle:@"Zoom" action:nil keyEquivalent:@""];
	[item setSubmenu:submenu];
	[view_menu addItem:item];
	[item release];

	[view_menu addItem:[NSMenuItem separatorItem]];

	item = [[NSMenuItem alloc] initWithTitle:@"Full Screen" action:@selector(do_set_state:) keyEquivalent:@"f"];
	[item setTag:TAG_FULLSCREEN];
	[view_menu addItem:item];
	[item release];

	[view_menu addItem:[NSMenuItem separatorItem]];

	item = [[NSMenuItem alloc] initWithTitle:@"Inverse Text" action:@selector(do_set_state:) keyEquivalent:@"i"];
	[item setKeyEquivalentModifierMask:NSCommandKeyMask|NSShiftKeyMask];
	[item setTag:TAG_VDG_INVERSE];
	[view_menu addItem:item];
	[item release];

	submenu = [[NSMenu alloc] initWithTitle:@"Cross-colour"];

	for (i = 0; xroar_cross_colour_list[i].name; i++) {
		NSString *s = [[NSString alloc] initWithUTF8String:xroar_cross_colour_list[i].description];
		item = [[NSMenuItem alloc] initWithTitle:s action:@selector(do_set_state:) keyEquivalent:@""];
		[item setTag:(TAG_CROSS_COLOUR | xroar_cross_colour_list[i].value)];
		[item setOnStateImage:[NSImage imageNamed:@"NSMenuRadio"]];
		[submenu addItem:item];
		[item release];
		[s release];
	}

	item = [[NSMenuItem alloc] initWithTitle:@"Cross-colour" action:nil keyEquivalent:@""];
	[item setSubmenu:submenu];
	[view_menu addItem:item];
	[item release];

	view_menu_item = [[NSMenuItem alloc] initWithTitle:@"View" action:nil keyEquivalent:@""];
	[view_menu_item setSubmenu:view_menu];
	[[NSApp mainMenu] addItem:view_menu_item];
	[view_menu_item release];
}

static NSMenu *machine_menu;

/* Create Machine menu */
static void setup_machine_menu(void) {
	NSMenuItem *machine_menu_item;
	NSMenuItem *item;
	NSMenu *submenu;

	machine_menu = [[NSMenu alloc] initWithTitle:@"Machine"];

	[machine_menu addItem:[NSMenuItem separatorItem]];

	submenu = [[NSMenu alloc] initWithTitle:@"Keyboard Map"];

	item = [[NSMenuItem alloc] initWithTitle:@"Dragon Layout" action:@selector(do_set_state:) keyEquivalent:@""];
	[item setTag:(TAG_KEYMAP | KEYMAP_DRAGON)];
	[item setOnStateImage:[NSImage imageNamed:@"NSMenuRadio"]];
	[submenu addItem:item];
	[item release];

	item = [[NSMenuItem alloc] initWithTitle:@"Dragon 200-E Layout" action:@selector(do_set_state:) keyEquivalent:@""];
	[item setTag:(TAG_KEYMAP | KEYMAP_DRAGON200E)];
	[item setOnStateImage:[NSImage imageNamed:@"NSMenuRadio"]];
	[submenu addItem:item];
	[item release];

	item = [[NSMenuItem alloc] initWithTitle:@"CoCo Layout" action:@selector(do_set_state:) keyEquivalent:@""];
	[item setTag:(TAG_KEYMAP | KEYMAP_COCO)];
	[item setOnStateImage:[NSImage imageNamed:@"NSMenuRadio"]];
	[submenu addItem:item];
	[item release];

	item = [[NSMenuItem alloc] initWithTitle:@"Keyboard Map" action:nil keyEquivalent:@""];
	[item setSubmenu:submenu];
	[machine_menu addItem:item];
	[item release];

	[machine_menu addItem:[NSMenuItem separatorItem]];

	submenu = [[NSMenu alloc] initWithTitle:@"Right Joystick"];

	int joy;
	for (joy = 0; joy < 5; joy++) {
		item = [[NSMenuItem alloc] initWithTitle:joystick_names[joy].description action:@selector(do_set_state:) keyEquivalent:@""];
		[item setTag:(TAG_JOY_RIGHT | joy)];
		[item setOnStateImage:[NSImage imageNamed:@"NSMenuRadio"]];
		[submenu addItem:item];
		[item release];
	}

	item = [[NSMenuItem alloc] initWithTitle:@"Right Joystick" action:nil keyEquivalent:@""];
	[item setSubmenu:submenu];
	[machine_menu addItem:item];
	[item release];

	submenu = [[NSMenu alloc] initWithTitle:@"Left Joystick"];

	for (joy = 0; joy < 5; joy++) {
		item = [[NSMenuItem alloc] initWithTitle:joystick_names[joy].description action:@selector(do_set_state:) keyEquivalent:@""];
		[item setTag:(TAG_JOY_LEFT | joy)];
		[item setOnStateImage:[NSImage imageNamed:@"NSMenuRadio"]];
		[submenu addItem:item];
		[item release];
	}

	item = [[NSMenuItem alloc] initWithTitle:@"Left Joystick" action:nil keyEquivalent:@""];
	[item setSubmenu:submenu];
	[machine_menu addItem:item];
	[item release];

	item = [[NSMenuItem alloc] initWithTitle:@"Swap Joysticks" action:@selector(do_set_state:) keyEquivalent:@"J"];
	[item setTag:(TAG_SIMPLE_ACTION | TAG_JOY_SWAP)];
	[machine_menu addItem:item];
	[item release];

	[machine_menu addItem:[NSMenuItem separatorItem]];

	item = [[NSMenuItem alloc] initWithTitle:@"Soft Reset" action:@selector(do_set_state:) keyEquivalent:@"r"];
	[item setTag:(TAG_SIMPLE_ACTION | TAG_RESET_SOFT)];
	[machine_menu addItem:item];
	[item release];

	item = [[NSMenuItem alloc] initWithTitle:@"Hard Reset" action:@selector(do_set_state:) keyEquivalent:@"R"];
	[item setTag:(TAG_SIMPLE_ACTION | TAG_RESET_HARD)];
	[machine_menu addItem:item];
	[item release];

	machine_menu_item = [[NSMenuItem alloc] initWithTitle:@"Machine" action:nil keyEquivalent:@""];
	[machine_menu_item setSubmenu:machine_menu];
	[[NSApp mainMenu] addItem:machine_menu_item];
	[machine_menu_item release];
}

static NSMenu *cartridge_menu;

/* Create Cartridge menu */
static void setup_cartridge_menu(void) {
	NSMenuItem *cartridge_menu_item;

	cartridge_menu = [[NSMenu alloc] initWithTitle:@"Cartridge"];

	cartridge_menu_item = [[NSMenuItem alloc] initWithTitle:@"Cartridge" action:nil keyEquivalent:@""];
	[cartridge_menu_item setSubmenu:cartridge_menu];
	[[NSApp mainMenu] addItem:cartridge_menu_item];
	[cartridge_menu_item release];
}

/* Create Tool menu */
static void setup_tool_menu(void) {
	NSMenu *tool_menu;
	NSMenuItem *tool_menu_item;
	NSMenuItem *item;

	tool_menu = [[NSMenu alloc] initWithTitle:@"Tool"];

	item = [[NSMenuItem alloc] initWithTitle:@"Keyboard Translation" action:@selector(do_set_state:) keyEquivalent:@"z"];
	[item setTag:TAG_KBD_TRANSLATE];
	[tool_menu addItem:item];
	[item release];

	item = [[NSMenuItem alloc] initWithTitle:@"Fast Sound" action:@selector(do_set_state:) keyEquivalent:@""];
	[item setTag:TAG_FAST_SOUND];
	[tool_menu addItem:item];
	[item release];

	tool_menu_item = [[NSMenuItem alloc] initWithTitle:@"Tool" action:nil keyEquivalent:@""];
	[tool_menu_item setSubmenu:tool_menu];
	[[NSApp mainMenu] addItem:tool_menu_item];
	[tool_menu_item release];
}

/* Create a window menu */
static void setup_window_menu(void) {
	NSMenu *window_menu;
	NSMenuItem *window_menu_item;
	NSMenuItem *item;

	window_menu = [[NSMenu alloc] initWithTitle:@"Window"];

	/* "Minimize" item */
	item = [[NSMenuItem alloc] initWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
	[window_menu addItem:item];
	[item release];

	/* Put menu into the menubar */
	window_menu_item = [[NSMenuItem alloc] initWithTitle:@"Window" action:nil keyEquivalent:@""];
	[window_menu_item setSubmenu:window_menu];
	[[NSApp mainMenu] addItem:window_menu_item];

	/* Tell the application object that this is now the window menu */
	[NSApp setWindowsMenu:window_menu];

	/* Finally give up our references to the objects */
	[window_menu release];
	[window_menu_item release];
}

/* Replacement for NSApplicationMain */
static void CustomApplicationMain(int argc, char **argv) {
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	SDLMain *sdlMain;

	(void)argc;
	(void)argv;

	/* Ensure the application object is initialised */
	[SDLApplication sharedApplication];

#ifdef SDL_USE_CPS
	{
		CPSProcessSerNum PSN;
		/* Tell the dock about us */
		if (!CPSGetCurrentProcess(&PSN))
			if (!CPSEnableForegroundOperation(&PSN,0x03,0x3C,0x2C,0x1103))
				if (!CPSSetFrontProcess(&PSN))
					[SDLApplication sharedApplication];
	}
#endif /* SDL_USE_CPS */

	/* Set up the menubar */
	[NSApp setMainMenu:[[NSMenu alloc] init]];
	setApplicationMenu();
	setup_file_menu();
	setup_view_menu();
	setup_machine_menu();
	setup_cartridge_menu();
	setup_tool_menu();
	setup_window_menu();

	/* Create SDLMain and make it the app delegate */
	sdlMain = [[SDLMain alloc] init];
	[NSApp setDelegate:sdlMain];

	/* Start the main event loop */
	[NSApp run];

	[sdlMain release];
	[pool release];
}


/*
 * Catch document open requests...this lets us notice files when the app was
 * launched by double-clicking a document, or when a document was
 * dragged/dropped on the app's icon. You need to have a CFBundleDocumentsType
 * section in your Info.plist to get this message, apparently.
 *
 * Files are added to gArgv, so to the app, they'll look like command line
 * arguments. Previously, apps launched from the finder had nothing but an
 * argv[0].
 *
 * This message may be received multiple times to open several docs on launch.
 *
 * This message is ignored once the app's mainline has been called.
 */
- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename {
	const char *temparg;
	size_t arglen;
	char *arg;
	char **newargv;

	(void)theApplication;

	if (!gFinderLaunch)  /* MacOS is passing command line args. */
		return FALSE;

	if (gCalledAppMainline)  /* app has started, ignore this document. */
		return FALSE;

	temparg = [filename UTF8String];
	arglen = SDL_strlen(temparg) + 1;
	arg = (char *)SDL_malloc(arglen);
	if (arg == NULL)
		return FALSE;

	newargv = (char **)realloc(gArgv, sizeof(char *) * (gArgc + 2));
	if (newargv == NULL) {
		SDL_free(arg);
		return FALSE;
	}
	gArgv = newargv;

	SDL_strlcpy(arg, temparg, arglen);
	gArgv[gArgc++] = arg;
	gArgv[gArgc] = NULL;
	return TRUE;
}


/* Called when the internal event loop has just started running */
- (void)applicationDidFinishLaunching: (NSNotification *)note {
	int status;

	(void)note;
	/* Set the working directory to the .app's parent directory */
	[self setupWorkingDirectory:gFinderLaunch];
	/* Pass keypresses etc. to Cocoa */
	setenv("SDL_ENABLEAPPEVENTS", "1", 1);
	/* Hand off to main application code */
	gCalledAppMainline = TRUE;
	status = SDL_main (gArgc, gArgv);
	/* We're done, thank you for playing */
	exit(status);
}

@end


#ifdef main
#  undef main
#endif


/* Main entry point to executable - should *not* be SDL_main! */
int main(int argc, char **argv) {
	/* Copy the arguments into a global variable */
	/* This is passed if we are launched by double-clicking */
	if (argc >= 2 && strncmp(argv[1], "-psn", 4) == 0) {
		gArgv = (char **)SDL_malloc(sizeof(char *) * 2);
		gArgv[0] = argv[0];
		gArgv[1] = NULL;
		gArgc = 1;
		gFinderLaunch = YES;
	} else {
		int i;
		gArgc = argc;
		gArgv = (char **)SDL_malloc(sizeof(char *) * (argc+1));
		for (i = 0; i <= argc; i++)
			gArgv[i] = argv[i];
		gFinderLaunch = NO;
	}

	CustomApplicationMain(argc, argv);
	return 0;
}

/**************************************************************************/

/* XRoar UI definition */

static _Bool init(void);
static void shutdown(void);
static void cross_colour_changed_cb(int cc);
static void machine_changed_cb(int machine_type);
static void keymap_changed_cb(int map);
static void kbd_translate_changed_cb(_Bool kbd_translate);
static void cart_changed_cb(int cart_index);
static void fullscreen_changed_cb(_Bool fullscreen);
static void fast_sound_changed_cb(_Bool fast_sound);
static void update_drive_write_enable(int drive, _Bool write_enable);
static void update_drive_write_back(int drive, _Bool write_back);

static void update_machine_menu(void);
static void update_cartridge_menu(void);

UIModule ui_macosx_module = {
	.common = { .name = "macosx", .description = "Mac OS X SDL UI",
	            .init = init, .shutdown = shutdown },
	.video_module_list = sdl_video_module_list,
	.keyboard_module_list = sdl_keyboard_module_list,
	.joystick_module_list = sdl_js_modlist,
	.run = sdl_run,
	.fullscreen_changed_cb = fullscreen_changed_cb,
	.cross_colour_changed_cb = cross_colour_changed_cb,
	.machine_changed_cb = machine_changed_cb,
	.keymap_changed_cb = keymap_changed_cb,
	.kbd_translate_changed_cb = kbd_translate_changed_cb,
	.fast_sound_changed_cb = fast_sound_changed_cb,
	.cart_changed_cb = cart_changed_cb,
	.update_drive_write_enable = update_drive_write_enable,
	.update_drive_write_back = update_drive_write_back,
};

static _Bool init(void) {
	if (!SDL_WasInit(SDL_INIT_NOPARACHUTE)) {
		if (SDL_Init(SDL_INIT_NOPARACHUTE) < 0) {
			LOG_ERROR("Failed to initialise SDL: %s\n", SDL_GetError());
			return 0;
		}
	}

	if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
		LOG_ERROR("Failed to initialise SDL video: %s\n", SDL_GetError());
		return 0;
	}

	update_machine_menu();
	update_cartridge_menu();

	return 1;
}

static void shutdown(void) {
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

static void update_machine_menu(void) {
	int num_machines = machine_config_count();
	int i;
	NSMenuItem *item;

	if (xroar_machine_config) current_machine = TAG_MACHINE | (xroar_machine_config->index & TAG_VALUE_MASK);

	for (i = num_machines-1; i >= 0; i--) {
		struct machine_config *mc = machine_config_index(i);
		NSString *description = [[NSString alloc] initWithUTF8String:mc->description];
		item = [[NSMenuItem alloc] initWithTitle:description action:@selector(do_set_state:) keyEquivalent:@""];
		[item setTag:(TAG_MACHINE | mc->index)];
		[item setOnStateImage:[NSImage imageNamed:@"NSMenuRadio"]];
		[description release];
		[machine_menu insertItem:item atIndex:0];
		[item release];
	}
}

static void update_cartridge_menu(void) {
	int num_carts = cart_config_count();
	int i;
	NSMenuItem *item;

	if (xroar_cart)
		current_cartridge = TAG_CARTRIDGE | (xroar_cart->config->index & TAG_VALUE_MASK);

	for (i = num_carts-1; i >= 0; i--) {
		struct cart_config *cc = cart_config_index(i);
		NSString *description = [[NSString alloc] initWithUTF8String:cc->description];
		item = [[NSMenuItem alloc] initWithTitle:description action:@selector(do_set_state:) keyEquivalent:@""];
		[item setTag:(TAG_CARTRIDGE | cc->index)];
		[item setOnStateImage:[NSImage imageNamed:@"NSMenuRadio"]];
		[description release];
		[cartridge_menu insertItem:item atIndex:0];
		[item release];
	}
	item = [[NSMenuItem alloc] initWithTitle:@"None" action:@selector(do_set_state:) keyEquivalent:@""];
	[item setTag:(TAG_CARTRIDGE | (-1 & TAG_VALUE_MASK))];
	[cartridge_menu insertItem:item atIndex:0];
	[item release];
}

static void cross_colour_changed_cb(int cc) {
	current_cc = TAG_CROSS_COLOUR | (cc & TAG_VALUE_MASK);
}

static void machine_changed_cb(int machine_type) {
	current_machine = TAG_MACHINE | (machine_type & TAG_VALUE_MASK);
}

static void keymap_changed_cb(int map) {
	current_keymap = TAG_KEYMAP | (map & TAG_VALUE_MASK);
}

static void kbd_translate_changed_cb(_Bool kbd_translate) {
	is_kbd_translate = kbd_translate;
}

static void cart_changed_cb(int cart_index) {
	current_cartridge = TAG_CARTRIDGE | (cart_index & TAG_VALUE_MASK);
}

static void fullscreen_changed_cb(_Bool fullscreen) {
	is_fullscreen = fullscreen;
}

static void fast_sound_changed_cb(_Bool fast_sound) {
	is_fast_sound = fast_sound;
}

static void update_drive_write_enable(int drive, _Bool write_enable) {
	if (drive >= 0 && drive <= 3)
		disk_write_enable[drive] = write_enable;
}

static void update_drive_write_back(int drive, _Bool write_back) {
	if (drive >= 0 && drive <= 3)
		disk_write_back[drive] = write_back;
}
