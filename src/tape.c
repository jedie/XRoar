/*  Copyright 2003-2014 Ciaran Anscomb
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

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "xalloc.h"

#include "breakpoint.h"
#include "delegate.h"
#include "events.h"
#include "fs.h"
#include "keyboard.h"
#include "logging.h"
#include "machine.h"
#include "mc6809.h"
#include "module.h"
#include "sound.h"
#include "tape.h"
#include "xroar.h"

/* Tape output delegate.  Called when the output from the tape unit (i.e., the
 * input to the machine) changes. */
delegate_float tape_update_audio = { delegate_default_float, NULL };

struct tape *tape_input = NULL;
struct tape *tape_output = NULL;

static void waggle_bit(void *);
static struct event waggle_event;
static void flush_output(void *);
static struct event flush_event;

static int tape_fast = 0;
static int tape_pad = 0;
static int tape_pad_auto = 0;
static int tape_rewrite = 0;
static int in_pulse = -1;
static int in_pulse_width = 0;

static uint8_t last_tape_output = 0;
static _Bool motor = 0;

static int input_skip_sync = 0;
static int rewrite_have_sync = 0;
static int rewrite_leader_count = 256;
static int rewrite_bit_count = 0;
static void tape_desync(int leader);
static void rewrite_sync(struct MC6809 *cpu);
static void rewrite_bitin(struct MC6809 *cpu);
static void rewrite_tape_on(struct MC6809 *cpu);
static void rewrite_end_of_block(struct MC6809 *cpu);

static void set_breakpoints(void);

/**************************************************************************/

int tape_seek(struct tape *t, long offset, int whence) {
	int r = t->module->seek(t, offset, whence);
	tape_update_motor(motor);
	/* if seeking to beginning of tape, ensure any fake leader etc.
	 * is set up properly */
	if (r >= 0 && t->offset == 0) {
		tape_desync(256);
	}
	return r;
}

static int tape_pulse_in(struct tape *t, int *pulse_width) {
	if (!t) return -1;
	return t->module->pulse_in(t, pulse_width);
}

static int tape_bit_in(struct tape *t) {
	if (!t) return -1;
	int phase, pulse1_width, cycle_width;
	if (tape_pulse_in(t, &pulse1_width) == -1)
		return -1;
	do {
		int pulse0_width = pulse1_width;
		if ((phase = tape_pulse_in(t, &pulse1_width)) == -1)
			return -1;
		cycle_width = pulse0_width + pulse1_width;
	} while (!phase || (cycle_width < (TAPE_BIT1_LENGTH / 2))
	         || (cycle_width > (TAPE_BIT0_LENGTH * 2)));
	if (cycle_width < TAPE_AV_BIT_LENGTH) {
		return 1;
	}
	return 0;
}

static int tape_byte_in(struct tape *t) {
	int byte = 0, i;
	for (i = 8; i; i--) {
		int bit = tape_bit_in(t);
		if (bit == -1) return -1;
		byte = (byte >> 1) | (bit ? 0x80 : 0);
	}
	return byte;
}

static void tape_bit_out(struct tape *t, int bit) {
	if (!t) return;
	if (bit) {
		tape_sample_out(t, 0xf8, TAPE_BIT1_LENGTH / 2);
		tape_sample_out(t, 0x00, TAPE_BIT1_LENGTH / 2);
	} else {
		tape_sample_out(t, 0xf8, TAPE_BIT0_LENGTH / 2);
		tape_sample_out(t, 0x00, TAPE_BIT0_LENGTH / 2);
	}
	rewrite_bit_count = (rewrite_bit_count + 1) & 7;
	last_tape_output = 0;
}

static void tape_byte_out(struct tape *t, int byte) {
	int i;
	if (!t) return;
	for (i = 8; i; i--) {
		tape_bit_out(t, byte & 1);
		byte >>= 1;
	}
}

/**************************************************************************/

static int block_sync(struct tape *tape) {
	int byte = 0;
	for (;;) {
		int bit = tape_bit_in(tape);
		if (bit == -1) return -1;
		byte = (byte >> 1) | (bit ? 0x80 : 0);
		if (byte == 0x3c) {
			return 0;
		}
	}
}

/* read next block.  returns -1 on EOF/error, block type on success. */
/* *sum will be computed sum - checksum byte, which should be 0 */
static int block_in(struct tape *t, uint8_t *sum, long *offset, uint8_t *block) {
	int type, size, sumbyte, i;

	if (block_sync(t) == -1) return -1;
	if (offset) {
		*offset = tape_tell(t);
	}
	if ((type = tape_byte_in(t)) == -1) return -1;
	if (block) block[0] = type;
	if ((size = tape_byte_in(t)) == -1) return -1;
	if (block) block[1] = size;
	if (sum) *sum = type + size;
	for (i = 0; i < size; i++) {
		int data;
		if ((data = tape_byte_in(t)) == -1) return -1;
		if (block) block[2+i] = data;
		if (sum) *sum += data;
	}
	if ((sumbyte = tape_byte_in(t)) == -1) return -1;
	if (block) block[2+size] = sumbyte;
	if (sum) *sum -= sumbyte;
	return type;
}

struct tape_file *tape_file_next(struct tape *t, int skip_bad) {
	struct tape_file *f;
	uint8_t block[258];
	uint8_t sum;
	long offset;

	for (;;) {
		long start = tape_tell(t);
		int type = block_in(t, &sum, &offset, block);
		if (type == -1)
			return NULL;
		/* If skip_bad set, this aggressively scans for valid header
		   blocks by seeking back to just after the last sync byte: */
		if (skip_bad && (type != 0 || sum != 0 || block[1] < 15)) {
			tape_seek(t, offset, SEEK_SET);
			continue;
		}
		if (type != 0 || block[1] < 15)
			continue;
		f = xmalloc(sizeof(*f));
		f->offset = start;
		memcpy(f->name, &block[2], 8);
		int i = 8;
		do {
			f->name[i--] = 0;
		} while (i >= 0 && f->name[i] == ' ');
		f->type = block[10];
		f->ascii_flag = block[11] ? 1 : 0;
		f->gap_flag = block[12] ? 1 : 0;
		f->start_address = (block[13] << 8) | block[14];
		f->load_address = (block[15] << 8) | block[16];
		f->checksum_error = sum ? 1 : 0;
		return f;
	}
}

void tape_seek_to_file(struct tape *t, struct tape_file *f) {
	if (!t || !f) return;
	tape_seek(t, f->offset, SEEK_SET);
}

/**************************************************************************/

struct tape *tape_new(void) {
	struct tape *new = xzalloc(sizeof(*new));
	return new;
}

void tape_free(struct tape *t) {
	free(t);
}

/**************************************************************************/

void tape_init(void) {
	DELEGATE_CALL1(tape_update_audio, 0.5);
	event_init(&waggle_event, (delegate_null){waggle_bit, NULL});
	event_init(&flush_event, (delegate_null){flush_output, NULL});
}

void tape_reset(void) {
	tape_close_writing();
	motor = 0;
	event_dequeue(&waggle_event);
}

void tape_shutdown(void) {
	tape_reset();
}

int tape_open_reading(const char *filename) {
	tape_close_reading();
	input_skip_sync = 0;
	int type = xroar_filetype_by_ext(filename);
	switch (type) {
	case FILETYPE_CAS:
		if ((tape_input = tape_cas_open(filename, "rb")) == NULL) {
			LOG_WARN("Failed to open '%s'\n", filename);
			return -1;
		}
		if (tape_pad_auto) {
			int flags = tape_get_state() & ~TAPE_PAD;
			if (IS_DRAGON && tape_input->leader_count < 114)
				flags |= TAPE_PAD;
			if (IS_COCO && tape_input->leader_count < 130)
				flags |= TAPE_PAD;
			tape_select_state(flags);
		}
		break;
	case FILETYPE_ASC:
		if ((tape_input = tape_asc_open(filename, "rb")) == NULL) {
			LOG_WARN("Failed to open '%s'\n", filename);
			return -1;
		}
		break;
	default:
#ifdef HAVE_SNDFILE
		if ((tape_input = tape_sndfile_open(filename, "rb")) == NULL) {
			LOG_WARN("Failed to open '%s'\n", filename);
			return -1;
		}
		if (tape_pad_auto) {
			int flags = tape_get_state() & ~TAPE_PAD;
			tape_select_state(flags);
		}
		input_skip_sync = 1;
		break;
#else
		LOG_WARN("Failed to open '%s'\n", filename);
		return -1;
#endif
	}

	tape_desync(256);
	tape_update_motor(motor);
	LOG_DEBUG(1, "Tape: Attached '%s' for reading\n", filename);
	return 0;
}

void tape_close_reading(void) {
	if (tape_input)
		tape_close(tape_input);
	tape_input = NULL;
}

int tape_open_writing(const char *filename) {
	tape_close_writing();
	int type = xroar_filetype_by_ext(filename);
	switch (type) {
	case FILETYPE_CAS:
	case FILETYPE_ASC:
		if ((tape_output = tape_cas_open(filename, "wb")) == NULL) {
			LOG_WARN("Failed to open '%s' for writing.", filename);
			return -1;
		}
		break;
	default:
#ifdef HAVE_SNDFILE
		if ((tape_output = tape_sndfile_open(filename, "wb")) == NULL) {
			LOG_WARN("Failed to open '%s' for writing.", filename);
			return -1;
		}
#else
		LOG_WARN("Failed to open '%s' for writing.\n", filename);
		return -1;
#endif
		break;
	}

	tape_update_motor(motor);
	rewrite_bit_count = 0;
	LOG_DEBUG(1, "Tape: Attached '%s' for writing.\n", filename);
	return 0;
}

void tape_close_writing(void) {
	if (tape_rewrite) {
		tape_byte_out(tape_output, 0x55);
		tape_byte_out(tape_output, 0x55);
	}
	if (tape_output) {
		event_dequeue(&flush_event);
		tape_update_output(last_tape_output);
		tape_close(tape_output);
	}
	tape_output = NULL;
}

/* Close any currently-open tape file, open a new one and read the first
 * bufferful of data.  Tries to guess the filetype.  Returns -1 on error,
 * 0 for a BASIC program, 1 for data and 2 for M/C. */
int tape_autorun(const char *filename) {
	if (filename == NULL)
		return -1;
	keyboard_queue_basic(NULL);
	if (tape_open_reading(filename) == -1)
		return -1;
	struct tape_file *f = tape_file_next(tape_input, 0);
	tape_rewind(tape_input);
	if (!f) {
		return -1;
	}
	int type = f->type;
	free(f);
	switch (type) {
		case 0:
			keyboard_queue_basic("\003CLOAD\rRUN\r");
			break;
		case 2:
			keyboard_queue_basic("\003CLOADM:EXEC\r");
			break;
		default:
			break;
	}
	return type;
}

/* Called whenever the motor control line is written to. */
void tape_update_motor(_Bool state) {
	if (state) {
		if (tape_input && !waggle_event.queued) {
			/* If motor turned on and tape file attached,
			 * enable the tape input bit waggler */
			waggle_event.at_tick = event_current_tick;
			waggle_bit(NULL);
		}
		if (tape_output && !flush_event.queued) {
			flush_event.at_tick = event_current_tick + OSCILLATOR_RATE / 2;
			event_queue(&MACHINE_EVENT_LIST, &flush_event);
			tape_output->last_write_cycle = event_current_tick;
		}
	} else {
		event_dequeue(&waggle_event);
		event_dequeue(&flush_event);
		tape_update_output(last_tape_output);
		if (tape_output && tape_output->module->motor_off) {
			tape_output->module->motor_off(tape_output);
		}
		if (tape_pad || tape_rewrite) {
			tape_desync(256);
		}
	}
	if (motor != state) {
		LOG_DEBUG(2, "Tape: motor %s\n", state ? "ON" : "OFF");
	}
	motor = state;
	set_breakpoints();
}

/* Called whenever the DAC is written to. */
void tape_update_output(uint8_t value) {
	if (motor && tape_output && !tape_rewrite) {
		int length = event_current_tick - tape_output->last_write_cycle;
		tape_output->module->sample_out(tape_output, last_tape_output, length);
		tape_output->last_write_cycle = event_current_tick;
	}
	last_tape_output = value;
}

/* Read pulse & duration, schedule next read */
static void waggle_bit(void *data) {
	(void)data;
	in_pulse = tape_pulse_in(tape_input, &in_pulse_width);
	switch (in_pulse) {
	default:
	case -1:
		DELEGATE_CALL1(tape_update_audio, 0.5);
		event_dequeue(&waggle_event);
		return;
	case 0:
		DELEGATE_CALL1(tape_update_audio, 0.0);
		break;
	case 1:
		DELEGATE_CALL1(tape_update_audio, 1.0);
		break;
	}
	waggle_event.at_tick += in_pulse_width;
	event_queue(&MACHINE_EVENT_LIST, &waggle_event);
}

/* ensure any "pulse" over 1/2 second long is flushed to output, so it doesn't
 * overflow any counters */
static void flush_output(void *data) {
	(void)data;
	tape_update_output(last_tape_output);
	if (motor) {
		flush_event.at_tick += OSCILLATOR_RATE / 2;
		event_queue(&MACHINE_EVENT_LIST, &flush_event);
	}
}

/* Fast tape */

static int pskip = 0;

static int pulse_skip(void) {
	pskip <<= 4;
	for (;;) {
		if (pskip < in_pulse_width) {
			in_pulse_width -= pskip;
			pskip = 0;
			waggle_event.at_tick = event_current_tick + in_pulse_width;
			event_queue(&MACHINE_EVENT_LIST, &waggle_event);
			DELEGATE_CALL1(tape_update_audio, in_pulse ? 1.0 : 0.0);
			return in_pulse;
		}
		pskip -= in_pulse_width;
		in_pulse = tape_pulse_in(tape_input, &in_pulse_width);
		if (in_pulse < 0) {
			event_dequeue(&waggle_event);
			pskip = 0;
			return -1;
		}
	}
}

static uint8_t op_add(struct MC6809 *cpu, uint8_t v1, uint8_t v2) {
	unsigned int v = v1 + v2;
	cpu->reg_cc &= ~0x2f;  /* clear HNZVC */
	if (v & 0x80) cpu->reg_cc |= 0x08;  /* set N */
	if ((v & 0xff) == 0) cpu->reg_cc |= 0x04;  /* set Z */
	if ((v1^v2^v^(v>>1)) & 0x80) cpu->reg_cc |= 0x02;  /* set V */
	if (v & 0x100) cpu->reg_cc |= 0x01;  /* set C */
	if ((v1^v2^v) & 0x10) cpu->reg_cc |= 0x20;  /* set H */
	return v;
}

static uint8_t op_sub(struct MC6809 *cpu, uint8_t v1, uint8_t v2) {
	unsigned int v = v1 - v2;
	cpu->reg_cc &= ~0x0f;  /* clear NZVC */
	if (v & 0x80) cpu->reg_cc |= 0x08;  /* set N */
	if ((v & 0xff) == 0) cpu->reg_cc |= 0x04;  /* set Z */
	if ((v1^v2^v^(v>>1)) & 0x80) cpu->reg_cc |= 0x02;  /* set V */
	if (v & 0x100) cpu->reg_cc |= 0x01;  /* set C */
	return v;
}

#define BSR(f) do { pskip += 7; f(cpu); } while (0)
#define CLR(a) do { pskip += 6; machine_write_byte((a), 0); } while (0)
#define DEC(a) do { pskip += 6; machine_write_byte((a), machine_read_byte(a) - 1); } while (0)
#define INC(a) do { pskip += 6; machine_write_byte((a), machine_read_byte(a) + 1); } while (0)

static void motor_on(struct MC6809 *cpu) {
	int delay = IS_DRAGON ? 0x95 : 0x8a;
	pskip += 5;  /* LDX <$95 */
	int i = (machine_read_byte(delay) << 8) | machine_read_byte(delay+1);
	if (IS_DRAGON)
		pskip += 5;  /* LBRA delay_X */
	for (; i; i--) {
		pskip += 5;  /* LEAX -1,X */
		pskip += 3;  /* BNE delay_X */
		/* periodically sync up tape position */
		if ((i & 63) == 0)
			pulse_skip();
	}
	cpu->reg_x = 0;
	cpu->reg_cc |= 0x04;
	pskip += 5;  /* RTS */
}

static uint8_t op_clr(struct MC6809 *cpu) {
	cpu->reg_cc &= ~0x0b;  /* clear NVC */
	cpu->reg_cc |= 0x04;  /* set Z */
	return 0;
}

static void sample_cas(struct MC6809 *cpu) {
	int pwcount = IS_DRAGON ? 0x82 : 0x83;
	INC(pwcount);
	pskip += 5;  /* LDB >$FF20 (should this be split 4,1?) */
	pulse_skip();
	pskip += 2;  /* RORB */
	if (in_pulse) {
		cpu->reg_cc &= ~1;
	} else {
		cpu->reg_cc |= 1;
	}
	pskip += 5;  /* RTS */
}

static void tape_wait_p0(struct MC6809 *cpu) {
	do {
		BSR(sample_cas);
		if (in_pulse < 0) return;
		pskip += 3;  /* BCS tape_wait_p0 */
	} while (cpu->reg_cc & 0x01);
	pskip += 5;  /* RTS */
}

static void tape_wait_p1(struct MC6809 *cpu) {
	do {
		BSR(sample_cas);
		if (in_pulse < 0) return;
		pskip += 3;  /* BCC tape_wait_p1 */
	} while (!(cpu->reg_cc & 0x01));
	pskip += 5;  /* RTS */
}

static void tape_wait_p0_p1(struct MC6809 *cpu) {
	BSR(tape_wait_p0);
	if (in_pulse < 0) return;
	tape_wait_p1(cpu);
}

static void tape_wait_p1_p0(struct MC6809 *cpu) {
	BSR(tape_wait_p1);
	if (in_pulse < 0) return;
	tape_wait_p0(cpu);
}

static void L_BDC3(struct MC6809 *cpu) {
	int pwcount = IS_DRAGON ? 0x82 : 0x83;
	int bcount = IS_DRAGON ? 0x83 : 0x82;
	int minpw1200 = IS_DRAGON ? 0x93 : 0x91;
	int maxpw1200 = IS_DRAGON ? 0x94 : 0x90;
	pskip += 4;  /* LDB <$82 */
	pskip += 4;  /* CMPB <$94 */
	op_sub(cpu, machine_read_byte(pwcount), machine_read_byte(maxpw1200));
	pskip += 3;  /* BHI L_BDCC */
	if (!(cpu->reg_cc & 0x05)) {
		CLR(bcount);
		op_clr(cpu);
		pskip += 5;  /* RTS */
		return;
	}
	pskip += 4;  /* CMPB <$93 */
	op_sub(cpu, machine_read_byte(pwcount), machine_read_byte(minpw1200));
	pskip += 5;  /* RTS */
}

static void tape_cmp_p1_1200(struct MC6809 *cpu) {
	int pwcount = IS_DRAGON ? 0x82 : 0x83;
	CLR(pwcount);
	BSR(tape_wait_p0);
	if (in_pulse < 0) return;
	pskip += 3;  /* BRA L_BDC3 */
	L_BDC3(cpu);
}

static void tape_cmp_p0_1200(struct MC6809 *cpu) {
	int pwcount = IS_DRAGON ? 0x82 : 0x83;
	CLR(pwcount);
	BSR(tape_wait_p1);
	if (in_pulse < 0) return;
	L_BDC3(cpu);
}

static void sync_leader(struct MC6809 *cpu) {
	int bcount = IS_DRAGON ? 0x83 : 0x82;
	int store;
L_BDED:
	BSR(tape_wait_p0_p1);
	if (in_pulse < 0) return;
L_BDEF:
	BSR(tape_cmp_p1_1200);
	if (in_pulse < 0) return;
	pskip += 3;  /* BHI L_BDFF */
	if (!(cpu->reg_cc & 0x05))
		goto L_BDFF;
L_BDF3:
	BSR(tape_cmp_p0_1200);
	if (in_pulse < 0) return;
	pskip += 3;  /* BCS L_BE03 */
	if (cpu->reg_cc & 0x01)
		goto L_BE03;
	INC(bcount);
	pskip += 4;  /* LDA <$83 */
	pskip += 2;  /* CMPA #$60 */
	store = machine_read_byte(bcount);
	op_sub(cpu, store, 0x60);
	pskip += 3;  /* BRA L_BE0D */
	goto L_BE0D;
L_BDFF:
	BSR(tape_cmp_p0_1200);
	if (in_pulse < 0) return;
	pskip += 3;  /* BHI L_BDEF */
	if (!(cpu->reg_cc & 0x05))
		goto L_BDEF;
L_BE03:
	BSR(tape_cmp_p1_1200);
	if (in_pulse < 0) return;
	pskip += 3;  /* BCS L_BDF3 */
	if (cpu->reg_cc & 0x01)
		goto L_BDF3;
	DEC(bcount);
	pskip += 4;  /* LDA <$83 */
	pskip += 2;  /* ADDA #$60 */
	store = op_add(cpu, machine_read_byte(bcount), 0x60);
L_BE0D:
	pskip += 3;  /* BNE L_BDED */
	if (!(cpu->reg_cc & 0x04))
		goto L_BDED;
	pskip += 4;  /* STA <$84 */
	machine_write_byte(0x84, store);
	pskip += 5;  /* RTS */
}

static void tape_wait_2p(struct MC6809 *cpu) {
	int pwcount = IS_DRAGON ? 0x82 : 0x83;
	CLR(pwcount);
	pskip += 6;  /* TST <$84 */
	pskip += 3;  /* BNE tape_wait_p1_p0 */
	if (machine_read_byte(0x84)) {
		tape_wait_p1_p0(cpu);
	} else {
		tape_wait_p0_p1(cpu);
	}
}

static void bitin(struct MC6809 *cpu) {
	int pwcount = IS_DRAGON ? 0x82 : 0x83;
	int mincw1200 = IS_DRAGON ? 0x92 : 0x8f;
	BSR(tape_wait_2p);
	pskip += 4;  /* LDB <$82 */
	pskip += 2;  /* DECB */
	pskip += 4;  /* CMPB <$92 */
	op_sub(cpu, machine_read_byte(pwcount) - 1, machine_read_byte(mincw1200));
	pskip += 5;  /* RTS */
}

static void cbin(struct MC6809 *cpu) {
	int bcount = IS_DRAGON ? 0x83 : 0x82;
	int bin = 0, i;
	pskip += 2;  /* LDA #$08 */
	pskip += 4;  /* STA <$83 */
	for (i = 0; i < 8; i++) {
		BSR(bitin);
		pskip += 2;  /* RORA */
		bin >>= 1;
		bin |= (cpu->reg_cc & 0x01) ? 0x80 : 0;
		pskip += 6;  /* DEC <$83 */
		pskip += 3;  /* BNE $BDB1 */
	}
	pskip += 5;  /* RTS */
	MC6809_REG_A(cpu) = bin;
	machine_write_byte(bcount, 0);
}

static void fast_motor_on(struct MC6809 *cpu) {
	if (!tape_pad) {
		motor_on(cpu);
	}
	machine_op_rts(cpu);
	pulse_skip();
}

static void fast_sync_leader(struct MC6809 *cpu) {
	if (tape_pad) {
		machine_write_byte(0x84, 0);
	} else {
		sync_leader(cpu);
	}
	machine_op_rts(cpu);
	pulse_skip();
}

static void fast_bitin(struct MC6809 *cpu) {
	bitin(cpu);
	machine_op_rts(cpu);
	pulse_skip();
	if (tape_rewrite) rewrite_bitin(cpu);
}

static void fast_cbin(struct MC6809 *cpu) {
	cbin(cpu);
	machine_op_rts(cpu);
	pulse_skip();
}

/* Leader padding & tape rewriting */

static void tape_desync(int leader) {
	if (tape_rewrite) {
		/* complete last byte */
		while (rewrite_bit_count)
			tape_bit_out(tape_output, 0);
		/* desync writing - pick up at next sync byte */
		rewrite_have_sync = 0;
		rewrite_leader_count = leader;
	}
}

static void rewrite_sync(struct MC6809 *cpu) {
	/* BLKIN, having read sync byte $3C */
	(void)cpu;
	if (rewrite_have_sync) return;
	if (tape_rewrite) {
		for (int i = 0; i < rewrite_leader_count; i++)
			tape_byte_out(tape_output, 0x55);
		tape_byte_out(tape_output, 0x3c);
		rewrite_have_sync = 1;
	}
}

static void rewrite_bitin(struct MC6809 *cpu) {
	/* RTS from BITIN */
	(void)cpu;
	if (tape_rewrite && rewrite_have_sync) {
		tape_bit_out(tape_output, cpu->reg_cc & 0x01);
	}
}

static void rewrite_tape_on(struct MC6809 *cpu) {
	/* CSRDON */
	(void)cpu;
	/* desync with long leader */
	tape_desync(256);
	/* for audio files, when padding leaders, assume a phase */
	if (tape_pad && input_skip_sync) {
		machine_write_byte(0x84, 0);  /* phase */
		machine_op_rts(cpu);
	}
}

static void rewrite_end_of_block(struct MC6809 *cpu) {
	/* BLKIN, having confirmed checksum */
	(void)cpu;
	/* desync with short inter-block leader */
	tape_desync(2);
}

/* Configuring tape options */

static struct breakpoint bp_list_fast[] = {
	BP_DRAGON_ROM(.address = 0xbdd7, .handler = (bp_handler)fast_motor_on),
	BP_COCO_ROM(.address = 0xa7d1, .handler = (bp_handler)fast_motor_on),
	BP_DRAGON_ROM(.address = 0xbded, .handler = (bp_handler)fast_sync_leader),
	BP_COCO_ROM(.address = 0xa782, .handler = (bp_handler)fast_sync_leader),
	BP_DRAGON_ROM(.address = 0xbda5, .handler = (bp_handler)fast_bitin),
	BP_COCO_ROM(.address = 0xa755, .handler = (bp_handler)fast_bitin),
};

static struct breakpoint bp_list_fast_cbin[] = {
	BP_DRAGON_ROM(.address = 0xbdad, .handler = (bp_handler)fast_cbin),
	BP_COCO_ROM(.address = 0xa749, .handler = (bp_handler)fast_cbin),
};

static struct breakpoint bp_list_rewrite[] = {
	BP_DRAGON_ROM(.address = 0xb94d, .handler = (bp_handler)rewrite_sync),
	BP_COCO_ROM(.address = 0xa719, .handler = (bp_handler)rewrite_sync),
	BP_DRAGON_ROM(.address = 0xbdac, .handler = (bp_handler)rewrite_bitin),
	BP_COCO_ROM(.address = 0xa75c, .handler = (bp_handler)rewrite_bitin),
	BP_DRAGON_ROM(.address = 0xbdeb, .handler = (bp_handler)rewrite_tape_on),
	BP_COCO_ROM(.address = 0xa780, .handler = (bp_handler)rewrite_tape_on),
	BP_DRAGON_ROM(.address = 0xb97e, .handler = (bp_handler)rewrite_end_of_block),
	BP_COCO_ROM(.address = 0xa746, .handler = (bp_handler)rewrite_end_of_block),
};

static void set_breakpoints(void) {
	/* clear any old breakpoints */
	bp_remove_list(bp_list_fast);
	bp_remove_list(bp_list_fast_cbin);
	bp_remove_list(bp_list_rewrite);
	if (!motor)
		return;
	/* add required breakpoints */
	if (tape_fast) {
		bp_add_list(bp_list_fast);
		/* these are incompatible with the other flags */
		if (!tape_pad && !tape_rewrite) {
			bp_add_list(bp_list_fast_cbin);
		}
	}
	if (tape_pad || tape_rewrite) {
		bp_add_list(bp_list_rewrite);
	}
}

void tape_set_state(int flags) {
	/* set flags */
	tape_fast = flags & TAPE_FAST;
	tape_pad = flags & TAPE_PAD;
	tape_pad_auto = flags & TAPE_PAD_AUTO;
	tape_rewrite = flags & TAPE_REWRITE;
	set_breakpoints();
}

/* sets state and updates UI */
void tape_select_state(int flags) {
	tape_set_state(flags);
	if (ui_module && ui_module->update_tape_state) {
		ui_module->update_tape_state(flags);
	}
}

int tape_get_state(void) {
	int flags = 0;
	if (tape_fast) flags |= TAPE_FAST;
	if (tape_pad) flags |= TAPE_PAD;
	if (tape_pad_auto) flags |= TAPE_PAD_AUTO;
	if (tape_rewrite) flags |= TAPE_REWRITE;
	return flags;
}
