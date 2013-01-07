/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_WD279X_H_
#define XROAR_WD279X_H_

#include <inttypes.h>

#include "events.h"

enum WD279X_type {
	WD2791, WD2793, WD2795, WD2797
};

typedef struct WD279X WD279X;
struct WD279X {
	enum WD279X_type type;

	/* Registers */
	uint8_t status_register;
	uint8_t track_register;
	uint8_t sector_register;
	uint8_t data_register;
	uint8_t command_register;

	/* External handlers */
	void (*set_drq_handler)(void);
	void (*reset_drq_handler)(void);
	void (*set_intrq_handler)(void);
	void (*reset_intrq_handler)(void);

	/* WD279X internal state */
	int state;  /* from enum WD279X_state */
	struct event state_event;
	int direction;
	int side;
	int step_delay;
	_Bool double_density;

	_Bool is_step_cmd;
	uint16_t crc;
	int dam;
	int bytes_left;
	int index_holes_count;
	uint8_t track_register_tmp;

	/* Private */

	_Bool has_sso;
	_Bool has_length_flag;
	_Bool has_inverted_data;

};

WD279X *wd279x_new(enum WD279X_type type);
void wd279x_init(WD279X *fdc, enum WD279X_type type);
void wd279x_deinit(WD279X *fdc);
void wd279x_free(WD279X *fdc);
void wd279x_reset(WD279X *fdc);
void wd279x_set_dden(WD279X *fdc, _Bool dden);  /* 1 = Double density, 0 = Single */
uint8_t wd279x_read(WD279X *fdc, uint16_t A);
void wd279x_write(WD279X *fdc, uint16_t A, uint8_t D);

#endif  /* XROAR_WD279X_H_ */
