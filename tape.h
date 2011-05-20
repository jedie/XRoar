/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2011  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_TAPE_H_
#define XROAR_TAPE_H_

#include "types.h"
#include "machine.h"

/* These are the usual cycle lengths for each bit as written by the Dragon
 * BASIC ROM. */
#define TAPE_BIT0_LENGTH (13008)
#define TAPE_BIT1_LENGTH (6960)
#define TAPE_AV_BIT_LENGTH ((TAPE_BIT0_LENGTH + TAPE_BIT1_LENGTH) / 2)

struct tape_module;

struct tape {
	struct tape_module *module;
	void *data;  /* module-specific data */
	int playing;  /* 0 means tape stopped, must be manually restarted */
	long offset;  /* current tape position */
	long size;  /* current tape size */
	int fake_leader;  /* number of fake leader bytes */
	int fake_sync;  /* flag that fake leader should end with $3c (sync) */
	int fake_byte;
	int fake_bit;
	int fake_bit_index;  /* 0-7 */
	int fake_pulse_index;
	Cycle last_write_cycle;
};

struct tape_module {
	void (*close)(struct tape *t);
	long (*size)(struct tape *t);
	long (*tell)(struct tape *t);
	int (*seek)(struct tape *t, long offset, int whence);
	int (*to_ms)(struct tape *t, long pos);
	long (*ms_to)(struct tape *t, int ms);
	int (*pulse_in)(struct tape *t, int *pulse_width);
	int (*sample_out)(struct tape *t, uint8_t sample, int length);
	void (*motor_off)(struct tape *t);
};

extern struct tape *tape_input;
extern struct tape *tape_output;

#define tape_close(t) (t)->module->close(t)
/* for audio file input, tape_tell() might return an absolute file position,
   or sample number whichever is appropriate.  for cas file, it'll probably be
   file position * 8 + bit position */
#define tape_tell(t) (t)->module->tell(t)
#define tape_seek(t,...) (t)->module->seek((t), __VA_ARGS__)
#define tape_to_ms(t,...) (t)->module->to_ms((t), __VA_ARGS__)
#define tape_ms_to(t,...) (t)->module->ms_to((t), __VA_ARGS__)
#define tape_rewind(t) tape_seek(t, 0, SEEK_SET)
int tape_pulse_in(struct tape *t, int *pulse_width);
int tape_bit_in(struct tape *t);
int tape_byte_in(struct tape *t);
#define tape_sample_out(t,...) (t)->module->sample_out((t), __VA_ARGS__)
void tape_bit_out(struct tape *t, int bit);
void tape_byte_out(struct tape *t, int byte);
void tape_play(struct tape *t);
void tape_stop(struct tape *t);

struct tape_file {
	long offset;
	char name[9];
	int type;
	int ascii_flag;
	int gap_flag;
	int start_address;
	int load_address;
	int checksum_error;
};

/* find next tape file */
struct tape_file *tape_file_next(struct tape *t, int skip_bad);
/* seek to a tape file */
void tape_seek_to_file(struct tape *t, struct tape_file *f);

/* Module-specific open() calls */
struct tape *tape_cas_open(const char *filename, int mode);
struct tape *tape_asc_open(const char *filename, int mode);
struct tape *tape_sndfile_open(const char *filename, int mode);

/* Only to be used by tape modules */
struct tape *tape_new(void);
void tape_free(struct tape *t);

/**************************************************************************/

extern int tape_audio;

void tape_init(void);
void tape_reset(void);
void tape_shutdown(void);

int tape_open_reading(const char *filename);
void tape_close_reading(void);
int tape_open_writing(const char *filename);
void tape_close_writing(void);

int tape_autorun(const char *filename);

void tape_update_motor(void);
void tape_update_output(void);

#define TAPE_FAST (1 << 0)
#define TAPE_PAD (1 << 1)
#define TAPE_REWRITE (1 << 2)

void tape_set_state(int flags);
void tape_select_state(int flags);  /* set & update UI */
int tape_get_state(void);

#endif  /* XROAR_TAPE_H_ */
