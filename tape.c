/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2004  Ciaran Anscomb
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

#include "m6809.h"
#include "tape.h"
#include "fs.h"
#include "ui.h"
#include "keyboard.h"
#include "types.h"
#include "logging.h"

static FS_FILE tapefd;
static uint_fast8_t count = 0;
static uint_fast8_t tape_byte = 0;
static int isopen = 0;
static char buffer[512];
static uint_fast32_t bufferpos;

void tape_init(void) {
	isopen = 0;
}

void tape_reset(void) {
	if (isopen) fs_close(tapefd);
	isopen = 0;
}

/* Close any currently-open tape file, open a new one and read the first
 * bufferful of data.  Tries to guess the filetype.  Returns -1 on error,
 * 0 for a BASIC program, 1 for data and 2 for M/C. */
int tape_attach(char *filename) {
	int b, state, reload, type;
	if (filename == NULL)
		return -1;
	tape_detach();
	if ((tapefd = fs_open(filename, FS_READ)) == -1)
		return -1;
	isopen = 1;
	fs_read(tapefd, buffer, sizeof(buffer));
	bufferpos = 0;
	/* Little state machine to try and determine type of first
	 * file on tape */
	b = 0;
	state = 0;
	reload = 0;
	type = 1;  /* default to data - no autorun (if trying to) */
	do {
		if (b >= sizeof(buffer)) {
			fs_read(tapefd, buffer, sizeof(buffer));
			b = 0;
			reload = 1;
		}
		switch(state) {
			case 0:
				if (buffer[b] != 0x55) state = -1;
				if (buffer[b] == 0x3c) state = 1;
				break;
			case 1:
				state = 2;
				if (buffer[b] != 0x00) state = -1;
				break;
			case 2:
				state = -1;
				type = buffer[b+9];
				break;
			default:
				break;
		}
		b++;
	} while (state >= 0);
	if (reload) {
		fs_close(tapefd);
		fs_open(filename, FS_READ);
		fs_read(tapefd, buffer, sizeof(buffer));
	}
	return type;
}

void tape_detach(void) {
	if (isopen)
		fs_close(tapefd);
	isopen = 0;
}

int tape_autorun(char *filename) {
	int type = tape_attach(filename);
	if (type < 0)
		return type;
	switch (type) {
		/* BASIC programs don't autorun yet */ 
		case 0: keyboard_queue_string("CLOAD\r");
			return 0;
			break;
		case 2: keyboard_queue_string("CLOADM:EXEC\r");
			return 0;
			break;
		default:
			return 1;
			break;
	}
}

uint_fast8_t tape_read_bit(void) {
	uint_fast8_t ret;
	if (!isopen)
		return 0;
	if (!count) {
		tape_byte = buffer[bufferpos++];
		count = 8;
		if (bufferpos >= sizeof(buffer)) {
			fs_read(tapefd, buffer, sizeof(buffer));
			bufferpos = 0;
		}
	}
	ret = tape_byte & 1;
	tape_byte >>= 1;
	count--;
	return ret;
}
