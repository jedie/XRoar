/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2007  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef __VDG_H__
#define __VDG_H__

#include "types.h"

#define VDG_CYCLES(c) ((c) * 4)

#define VDG_tFP   VDG_CYCLES(7.0)
#define VDG_tBP   VDG_CYCLES(17.5)
#define VDG_tLB   VDG_CYCLES(29.5)
#define VDG_tRB   VDG_CYCLES(28.0)
#define VDG_tWHS  VDG_CYCLES(17.5)
#define VDG_tAV   VDG_CYCLES(128)

#define VDG_LEFT_BORDER_UNSEEN    (VDG_tLB - VDG_CYCLES(16))

/* All horizontal timings shall remain relative to the HS pulse falling edge */
#define VDG_HS_FALLING_EDGE    (0)
#define VDG_HS_RISING_EDGE     (VDG_HS_FALLING_EDGE + VDG_tWHS)
#define VDG_LEFT_BORDER_START  (VDG_HS_RISING_EDGE + VDG_tBP)
#define VDG_ACTIVE_LINE_START  (VDG_LEFT_BORDER_START + VDG_tLB)
#define VDG_RIGHT_BORDER_START (VDG_ACTIVE_LINE_START + VDG_tAV)
#define VDG_RIGHT_BORDER_END   (VDG_RIGHT_BORDER_START + VDG_tRB)
#define VDG_LINE_DURATION      VDG_CYCLES(228)
#define VDG_PAL_PADDING_LINE   VDG_LINE_DURATION

#define VDG_VBLANK_START       (0)
#define VDG_TOP_BORDER_START   (VDG_VBLANK_START + 13)
#define VDG_ACTIVE_AREA_START  (VDG_TOP_BORDER_START + 25)
#define VDG_ACTIVE_AREA_END    (VDG_ACTIVE_AREA_START + 192)
#define VDG_BOTTOM_BORDER_END  (VDG_ACTIVE_AREA_END + 26)
#define VDG_VRETRACE_END       (VDG_BOTTOM_BORDER_END + 6)
#define VDG_FRAME_DURATION     (262)

extern const unsigned int vdg_alpha[768];
extern Cycle scanline_start;
extern int beam_pos;

void vdg_init(void);
void vdg_reset(void);
void vdg_set_mode(void);

#endif  /* __VDG_H__ */
