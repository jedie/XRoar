/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_MC6809_H_
#define XROAR_MC6809_H_

#include "config.h"

#include <inttypes.h>

#define MC6809_INT_VEC_RESET (0xfffe)
#define MC6809_INT_VEC_NMI (0xfffc)
#define MC6809_INT_VEC_SWI (0xfffa)
#define MC6809_INT_VEC_IRQ (0xfff8)
#define MC6809_INT_VEC_FIRQ (0xfff6)
#define MC6809_INT_VEC_SWI2 (0xfff4)
#define MC6809_INT_VEC_SWI3 (0xfff2)

#define MC6809_COMPAT_STATE_NORMAL (0)
#define MC6809_COMPAT_STATE_SYNC (1)
#define MC6809_COMPAT_STATE_CWAI (2)
#define MC6809_COMPAT_STATE_DONE_INSTRUCTION (11)
#define MC6809_COMPAT_STATE_HCF (12)

/* Interface shared with all 6809-compatible CPUs */
struct MC6809 {
	/* Interrupt lines */
	_Bool halt, nmi, firq, irq;

	/* Methods */

	void (*free)(struct MC6809 *cpu);
	void (*reset)(struct MC6809 *cpu);
	void (*run)(struct MC6809 *cpu);
	void (*jump)(struct MC6809 *cpu, uint16_t pc);

	/* External handlers */

	/* Return result of a byte read cycle */
	uint8_t (*read_cycle)(uint16_t addr);
	/* Perform a byte write cycle */
	void (*write_cycle)(uint16_t addr, uint8_t value);
	/* Called just before instruction fetch if non-NULL */
	void (*instruction_hook)(struct MC6809 *cpu);
	/* Called after instruction is executed */
	void (*instruction_posthook)(struct MC6809 *cpu);
	/* Called just before an interrupt vector is read */
	void (*interrupt_hook)(struct MC6809 *cpu, uint16_t vector);

	/* Internal state */

	int state;
	_Bool running;

	/* Registers */
	uint8_t reg_cc, reg_dp;
	uint16_t reg_d;
	uint16_t reg_x, reg_y, reg_u, reg_s, reg_pc;
	/* Interrupts */
	_Bool nmi_armed;
	/* Interrupts are only recognised after being asserted for a number of
	 * cycles, so maintain an internal cycle count, and record when an
	 * interrupt is first seen. */
	unsigned cycle;
	unsigned halt_cycle, nmi_cycle, firq_cycle, irq_cycle;
};

#ifdef HAVE_BIG_ENDIAN
# define MC6809_REG_HI (0)
# define MC6809_REG_LO (1)
#else
# define MC6809_REG_HI (1)
# define MC6809_REG_LO (0)
#endif

#define MC6809_REG_A(cpu) (*((uint8_t *)&cpu->reg_d + MC6809_REG_HI))
#define MC6809_REG_B(cpu) (*((uint8_t *)&cpu->reg_d + MC6809_REG_LO))

#define MC6809_HALT_SET(cpu,val) do { if (!(cpu)->halt) { (cpu)->halt_cycle = (cpu)->cycle; } (cpu)->halt = (val); } while (0)
#define MC6809_NMI_SET(cpu,val) do { if (!(cpu)->nmi) { (cpu)->nmi_cycle = (cpu)->cycle; } (cpu)->nmi = (val); } while (0)
#define MC6809_FIRQ_SET(cpu,val) do { if (!(cpu)->firq) { (cpu)->firq_cycle = (cpu)->cycle; } (cpu)->firq = (val); } while (0)
#define MC6809_IRQ_SET(cpu,val) do { if (!(cpu)->irq) { (cpu)->irq_cycle = (cpu)->cycle; } (cpu)->irq = (val); } while (0)

struct MC6809 *mc6809_new(void);
void mc6809_init(struct MC6809 *cpu);

#endif  /* XROAR_MC6809_H_ */
