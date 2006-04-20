/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2006  Ciaran Anscomb
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
/*
#define CYCLES_PER_SCANLINE 910
#define ACTIVE_SCANLINES_PER_FRAME 243
#define TOTAL_SCANLINES_PER_FRAME 262
#define TOP_BORDER_OFFSET 20
*/

#define CYCLES_PER_SCANLINE 912
#define FIRST_DISPLAYED_SCANLINE 14
#define SCANLINE_OF_FS_IRQ_LOW 229
#define SCANLINE_OF_FS_IRQ_HIGH 261
#define TOTAL_SCANLINES_PER_FRAME 262

#define CYCLES_PER_LINE_PULSE 928
#define SCANLINE_OF_DELAYED_FS 253

#define CYCLES_PER_FRAME (CYCLES_PER_SCANLINE * TOTAL_SCANLINES_PER_FRAME)

#define CPU_SLOW_DIVISOR 16
#define CPU_FAST_DIVISOR 8

/* machine_romtype will be one of: */
#define NUM_MACHINES 4
#define DRAGON32 0
#define DRAGON64 1
#define TANO_DRAGON 2
#define COCO 3

#define IS_DRAGON64 (machine_romtype == DRAGON64 || machine_romtype == TANO_DRAGON)
#define IS_DRAGON32 (machine_romtype == DRAGON32)
/* Quicker test, but may need changing if there are more machines: */
#define IS_DRAGON (!IS_COCO)
#define IS_COCO (machine_romtype == COCO)

#define IS_PAL (machines[machine_romtype].tv_standard == PAL)
#define IS_NTSC (machines[machine_romtype].tv_standard == NTSC)

/* machine_keymap will be set to one of: */
#define NUM_KEYBOARDS 2
#define DRAGON_KEYBOARD 0
#define COCO_KEYBOARD 1

#define IS_DRAGON_KEYBOARD (machine_keymap == DRAGON_KEYBOARD)
#define IS_COCO_KEYBOARD (machine_keymap == COCO_KEYBOARD)

typedef struct { unsigned int col, row; } Key;
typedef Key Keymap[128];

typedef enum { PAL, NTSC } TVStandard;

typedef struct {
	const char *name;
	const char *description;
	unsigned int keymap;
	TVStandard tv_standard;
	const char *bas[5];
	const char *extbas[5];
	const char *dos[5];
	const char *rom1[5];
} machine_info;

extern machine_info machines[];
extern unsigned int machine_romtype;
extern unsigned int machine_keymap;
extern int dragondos_enabled;
extern Keymap keymap;
extern uint8_t ram0[0x8000];
extern uint8_t ram1[0x8000];
extern uint8_t rom0[0x8000];
extern uint8_t rom1[0x8000];

extern int noextbas;

void machine_helptext(void);
void machine_getargs(int argc, char **argv);
void machine_init(void);
void machine_reset(int hard);
void machine_set_romtype(int mode);
void machine_set_keymap(int mode);

#endif  /* __MACHINE_H__ */
