/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2005  Ciaran Anscomb
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

#include "config.h"

#ifdef HAVE_CARBON_UI

#include <Carbon/Carbon.h>
#include <CoreServices/CoreServices.h>

#include "ui.h"

static int init(void);
static void shutdown(void);
static void menu(void);
static char *get_filename(char *extension);

UIModule ui_carbon_module = {
	NULL,
	"carbon",
	"MacOS X Carbon interface",
	init, shutdown,
	menu, get_filename
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

static char *get_filename(char *extension){
	NavDialogCreationOptions options;
	NavDialogRef mydialog;
        
	OSStatus status;
        NavReplyRecord filespec; 
        AEDesc filedesc;
        FSRef fileref;
        UInt8* filename;
        int le, length=0; 

       	status = NavGetDefaultDialogCreationOptions (&options);
       	status = NavCreateChooseFileDialog ( &options, NULL, NULL, \
                NULL, NULL, NULL, &mydialog);
     status = NavDialogRun( mydialog );
        status = NavDialogGetReply ( mydialog, &filespec );
        status = AEGetNthDesc (&filespec.selection, 1, typeWildCard, \
                NULL, &filedesc);
        status = NavDisposeReply (&filespec);
       status = AEGetDescData (&filedesc, &fileref, sizeof(FSRef));
       le = FALSE;
        while( ! le ){   
        if (FSRefMakePath(&fileref, filename, length) == pathTooLongErr){
                length +=1;
                filename = (UInt8 *) malloc(length);
                }
        else
                {
                le = TRUE;
                }
        }
	return (char *)filename;
}

#endif  /* HAVE_CARBON_UI */
