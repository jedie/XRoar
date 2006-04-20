/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2006  Ciaran Anscomb
 *  This file Copyright (C) 2004 Stuart Teasdale
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

#ifdef HAVE_CARBON

#include <Carbon/Carbon.h>
#include <CoreServices/CoreServices.h>

#include "logging.h"
#include "ui.h"

static int init(void);
static void shutdown(void);
static void menu(void);
static char *load_filename(const char **extensions);
static char *save_filename(const char **extensions);

UIModule ui_carbon_module = {
	NULL,
	"carbon",
	"MacOS X Carbon interface",
	init, shutdown,
	menu, load_filename, save_filename
};

static int init(void){
	LOG_DEBUG(2, "Initialising Carbon interface\n");
	return 0;
}

static void shutdown(void) {
	LOG_DEBUG(2, "Shutting down Carbon interface\n");
}

static void menu(void) {
}

static char *load_filename(const char **extensions) {
	NavDialogCreationOptions options;
	NavDialogRef load_dialog;
        
	OSStatus status;
        NavReplyRecord replyrecord; 
        AEDesc filedesc;
        FSRef fileref;
        static UInt8 filename[768];

	(void)extensions;  /* unused */
       	status = NavGetDefaultDialogCreationOptions(&options);
       	status = NavCreateChooseFileDialog(&options, NULL, NULL, NULL, NULL, NULL, &load_dialog);
	status = NavDialogRun(load_dialog);
        status = NavDialogGetReply(load_dialog, &replyrecord);
	NavDialogDispose(load_dialog);
        status = AEGetNthDesc(&replyrecord.selection, 1, typeWildCard, NULL, &filedesc);
	status = NavDisposeReply(&replyrecord);
	status = AEGetDescData(&filedesc, &fileref, sizeof(FSRef));
	if (FSRefMakePath(&fileref, filename, sizeof(filename)) == noErr)
		return (char *)filename;
	return NULL;
}

static char *save_filename(const char **extensions) {
	NavDialogCreationOptions options;
	NavDialogRef save_dialog;
        NavReplyRecord replyrecord; 
        AEDesc filedesc;
        FSRef fileref;
        static char savename[768];
        static char filename[768];

	(void)extensions;  /* unused */
       	NavGetDefaultDialogCreationOptions(&options);
       	NavCreatePutFileDialog(&options, 0, 0, NULL, NULL, &save_dialog);
	NavDialogRun(save_dialog);
        NavDialogGetReply(save_dialog, &replyrecord);
	NavDialogDispose(save_dialog);
	if (!replyrecord.validRecord) {
		NavDisposeReply(&replyrecord);
		return NULL;
	}
        AEGetNthDesc(&replyrecord.selection, 1, typeWildCard, NULL, &filedesc);
	AEGetDescData(&filedesc, &fileref, sizeof(FSRef));
	filename[0] = 0;
	CFStringGetCString(replyrecord.saveFileName, savename, sizeof(savename), kCFStringEncodingUTF8);
	NavDisposeReply(&replyrecord);
	if (FSRefMakePath(&fileref, (UInt8 *)filename, sizeof(filename)) == noErr) {
		strcat(filename, "/");
		strcat(filename, savename);
		return (char *)filename;
	}
	return NULL;
}

#endif  /* HAVE_CARBON */
