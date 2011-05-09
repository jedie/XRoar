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
#include "keyboard.h"
#include "machine.h"
#include "module.h"
#include "xroar.h"

#define TAG_TYPE_MASK (0xff << 24)
#define TAG_VALUE_MASK (0xffffff)
#define TAG_MACHINE (1 << 24)
#define TAG_CARTRIDGE (2 << 24)
#define TAG_FULLSCREEN (3 << 24)
#define TAG_KEYMAP (4 << 24)
#define TAG_KBD_TRANSLATE (5 << 24)
#define TAG_CC (6 << 24)
#define TAG_FAST_SOUND (7 << 24)

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
static int is_fast_sound = 0;

/* Setting this to true is a massive hack so that cocoa file dialogues receive
 * keypresses.  Ideally, need to sort SDL out or turn this into a regular
 * OpenGL application. */
int cocoa_super_all_keys = 0;

@interface SDLApplication : NSApplication
@end

@implementation SDLApplication
/* Invoked from the Quit menu item */
- (void)terminate:(id)sender {
	(void)sender;
	/* Post a SDL_QUIT event */
	SDL_Event event;
	event.type = SDL_QUIT;
	SDL_PushEvent(&event);
}

- (void)sendEvent:(NSEvent *)anEvent {
	if (NSKeyDown == [anEvent type] || NSKeyUp == [anEvent type]) {
		if (cocoa_super_all_keys || ([anEvent modifierFlags] & NSCommandKeyMask))
			[super sendEvent:anEvent];
	} else {
		[super sendEvent:anEvent];
	}
}

- (void)do_run_file:(id)sender {
	(void)sender;
	xroar_run_file(NULL);
}

- (void)do_load_file:(id)sender {
	(void)sender;
	xroar_load_file(NULL);
}

- (void)do_save_snapshot:(id)sender {
	(void)sender;
	xroar_save_snapshot();
}

- (void)do_set_cc:(id)sender {
	current_cc = [sender tag];
	int set_to = current_cc & TAG_VALUE_MASK;
	xroar_select_cross_colour(set_to);
}

- (void)do_soft_reset:(id)sender {
	(void)sender;
	xroar_soft_reset();
}

- (void)do_hard_reset:(id)sender {
	(void)sender;
	xroar_hard_reset();
}

- (void)set_machine:(id)sender {
	current_machine = [sender tag];
	int set_to = current_machine & TAG_VALUE_MASK;
	xroar_set_machine(set_to);
}

- (void)do_set_keymap:(id)sender {
	current_keymap = [sender tag];
	xroar_set_keymap(current_keymap & TAG_VALUE_MASK);
}

- (void)set_cartridge:(id)sender {
	current_cartridge = [sender tag];
	int set_to = current_cartridge & TAG_VALUE_MASK;
	if (set_to & (1 << 23)) {
		set_to |= (0xff << 24);
	}
	xroar_set_cart(set_to);
}


- (void)do_fullscreen:(id)sender {
	(void)sender;
	is_fullscreen = !is_fullscreen;
	xroar_fullscreen(is_fullscreen);
}

- (void)do_kbd_translate:(id)sender {
	(void)sender;
	is_kbd_translate = !is_kbd_translate;
	xroar_set_kbd_translate(is_kbd_translate);
}

- (void)do_fast_sound:(id)sender {
	(void)sender;
	is_fast_sound = !is_fast_sound;
	machine_set_fast_sound(is_fast_sound);
}

- (BOOL)validateMenuItem:(NSMenuItem *)item {
	int tag = [item tag];
	if ((tag & TAG_TYPE_MASK) == TAG_MACHINE) {
		[item setState:((tag == current_machine) ? NSOnState : NSOffState)];
	}
	if ((tag & TAG_TYPE_MASK) == TAG_KEYMAP) {
		[item setState:((tag == current_keymap) ? NSOnState : NSOffState)];
	}
	if ((tag & TAG_TYPE_MASK) == TAG_CARTRIDGE) {
		[item setState:((tag == current_cartridge) ? NSOnState : NSOffState)];
	}
	if ((tag & TAG_TYPE_MASK) == TAG_FULLSCREEN) {
		[item setState:(is_fullscreen ? NSOnState : NSOffState)];
	}
	if ((tag & TAG_TYPE_MASK) == TAG_KBD_TRANSLATE) {
		[item setState:(is_kbd_translate ? NSOnState : NSOffState)];
	}
	if ((tag & TAG_TYPE_MASK) == TAG_FAST_SOUND) {
		[item setState:(is_fast_sound ? NSOnState : NSOffState)];
	}
	if ((tag & TAG_TYPE_MASK) == TAG_CC) {
		[item setState:((tag == current_cc) ? NSOnState : NSOffState)];
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
	[apple_menu addItemWithTitle:title action:@selector(terminate:) keyEquivalent:@"q"];


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

	file_menu = [[NSMenu alloc] initWithTitle:@"File"];

	item = [[NSMenuItem alloc] initWithTitle:@"Run" action:@selector(do_run_file:) keyEquivalent:@"L"];
	[file_menu addItem:item];
	[item release];

	item = [[NSMenuItem alloc] initWithTitle:@"Load" action:@selector(do_load_file:) keyEquivalent:@"l"];
	[file_menu addItem:item];
	[item release];

	[file_menu addItem:[NSMenuItem separatorItem]];

	item = [[NSMenuItem alloc] initWithTitle:@"Save Snapshot" action:@selector(do_save_snapshot:) keyEquivalent:@"s"];
	[file_menu addItem:item];
	[item release];

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

	item = [[NSMenuItem alloc] initWithTitle:@"Full Screen" action:@selector(do_fullscreen:) keyEquivalent:@"f"];
	[item setTag:TAG_FULLSCREEN];
	[view_menu addItem:item];
	[item release];

	[view_menu addItem:[NSMenuItem separatorItem]];

	submenu = [[NSMenu alloc] initWithTitle:@"Cross-colour"];

	for (i = 0; xroar_cross_colour_list[i].name; i++) {
		NSString *s = [[NSString alloc] initWithUTF8String:xroar_cross_colour_list[i].description];
		item = [[NSMenuItem alloc] initWithTitle:s action:@selector(do_set_cc:) keyEquivalent:@""];
		[item setTag:(TAG_CC | xroar_cross_colour_list[i].value)];
		[item setOnStateImage:[NSImage imageNamed:@"NSMenuRadio"]];
		[submenu addItem:item];
		[item release];
		[s release];
	}

	item = [[NSMenuItem alloc] initWithTitle:@"Cross-colour" action:nil keyEquivalent:@""];
	[item setSubmenu:submenu];
	[view_menu addItem:item];
	[item release];

	view_menu_item = [[NSMenuItem alloc] initWithTitle:@"Machine" action:nil keyEquivalent:@""];
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

	item = [[NSMenuItem alloc] initWithTitle:@"Dragon Layout" action:@selector(do_set_keymap:) keyEquivalent:@""];
	[item setTag:(TAG_KEYMAP | KEYMAP_DRAGON)];
	[item setOnStateImage:[NSImage imageNamed:@"NSMenuRadio"]];
	[submenu addItem:item];
	[item release];

	item = [[NSMenuItem alloc] initWithTitle:@"CoCo Layout" action:@selector(do_set_keymap:) keyEquivalent:@""];
	[item setTag:(TAG_KEYMAP | KEYMAP_COCO)];
	[item setOnStateImage:[NSImage imageNamed:@"NSMenuRadio"]];
	[submenu addItem:item];
	[item release];

	item = [[NSMenuItem alloc] initWithTitle:@"Keyboard Map" action:nil keyEquivalent:@""];
	[item setSubmenu:submenu];
	[machine_menu addItem:item];
	[item release];

	[machine_menu addItem:[NSMenuItem separatorItem]];

	item = [[NSMenuItem alloc] initWithTitle:@"Soft Reset" action:@selector(do_soft_reset:) keyEquivalent:@"r"];
	[machine_menu addItem:item];
	[item release];

	item = [[NSMenuItem alloc] initWithTitle:@"Hard Reset" action:@selector(do_hard_reset:) keyEquivalent:@"R"];
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

	item = [[NSMenuItem alloc] initWithTitle:@"Keyboard Translation" action:@selector(do_kbd_translate:) keyEquivalent:@"z"];
	[item setTag:TAG_KBD_TRANSLATE];
	[tool_menu addItem:item];
	[item release];

	item = [[NSMenuItem alloc] initWithTitle:@"Fast Sound" action:@selector(do_fast_sound:) keyEquivalent:@""];
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

static int init(void);
void sdl_run(void);
static void cross_colour_changed_cb(int cc);
static void machine_changed_cb(int machine_type);
static void keymap_changed_cb(int keymap);
static void cart_changed_cb(int cart_index);
static void fullscreen_changed_cb(int fullscreen);
static void kbd_translate_changed_cb(int kbd_translate);
static void fast_sound_changed_cb(int fast_sound);

static void update_machine_menu(void);
static void update_cartridge_menu(void);

extern VideoModule video_sdlgl_module;
extern VideoModule video_sdlyuv_module;
extern VideoModule video_sdl_module;
static VideoModule *sdl_video_module_list[] = {
#ifdef HAVE_SDLGL
	&video_sdlgl_module,
#endif
#ifdef PREFER_NOYUV
	&video_sdl_module,
	&video_sdlyuv_module,
#else
	&video_sdlyuv_module,
	&video_sdl_module,
#endif
	NULL
};

extern KeyboardModule keyboard_sdl_module;
static KeyboardModule *sdl_keyboard_module_list[] = {
	&keyboard_sdl_module,
	NULL
};

UIModule ui_macosx_module = {
	.common = { .name = "macosx", .description = "Mac OS X SDL user-interface",
	            .init = init },
	.video_module_list = sdl_video_module_list,
	.keyboard_module_list = sdl_keyboard_module_list,
	.run = sdl_run,
	.cross_colour_changed_cb = cross_colour_changed_cb,
	.machine_changed_cb = machine_changed_cb,
	.keymap_changed_cb = keymap_changed_cb,
	.fast_sound_changed_cb = fast_sound_changed_cb,
	.cart_changed_cb = cart_changed_cb,
};

static int init(void) {
	update_machine_menu();
	update_cartridge_menu();
	xroar_fullscreen_changed_cb = fullscreen_changed_cb;
	xroar_kbd_translate_changed_cb = kbd_translate_changed_cb;
	return 0;
}

static void update_machine_menu(void) {
	int num_machines = machine_config_count();
	int i;
	NSMenuItem *item;

	if (xroar_machine_config) current_machine = TAG_MACHINE | (xroar_machine_config->index & TAG_VALUE_MASK);

	for (i = num_machines-1; i >= 0; i--) {
		struct machine_config *mc = machine_config_index(i);
		NSString *description = [[NSString alloc] initWithCString:mc->description];
		item = [[NSMenuItem alloc] initWithTitle:description action:@selector(set_machine:) keyEquivalent:@""];
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

	if (xroar_cart_config) current_cartridge = TAG_CARTRIDGE | (xroar_cart_config->index & TAG_VALUE_MASK);

	for (i = num_carts-1; i >= 0; i--) {
		struct cart_config *cc = cart_config_index(i);
		NSString *description = [[NSString alloc] initWithCString:cc->description];
		item = [[NSMenuItem alloc] initWithTitle:description action:@selector(set_cartridge:) keyEquivalent:@""];
		[item setTag:(TAG_CARTRIDGE | cc->index)];
		[item setOnStateImage:[NSImage imageNamed:@"NSMenuRadio"]];
		[description release];
		[cartridge_menu insertItem:item atIndex:0];
		[item release];
	}
	item = [[NSMenuItem alloc] initWithTitle:@"None" action:@selector(set_cartridge:) keyEquivalent:@""];
	[item setTag:(TAG_CARTRIDGE | (-1 & TAG_VALUE_MASK))];
	[cartridge_menu insertItem:item atIndex:0];
	[item release];
}

static void cross_colour_changed_cb(int cc) {
	current_cc = TAG_CC | (cc & TAG_VALUE_MASK);
}

static void machine_changed_cb(int machine_type) {
	current_machine = TAG_MACHINE | (machine_type & TAG_VALUE_MASK);
}

static void keymap_changed_cb(int keymap) {
	current_keymap = TAG_KEYMAP | (keymap & TAG_VALUE_MASK);
}

static void cart_changed_cb(int cart_index) {
	current_cartridge = TAG_CARTRIDGE | (cart_index & TAG_VALUE_MASK);
}

static void fullscreen_changed_cb(int fullscreen) {
	is_fullscreen = fullscreen;
}

static void kbd_translate_changed_cb(int kbd_translate) {
	is_kbd_translate = kbd_translate;
}

static void fast_sound_changed_cb(int fast_sound) {
	is_fast_sound = fast_sound;
}
