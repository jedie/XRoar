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

#define _POSIX_C_SOURCE 200112L

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "portalib/glib.h"

#include "printer.h"
#include "events.h"
#include "machine.h"
#include "mc6821.h"
#include "xroar.h"
#include "logging.h"

static FILE *stream;
static char *stream_dest;
static int is_pipe;
static int busy;
static struct event ack_clear_event;
static int strobe_state;

static void do_ack_clear(void *);
static void set_busy(int state);
static void open_stream(void);

void printer_init(void) {
	stream = NULL;
	stream_dest = NULL;
	is_pipe = 0;
	event_init(&ack_clear_event, do_ack_clear, NULL);
	strobe_state = 0;
}

void printer_reset(void) {
	strobe_state = PIA_VALUE_A(PIA1) & 0x02;
}

/* "Open" routines don't directly open the stream.  This way, a file or pipe
 * can be specified in the config file, but we won't send anything unless
 * something is printed. */

void printer_open_file(const char *filename) {
	printer_close();
	if (stream_dest) g_free(stream_dest);
	stream_dest = g_strdup(filename);
	is_pipe = 0;
	set_busy(0);
}

void printer_open_pipe(const char *command) {
	printer_close();
	if (stream_dest) g_free(stream_dest);
	stream_dest = g_strdup(command);
	is_pipe = 1;
	set_busy(0);
}

void printer_close(void) {
	/* flush stream, but destroy stream_dest so it won't be reopened */
	printer_flush();
	if (stream_dest) g_free(stream_dest);
	stream_dest = NULL;
	is_pipe = 0;
	set_busy(1);
}

/* close stream but leave stream_dest intact so it will be reopened */
void printer_flush(void) {
	if (!stream) return;
	if (is_pipe) {
		pclose(stream);
	} else {
		fclose(stream);
	}
	stream = NULL;
}

/* Called when the PIA bus containing STROBE is changed */
void printer_strobe(void) {
	int new_strobe, byte;
	new_strobe = PIA_VALUE_A(PIA1) & 0x02;
	/* Ignore if this is not a transition to high */
	if (new_strobe == strobe_state) return;
	strobe_state = new_strobe;
	if (!strobe_state) return;
	/* Open stream for output if it's not already */
	if (!stream_dest) return;
	if (!stream) open_stream();
	/* Print byte */
	byte = PIA_VALUE_B(PIA0);
	if (stream) {
		fputc(byte, stream);
	}
	/* ACK, and schedule !ACK */
	PIA_RESET_Cx1(PIA1.a);
	ack_clear_event.at_tick = event_current_tick + (OSCILLATOR_RATE / 150000);
	event_queue(&MACHINE_EVENT_LIST, &ack_clear_event);
}

static void open_stream(void) {
	if (!stream_dest) return;
	if (is_pipe) {
		stream = popen(stream_dest, "w");
	} else {
		stream = fopen(stream_dest, "ab");
	}
	if (stream) {
		set_busy(0);
	} else {
		printer_close();
	}
}

static void do_ack_clear(void *data) {
	(void)data;
	PIA_SET_Cx1(PIA1.a);
}

static void set_busy(int state) {
	busy = state;
	if (state) {
		PIA1.b.in_sink |= 0x01;
	} else {
		PIA1.b.in_sink &= ~0x01;
	}
}
