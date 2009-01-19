/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2009  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef __XROAR_H__
#define __XROAR_H__

#include "types.h"
#include "events.h"

#define RESET_SOFT 0
#define RESET_HARD 1

#define FILETYPE_UNKNOWN (0)
#define FILETYPE_VDK (1)
#define FILETYPE_JVC (2)
#define FILETYPE_DMK (3)
#define FILETYPE_BIN (4)
#define FILETYPE_HEX (5)
#define FILETYPE_CAS (6)
#define FILETYPE_WAV (7)
#define FILETYPE_SNA (8)
#define FILETYPE_ROM (9)

extern int requested_frameskip;
extern int frameskip;
extern int noratelimit;
extern int xroar_trace_enabled;

#define UI_EVENT_LIST xroar_ui_events
#define MACHINE_EVENT_LIST xroar_machine_events
extern event_t *xroar_ui_events;
extern event_t *xroar_machine_events;

void xroar_getargs(int argc, char **argv);
int xroar_init(int argc, char **argv);
void xroar_shutdown(void);
void xroar_mainloop(void);
int xroar_filetype_by_ext(const char *filename);
int xroar_load_file(const char *filename, int autorun);

#endif  /* __XROAR_H__ */
