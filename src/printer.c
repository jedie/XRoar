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

#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pl_glib.h"

#include "breakpoint.h"
#include "events.h"
#include "logging.h"
#include "machine.h"
#include "mc6809.h"
#include "printer.h"
#include "xroar.h"

static FILE *stream;
static char *stream_dest;
static int is_pipe;
static struct event ack_clear_event;
static int strobe_state;
static _Bool busy;

printer_line_delegate printer_signal_ack = { NULL, NULL };

static void do_ack_clear(void *);
static void open_stream(void);

static void coco_print_byte(struct MC6809 *cpu);

static struct breakpoint coco_print_breakpoint[] = {
	BP_COCO_ROM(.address = 0xa2c1, .handler = (bp_handler)coco_print_byte),
};

void printer_init(void) {
	stream = NULL;
	stream_dest = NULL;
	is_pipe = 0;
	event_init(&ack_clear_event, do_ack_clear, NULL);
	strobe_state = 1;
	busy = 0;
}

void printer_reset(void) {
	strobe_state = 1;
}

/* "Open" routines don't directly open the stream.  This way, a file or pipe
 * can be specified in the config file, but we won't send anything unless
 * something is printed. */

void printer_open_file(const char *filename) {
	printer_close();
	if (stream_dest) g_free(stream_dest);
	stream_dest = g_strdup(filename);
	is_pipe = 0;
	busy = 0;
	bp_add_list(coco_print_breakpoint);
}

void printer_open_pipe(const char *command) {
	printer_close();
	if (stream_dest) g_free(stream_dest);
	stream_dest = g_strdup(command);
	is_pipe = 1;
	busy = 0;
	bp_add_list(coco_print_breakpoint);
}

void printer_close(void) {
	/* flush stream, but destroy stream_dest so it won't be reopened */
	printer_flush();
	if (stream_dest) g_free(stream_dest);
	stream_dest = NULL;
	is_pipe = 0;
	busy = 1;
	bp_remove_list(coco_print_breakpoint);
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
void printer_strobe(_Bool strobe, int data) {
	/* Ignore if this is not a transition to high */
	if (strobe == strobe_state) return;
	strobe_state = strobe;
	if (!strobe_state) return;
	/* Open stream for output if it's not already */
	if (!stream_dest) return;
	if (!stream) open_stream();
	/* Print byte */
	if (stream) {
		fputc(data, stream);
	}
	/* ACK, and schedule !ACK */
	if (printer_signal_ack.delegate)
		printer_signal_ack.delegate(printer_signal_ack.dptr, 1);
	ack_clear_event.at_tick = event_current_tick + (OSCILLATOR_RATE / 150000);
	event_queue(&MACHINE_EVENT_LIST, &ack_clear_event);
}

static void coco_print_byte(struct MC6809 *cpu) {
	int byte;
	/* Open stream for output if it's not already */
	if (!stream_dest) return;
	if (!stream) open_stream();
	/* Print byte */
	byte = MC6809_REG_A(cpu);
	if (stream) {
		fputc(byte, stream);
	}
}

static void open_stream(void) {
	if (!stream_dest) return;
	if (is_pipe) {
		stream = popen(stream_dest, "w");
	} else {
		stream = fopen(stream_dest, "ab");
	}
	if (stream) {
		busy = 0;
	} else {
		printer_close();
	}
}

static void do_ack_clear(void *data) {
	(void)data;
	if (printer_signal_ack.delegate)
		printer_signal_ack.delegate(printer_signal_ack.dptr, 0);
}

_Bool printer_busy(void) {
	return busy;
}
