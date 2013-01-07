/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 */

#include "config.h"

#import <Cocoa/Cocoa.h>

#include "module.h"

static char *load_filename(const char **extensions);
static char *save_filename(const char **extensions);

FileReqModule filereq_cocoa_module = {
	.common = { .name = "cocoa", .description = "Cocoa file requester" },
	.load_filename = load_filename,
	.save_filename = save_filename
};

extern int cocoa_super_all_keys;
static char *filename = NULL;

/* Assuming filenames are UTF8 strings seems to do the job */

static char *load_filename(const char **extensions) {
	NSOpenPanel *dialog = [NSOpenPanel openPanel];
	(void)extensions;
	cocoa_super_all_keys = 1;
	if (filename) {
		free(filename);
		filename = NULL;
	}
	if ([dialog runModal] == NSFileHandlingPanelOKButton) {
		filename = strdup([[[[dialog URLs] objectAtIndex:0] path] UTF8String]);
	}
	cocoa_super_all_keys = 0;
	return filename;
}

static char *save_filename(const char **extensions) {
	NSSavePanel *dialog = [NSSavePanel savePanel];
	(void)extensions;
	cocoa_super_all_keys = 1;
	if (filename) {
		free(filename);
		filename = NULL;
	}
	if ([dialog runModal] == NSFileHandlingPanelOKButton) {
		filename = strdup([[[dialog URL] path] UTF8String]);
	}
	cocoa_super_all_keys = 0;
	return filename;
}
