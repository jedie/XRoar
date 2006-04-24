/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2006  Ciaran Anscomb
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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/rtc.h>

#include "types.h"
#include "events.h"
#include "pia.h"
#include "sound.h"
#include "xroar.h"
#include "logging.h"

static int init(void);
static void shutdown(void);
static void reset(void);
static void update(void);

static void flush_frame(void);
static event_t *flush_event;

SoundModule sound_rtc_module = {
	"rtc",
	"Use real time clock for timing (no audio)",
	init, shutdown, reset, update
};

#define SAMPLE_RATE 64
#define FRAME_CYCLES ((int)(OSCILLATOR_RATE / SAMPLE_RATE))

static Cycle frame_cycle_base;
int fd;

static int init(void) {
	LOG_DEBUG(2,"Initialising RTC driver\n");
	if ((fd = open ("/dev/rtc", O_RDONLY)) == -1) {
		LOG_ERROR("Couldn't open /dev/rtc\n");
		return 1;
	}
	if (ioctl(fd, RTC_IRQP_SET, SAMPLE_RATE) == -1) {
		LOG_ERROR("Couldn't set %dHz clock\n", SAMPLE_RATE);
		close(fd);
		return 1;
	}
	if (ioctl(fd, RTC_PIE_ON, 0) == -1) {
		LOG_ERROR("Couldn't enable periodic interrupts\n");
		close(fd);
		return 1;
	}
	flush_event = event_new();
	flush_event->dispatch = flush_frame;
	return 0;
}

static void shutdown(void) {
	event_free(flush_event);
	if (ioctl(fd, RTC_PIE_OFF, 0) == -1) {
		LOG_WARN("Couldn't disable periodic interrupts\n");
	}
	close(fd);
}

static void reset(void) {
	frame_cycle_base = current_cycle;
	flush_event->at_cycle = frame_cycle_base + FRAME_CYCLES;
	event_queue(flush_event);
}

static void update(void) {
}

static void flush_frame(void) {
	unsigned long data;
	frame_cycle_base += FRAME_CYCLES;
	flush_event->at_cycle = frame_cycle_base + FRAME_CYCLES;
	event_queue(flush_event);
	read(fd, &data, sizeof(unsigned long));
}
