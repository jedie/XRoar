/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_MC6821_H_
#define XROAR_MC6821_H_

#include <inttypes.h>

/* Two "sides" per PIA (A & B), with slightly different characteristics.  A
 * side represented as output and input sink (the struct used is common to both
 * but for the A side the source values are ignored).  B side represented by
 * separate output and input source and sink.
 *
 * Pointers to preread and postwrite hooks can be set for data & control
 * registers.
 *
 * Not implemented: Cx2/IRQx2 behaviour.
 */

struct MC6821_side {
	/* Internal state */
	uint8_t control_register;
	uint8_t direction_register;
	uint8_t output_register;
	_Bool interrupt_received;
	_Bool irq;
	/* Calculated pin state */
	uint8_t out_source;  /* ignored for side A */
	uint8_t out_sink;
	/* External state */
	uint8_t in_source;  /* ignored for side A */
	uint8_t in_sink;
	/* Hooks */
	void (*control_preread)(void);
	void (*control_postwrite)(void);
	void (*data_preread)(void);
	void (*data_postwrite)(void);
};

struct MC6821 {
	struct MC6821_side a, b;
};

#define PIA_SET_Cx1(s) mc6821_set_cx1(&s)
#define PIA_RESET_Cx1(s) mc6821_reset_cx1(&s)

/* Convenience macros to calculate the effective value of a port output, for
 * example as seen by a high impedance input. */

#define PIA_VALUE_A(p) ((p).a.out_sink & (p).a.in_sink)
#define PIA_VALUE_B(p) (((p).b.out_source | (p).b.in_source) & (p).b.out_sink & (p).b.in_sink)

struct MC6821 *mc6821_new(void);
void mc6821_init(struct MC6821 *pia);
void mc6821_destroy(struct MC6821 *pia);

void mc6821_reset(struct MC6821 *pia);
void mc6821_set_cx1(struct MC6821_side *side);
void mc6821_reset_cx1(struct MC6821_side *side);
void mc6821_update_state(struct MC6821 *pia);
uint8_t mc6821_read(struct MC6821 *pia, uint16_t A);
void mc6821_write(struct MC6821 *pia, uint16_t A, uint8_t D);

#endif  /* XROAR_MC6821_H_ */
