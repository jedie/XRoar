/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2012  Ciaran Anscomb
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
extern int m6809_halt, m6809_nmi;
extern int m6809_firq, m6809_irq;

#define m6809_init()
void m6809_reset(void);
void m6809_run(void);
void m6809_get_state(M6809State *state);
void m6809_set_state(M6809State *state);
void m6809_jump(unsigned int pc);

#define m6809_halt_set() do { m6809_halt = 1; } while (0)
#define m6809_halt_clear() do { m6809_halt = 0; } while (0)
#define m6809_nmi_set() do { m6809_nmi = 1; } while (0)
#define m6809_nmi_clear() do { m6809_nmi = 0; } while (0)
#define m6809_firq_set() do { m6809_firq = 1; } while (0)
#define m6809_firq_clear() do { m6809_firq = 0; } while (0)
#define m6809_irq_set() do { m6809_irq = 1; } while (0)
#define m6809_irq_clear() do { m6809_irq = 0; } while (0)

/*** Private ***/

/* Returns result of a byte read cycle */
extern uint8_t (*m6809_read_cycle)(uint16_t addr);

/* Performs a byte write cycle */
extern void (*m6809_write_cycle)(uint16_t addr, uint8_t value);

/* Non valid memory access ("busy") cycles */
extern void (*m6809_nvma_cycles)(int cycles);

/* Called just before instruction fetch if non-NULL */
extern void (*m6809_instruction_hook)(M6809State *state);

/* Called after instruction is executed */
extern void (*m6809_instruction_posthook)(M6809State *state);

/* Called just before an interrupt vector is read */
extern void (*m6809_interrupt_hook)(uint16_t vector);

#endif  /* XROAR_M6809_H_ */
