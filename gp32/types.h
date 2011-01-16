/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2011  Ciaran Anscomb
 *
 *  See COPYING for redistribution conditions. */

#ifndef XROAR_GP32_TYPES_H_
#define XROAR_GP32_TYPES_H_

#include <gpstdlib.h>
#include <gpstdio.h>
#include "gplib.h"

#define malloc gm_malloc
#define free gm_free

#define memset gm_memset
#define memcpy gm_memcpy
#define strcat gm_strcat
#define strcpy gm_strcpy
#define strdup gm_strdup

#define exit(r)
#define fprintf(...)
#define printf(...)
#define puts(...)
#define getenv(e) (NULL)

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef char int8_t;
typedef short int16_t;
typedef int int32_t;

typedef unsigned int uint_least8_t __attribute__ ((aligned (4)));
typedef unsigned int uint_least16_t __attribute__ ((aligned (4)));
typedef unsigned int uint_least32_t __attribute__ ((aligned (4)));
typedef int int_least8_t __attribute__ ((aligned (4)));
typedef int int_least16_t __attribute__ ((aligned (4)));
typedef int int_least32_t __attribute__ ((aligned (4)));

typedef int ssize_t;

typedef uint32_t Cycle __attribute__ ((aligned (4)));

/* FCLK = 100/50/50MHz */
/*
#define SPEED_FAST GpClockSpeedChange(100000000, 0x2a011, 2)
#define SPEED_SLOW GpClockSpeedChange(50000000, 0x2a012, 0)
#define PCLK 50000000
*/

/* FCLK = 90/45/45MHz */
/*
#define SPEED_FAST GpClockSpeedChange(90000000, 0x25011, 2)
#define SPEED_SLOW GpClockSpeedChange(45000000, 0x25012, 0)
#define PCLK 45000000
*/

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
#define MMU_NCNB (0xff2)        /* Noncached, nonbuffered */
#define MMU_NCB (0xff6)         /* Noncached, buffered */
#define MMU_WT (0xffa)          /* Cached write-through mode */
#define MMU_WB (0xffe)          /* Cached write-back mode */

#endif  /* XROAR_GP32_TYPES_H_ */
