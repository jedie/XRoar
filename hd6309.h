/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_HD6309_H_
#define XROAR_HD6309_H_

#include <inttypes.h>

#include "mc6809.h"

#define HD6309_INT_VEC_ILLEGAL (0xfff0)

struct HD6309 {
	struct MC6809 mc6809;
	/* Extra registers */
	uint16_t reg_w;
	uint8_t reg_md;
	uint16_t reg_v;
	/* Extra state (derived from reg_md) */
	_Bool native_mode;
	_Bool firq_stack_all;
	/* TFM state */
	uint16_t *tfm_src;
	uint16_t *tfm_dest;
	uint8_t tfm_data;
	int tfm_src_mod;
	int tfm_dest_mod;
};

#define HD6309_REG_E(hcpu) (*((uint8_t *)&hcpu->reg_w + MC6809_REG_HI))
#define HD6309_REG_F(hcpu) (*((uint8_t *)&hcpu->reg_w + MC6809_REG_LO))

/* new still returns a struct MC6809 */
struct MC6809 *hd6309_new(void);
/* but init() requires a struct HD6309 */
void hd6309_init(struct HD6309 *hcpu);

#endif  /* XROAR_HD6309_H_ */
