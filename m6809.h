/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2011  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_M6809_H_
#define XROAR_M6809_H_

#include "types.h"

#define M6809_COMPAT_STATE_NORMAL (0)
#define M6809_COMPAT_STATE_SYNC (1)
#define M6809_COMPAT_STATE_CWAI (2)
#define M6809_COMPAT_STATE_DONE_INSTRUCTION (11)
#define M6809_COMPAT_STATE_HCF (12)

enum m6809_cpu_state {
	m6809_flow_label_a      = M6809_COMPAT_STATE_NORMAL,
	m6809_flow_sync         = M6809_COMPAT_STATE_SYNC,
	m6809_flow_dispatch_irq = M6809_COMPAT_STATE_CWAI,
	m6809_flow_label_b,
	m6809_flow_reset,
	m6809_flow_reset_check_halt,
	m6809_flow_next_instruction,
	m6809_flow_instruction_page_2,
	m6809_flow_instruction_page_3,
	m6809_flow_cwai_check_halt,
	m6809_flow_sync_check_halt,
	m6809_flow_done_instruction = M6809_COMPAT_STATE_DONE_INSTRUCTION,
	m6809_flow_hcf          = M6809_COMPAT_STATE_HCF
};

typedef struct {
	uint8_t reg_cc, reg_a, reg_b, reg_dp;
	uint16_t reg_x, reg_y, reg_u, reg_s, reg_pc;
	int halt, nmi, firq, irq;
	int nmi_armed;
	enum m6809_cpu_state cpu_state;
} M6809State;

extern int m6809_running;

void m6809_init(void);
void m6809_reset(void);
void m6809_run(void);
void m6809_get_state(M6809State *state);
void m6809_set_state(M6809State *state);
void m6809_jump(unsigned int pc);

void m6809_halt_set(void);
void m6809_halt_clear(void);
void m6809_nmi_set(void);
void m6809_nmi_clear(void);
void m6809_firq_set(void);
void m6809_firq_clear(void);
void m6809_irq_set(void);
void m6809_irq_clear(void);

/*** Private ***/

/* Returns result of a byte read cycle */
extern unsigned int (*m6809_read_cycle)(unsigned int addr);

/* Performs a byte write cycle */
extern void (*m6809_write_cycle)(unsigned int addr, unsigned int value);

/* Non valid memory access ("busy") cycles */
extern void (*m6809_nvma_cycles)(int cycles);

/* Ensure all outside events are complete up to current cycle */
extern void (*m6809_sync)(void);

/* Called just before instruction fetch if non-NULL */
extern void (*m6809_instruction_hook)(M6809State *state);

/* Called after instruction is executed */
extern void (*m6809_instruction_posthook)(M6809State *state);

/* Called just before an interrupt vector is read */
extern void (*m6809_interrupt_hook)(unsigned int vector);

#endif  /* XROAR_M6809_H_ */
