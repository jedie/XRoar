/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2005  Ciaran Anscomb
 *
 *  See COPYING for redistribution conditions. */

#ifndef __TYPES_H__
#define __TYPES_H__

#ifdef HAVE_GP32  /* GP32 types & macros */

#include "gp32/types.h"  /* base types */

/* All this affects is how registers A and B combine to form D */
#define WRONG_ENDIAN

/* FCLK = 80/40/40MHz */

#define SPEED_FAST GpClockSpeedChange(80000000, 0x48012, 2)
#define SPEED_SLOW GpClockSpeedChange(40000000, 0x48013, 0)
#define PCLK 40000000


/* FCLK = 72/36/36MHz */
/*
#define SPEED_FAST GpClockSpeedChange(72000000, 0x28002, 2)
#define SPEED_SLOW GpClockSpeedChange(36000000, 0x28003, 0)
#define PCLK 36000000
*/

/* FCLK = 66/33/33MHz */
/*
#define SPEED_FAST GpClockSpeedChange(66000000, 0x24002, 2)
#define SPEED_SLOW GpClockSpeedChange(33000000, 0x24003, 0)
#define PCLK 33000000
*/

/* These are in the SDK but not declared: */
void ARMEnableInterrupt(void);
void ARMDisableInterrupt(void);
void swi_install_irq(int irqnum, void *service_routine);
void swi_uninstall_irq(int irqnum, void *service_routine);
void swi_mmu_change(void *mempos_start, void *mempos_end, int mode);

/* For use with swi_mmu_change: */
#define MMU_NCNB (0xff2)	/* Noncached, nonbuffered */
#define MMU_NCB (0xff6)		/* Noncached, buffered */
#define MMU_WT (0xffa)		/* Cached write-through mode */
#define MMU_WB (0xffe)		/* Cached write-back mode */

#else  /* Unix types & macros */

#include <inttypes.h>

typedef int32_t Cycle;

#endif  /* Tests for architecture */

#endif  /* __TYPES_H__ */
