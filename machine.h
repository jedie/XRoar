/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2005  Ciaran Anscomb
 *
 *  See COPYING for redistribution conditions. */

#ifndef __MACHINE_H__
#define __MACHINE_H__

#include "types.h"

/* Dragon 64s and later Dragon 32s used a 14.218MHz crystal
 * (ref: "Dragon 64 differences", Graham E. Kinns and then a motherboard) */
//#define OSCILLATOR_RATE 14218000
#define OSCILLATOR_RATE 14318180

/* NTSC timings: */
#define CYCLES_PER_SCANLINE 910
#define ACTIVE_SCANLINES_PER_FRAME 243
#define TOTAL_SCANLINES_PER_FRAME 262
#define TOP_BORDER_OFFSET 21
//#define CYCLES_PER_SCANLINE 910
//#define ACTIVE_SCANLINES_PER_FRAME 256
//#define TOTAL_SCANLINES_PER_FRAME 262
//#define TOP_BORDER_OFFSET 14

/* NTSC kludged timings: */
/*
#define CYCLES_PER_SCANLINE 910
#define ACTIVE_SCANLINES_PER_FRAME 262
#define TOTAL_SCANLINES_PER_FRAME 310
#define TOP_BORDER_OFFSET 25
*/

#define CYCLES_PER_FRAME (CYCLES_PER_SCANLINE * TOTAL_SCANLINES_PER_FRAME)

#define CPU_SLOW_DIVISOR 16
#define CPU_FAST_DIVISOR 8

#define NUM_MACHINES 3
#define DRAGON64 0
#define COCO 1
#define DRAGON32 2

#define IS_DRAGON64 (machine_romtype == DRAGON64)
#define IS_DRAGON32 (machine_romtype == DRAGON32)
/* Quicker test, but may need changing if there are more machines: */
#define IS_DRAGON (!IS_COCO)
#define IS_COCO (machine_romtype == COCO)

#define NUM_KEYBOARDS 2
#define DRAGON_KEYBOARD 0
#define COCO_KEYBOARD 1

#define IS_DRAGON_KEYBOARD (machine_keymap == DRAGON_KEYBOARD)
#define IS_COCO_KEYBOARD (machine_keymap == COCO_KEYBOARD)

typedef struct { unsigned int col, row; } Key;
typedef Key Keymap[128];

extern int machine_romtype, machine_keymap;
extern int dragondos_enabled;
extern uint_least16_t brk_csrdon, brk_bitin;
extern Keymap keymap;
extern uint8_t ram0[0x8000];
extern uint8_t ram1[0x8000];
extern uint8_t rom0[0x8000];
extern uint8_t rom1[0x8000];

void machine_init(void);
void machine_reset(int hard);
void machine_set_romtype(int mode);
void machine_set_keymap(int mode);

#endif  /* __MACHINE_H__ */
