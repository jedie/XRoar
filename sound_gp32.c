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

#include "config.h"

#ifdef HAVE_GP32

#include <string.h>
#include <gpdef.h>
#include <gpstdlib.h>
#include <gpgraphic.h>
#include <gpstdio.h>
#include <gpfont.h>

#include "sound.h"
#include "pia.h"
#include "xroar.h"
#include "types.h"
#include "gp32/gpsound.h"
#include "gp32/gp32.h"

static int init(void);
static void shutdown(void);
static void reset(void);
static void update(void);

SoundModule sound_gp32_module = {
	NULL,
	"gp32",
	"GP32 audio",
	init, shutdown, reset, update
};

typedef uint16_t Sample;  /* 8-bit stereo */
typedef uint_fast16_t Sample_f;

uint_fast32_t sample_rate;
Cycle sample_cycles;
uint_fast32_t frame_size;
uint_fast32_t frame_cycles;

static Cycle frame_cycle_base;
uint_fast8_t writing_frame;
static Sample **buffer;
static Sample *wrptr;
static Sample lastsample;

static int init(void) {
	sample_rate = 22050;
	gpsound_init(PCLK, &sample_rate);
	sample_cycles = OSCILLATOR_RATE / sample_rate;
	frame_size = CYCLES_PER_FRAME / sample_cycles;
	frame_cycles = sample_cycles * frame_size;
	buffer = gpsound_buffers(frame_size);
	gpsound_start();
	return 0;
}

static void shutdown(void) {
}

static void reset(void) {
	memset(buffer[0], 0x80, frame_size * sizeof(Sample));
	memset(buffer[1], 0x80, frame_size * sizeof(Sample));
	wrptr = buffer[1];
	writing_frame = 1;
	frame_cycle_base = current_cycle;
	next_sound_update = frame_cycle_base + frame_cycles;
	lastsample = 0x80;
}

static void update(void) {
	Cycle elapsed_cycles = current_cycle - frame_cycle_base;
	Sample_f fill_with;
	Sample *fill_to;
	if (elapsed_cycles >= frame_cycles) {
		fill_to = buffer[writing_frame] + frame_size;
	} else {
		fill_to = buffer[writing_frame] + (elapsed_cycles/(Cycle)sample_cycles);
	}
	if (!(PIA_1B.control_register & 0x08)) {
		/* Single-bit sound */
		//fill_with = (PIA_1B.port_output & 0x02) ? 0xffff : 0;
		//fill_with = lastsample;
		fill_with = ((PIA_1B.port_output & 0x02) << 5) ^ 0x80;
	} else  {
		if (PIA_0B.control_register & 0x08) {
			/* Sound disabled */
			//fill_with = 0x80;
			fill_with = lastsample & 0xff;
		} else {
			/* DAC output */
			fill_with = ((PIA_1A.port_output & 0xfc) >> 1) ^ 0x80;
		}
	}
	while (wrptr < fill_to)
		*(wrptr++) = lastsample;
	if ((fill_to - buffer[writing_frame]) >= frame_size) {
		frame_cycle_base += frame_cycles;
		next_sound_update = frame_cycle_base + frame_cycles;
		writing_frame ^= 1;
		wrptr = buffer[writing_frame];
		/* In theory, setting bit 2 of CLKCON sends the CPU into
		 * IDLE mode, which turns off the clock until an interrupt
		 * is raised. */
		while ((rDCSRC2 >= (unsigned)buffer[1]) == writing_frame);
			//rCLKCON |= (1<<2);
	}
	lastsample = (fill_with << 8) | fill_with;
	return;
}

void sound_silence(void) {
	memset(buffer[0], 0x80, frame_size * sizeof(Sample));
	memset(buffer[1], 0x80, frame_size * sizeof(Sample));
}

#endif  /* HAVE_GP32 */
