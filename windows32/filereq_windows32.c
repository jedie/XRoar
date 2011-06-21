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

/* This Windows32 code is probably all wrong, but it does seem to work */

#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <commdlg.h>

#include "types.h"
#include "logging.h"
#include "fs.h"
#include "module.h"
#include "windows32/common_windows32.h"

static int init(void);
static void shutdown_module(void);
static char *load_filename(const char **extensions);
static char *save_filename(const char **extensions);

FileReqModule filereq_windows32_module = {
	{ "windows32", "Windows32 file requester",
	  init, 0, shutdown_module },
	load_filename, save_filename
};

static char *filename = NULL;

static int init(void) {
	LOG_DEBUG(2, "Windows32 file requester selected.\n");
	return 0;
}

static void shutdown_module(void) {
}

static char *load_filename(const char **extensions) {
	OPENFILENAME ofn;
	char fn_buf[260];
	int was_fullscreen;

	(void)extensions;  /* unused */
	was_fullscreen = video_module->is_fullscreen;
	if (video_module->set_fullscreen && was_fullscreen)
		video_module->set_fullscreen(0);

	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = windows32_main_hwnd;
	ofn.lpstrFile = fn_buf;
	ofn.lpstrFile[0] = '\0';
	ofn.nMaxFile = sizeof(fn_buf);
	ofn.lpstrFilter = "All\0*.*\0Snapshots\0*.SNA\0Cassette images\0*.CAS\0Virtual discs\0*.VDK;*.DSK;*.DMK;*.JVC\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR
		| OFN_HIDEREADONLY;

	if (filename)
		free(filename);
	filename = NULL;
	if (GetOpenFileName(&ofn)==TRUE) {
		filename = strdup(ofn.lpstrFile);
	}
	if (video_module->set_fullscreen && was_fullscreen)
		video_module->set_fullscreen(1);
	return filename;
}

static char *save_filename(const char **extensions) {
	OPENFILENAME ofn;
	char fn_buf[260];
	int was_fullscreen;

	(void)extensions;  /* unused */
	was_fullscreen = video_module->is_fullscreen;
	if (video_module->set_fullscreen && was_fullscreen)
		video_module->set_fullscreen(0);

	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = windows32_main_hwnd;
	ofn.lpstrFile = fn_buf;
	ofn.lpstrFile[0] = '\0';
	ofn.nMaxFile = sizeof(fn_buf);
	ofn.lpstrFilter = "All\0*.*\0Snapshots\0*.SNA\0Cassette images\0*.CAS\0Virtual discs\0*.VDK;*.DSK;*.DMK;*.JVC\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_HIDEREADONLY
		| OFN_OVERWRITEPROMPT;

	if (filename)
		free(filename);
	filename = NULL;
	if (GetSaveFileName(&ofn)==TRUE) {
		filename = strdup(ofn.lpstrFile);
	}
	if (video_module->set_fullscreen && was_fullscreen)
		video_module->set_fullscreen(1);
	return filename;
}
