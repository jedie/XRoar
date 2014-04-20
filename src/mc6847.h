/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2014  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_VDG_H_
#define XROAR_VDG_H_

#include <stdint.h>

#include "delegate.h"

// Horizontal timing, all measured in half-VDG-clocks (i.e., pixels)

#define VDG_tFP   (17)  // 14
#define VDG_tWHS  (32)  // 35
#define VDG_tBP   (35)
#define VDG_tHBNK (VDG_tFP + VDG_tWHS + VDG_tBP)
#define VDG_tLB   (60)  // 59
#define VDG_tAV   (256)
#define VDG_tRB   (56)
#define VDG_tAVB  (VDG_tLB + VDG_tAV + VDG_tRB)
#define VDG_tHST  (VDG_tHBNK + VDG_tAVB)
// tHCD = time from start of back porch to beginning of colour burst
#define VDG_tHCD  (7)
// tCB = duration of colour burst
#define VDG_tCB   (21)

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

/* Nothing in this struct yet, might expose some things in future. */
struct MC6847 {
	/* Delegates to notify on signal edges */
	DELEGATE_T1(void, bool) signal_hs;
	DELEGATE_T1(void, bool) signal_fs;
	/* External handler to fetch data for display.  First arg is number of bytes,
	 * second a pointer to a buffer to receive them. */
	DELEGATE_T2(void, int, uint8p) fetch_bytes;
};

struct MC6847 *mc6847_new(_Bool t1);
void mc6847_free(struct MC6847 *);

void mc6847_reset(struct MC6847 *);

void mc6847_set_inverted_text(struct MC6847 *, _Bool);

/*
 * Mode bits:
 * 8    ¬INT/EXT
 * 7    ¬A/G
 * 6..4 GM2..GM0
 * 3    CSS
 * 2..0 ignored
 */

void mc6847_set_mode(struct MC6847 *, unsigned mode);

void mc6847_set_ext_charset(struct MC6847 *, uint8_t *);

#endif  /* XROAR_VDG_H_ */
