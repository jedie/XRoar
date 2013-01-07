/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_VDG_H_
#define XROAR_VDG_H_

#include <inttypes.h>

#define VDG_CYCLES(c) ((int)((c) * 4))

#define VDG_tFP   VDG_CYCLES(8.5)   /* 7.0 */
#define VDG_tWHS  VDG_CYCLES(16.0)  /* 17.5 */
#define VDG_tBP   VDG_CYCLES(17.5)
#define VDG_tHBNK (VDG_tFP + VDG_tWHS + VDG_tBP)
#define VDG_tLB   VDG_CYCLES(30.0)  /* 29.5 */
#define VDG_tAV   VDG_CYCLES(128)
#define VDG_tRB   VDG_CYCLES(28.0)
#define VDG_tAVB  (VDG_tLB + VDG_tAV + VDG_tRB)
#define VDG_tHST  (VDG_tHBNK + VDG_tAVB)
/* tHCD = time from start of back porch to beginning of colour burst */
#define VDG_tHCD  VDG_CYCLES(3.5)
/* tCB = duration of colour burst */
#define VDG_tCB   VDG_CYCLES(10.5)

#define VDG_LEFT_BORDER_UNSEEN    (VDG_tLB - VDG_CYCLES(16))

/* All horizontal timings shall remain relative to the HS pulse falling edge */
#define VDG_HS_FALLING_EDGE    (0)
#define VDG_HS_RISING_EDGE     (VDG_HS_FALLING_EDGE + VDG_tWHS)
#define VDG_LEFT_BORDER_START  (VDG_HS_FALLING_EDGE + VDG_tWHS + VDG_tBP)
#define VDG_ACTIVE_LINE_START  (VDG_LEFT_BORDER_START + VDG_tLB)
#define VDG_RIGHT_BORDER_START (VDG_ACTIVE_LINE_START + VDG_tAV)
#define VDG_RIGHT_BORDER_END   (VDG_RIGHT_BORDER_START + VDG_tRB)
#define VDG_LINE_DURATION      (VDG_tHBNK + VDG_tAVB)
#define VDG_PAL_PADDING_LINE   VDG_LINE_DURATION

#define VDG_VBLANK_START       (0)
#define VDG_TOP_BORDER_START   (VDG_VBLANK_START + 13)
#define VDG_ACTIVE_AREA_START  (VDG_TOP_BORDER_START + 25)
#define VDG_ACTIVE_AREA_END    (VDG_ACTIVE_AREA_START + 192)
#define VDG_BOTTOM_BORDER_END  (VDG_ACTIVE_AREA_END + 26)
#define VDG_VRETRACE_END       (VDG_BOTTOM_BORDER_END + 6)
#define VDG_FRAME_DURATION     (262)

/* Basic colours the VDG can generate */

enum vdg_colour {
	VDG_GREEN, VDG_YELLOW, VDG_BLUE, VDG_RED,
	VDG_WHITE, VDG_CYAN, VDG_MAGENTA, VDG_ORANGE,
	VDG_BLACK, VDG_DARK_GREEN, VDG_DARK_ORANGE, VDG_BRIGHT_ORANGE
};

/* External handler to fetch data for display.  First arg is number of bytes,
 * second a pointer to a buffer to receive them. */
extern void (*vdg_fetch_bytes)(int, uint8_t *);

void vdg_init(void);
void vdg_reset(void);
/* Mode bits:
 * 7..5  GM2..GM0
 * 4    ¬INT/EXT
 * 3    CSS */
void vdg_set_mode(unsigned mode);

#endif  /* XROAR_VDG_H_ */
