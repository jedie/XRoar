/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2006  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef __XROAR_H__
#define __XROAR_H__

#include "types.h"

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

extern int requested_frameskip;
extern int frameskip;
extern int noratelimit;
extern int video_artifact_mode;

#ifdef TRACE
extern int trace;
# define IF_TRACE(s) if (trace) { s; }
#else
# define trace 0
# define IF_TRACE(s)
#endif

void xroar_getargs(int argc, char **argv);
int xroar_init(int argc, char **argv);
void xroar_shutdown(void);
void xroar_mainloop(void);
int xroar_filetype_by_ext(const char *filename);

#endif  /* __XROAR_H__ */
