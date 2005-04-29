/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2005  Ciaran Anscomb
 *
 *  See COPYING for redistribution conditions. */

/* This file is included by video.h when compiling for GP32 target */

#include <gpdef.h>
#include <gpstdlib.h>
#include <gpgraphic.h>
#include <gpstdio.h>
#include <gpfont.h>

#include "types.h"

#define VIDEO_BPP GPC_DFLAG_8BPP
typedef uint8_t Pixel;
#define MAPCOLOUR(r,g,b) ((r & 0xc0) | (g & 0xe0) >> 2 | (b & 0xe0) >> 5)

#define VIDEO_UPDATE
#define VIDEO_SCREENBASE ((Pixel *)screen.ptbuffer)
#define VIDEO_CURRENTBASE ((Pixel *)screen.ptbuffer)
#define VIDEO_CURRENT_SCREEN (0)

#define XSTEP 240
#define NEXTLINE -76801
#define VIDEO_TOPLEFT (VIDEO_SCREENBASE + 239)
#define VIDEO_VIEWPORT_XOFFSET (32 * 240)
#define VIDEO_VIEWPORT_YOFFSET (-7)
#define VIDEO_VIEWPORT_TOPLEFT (VIDEO_TOPLEFT + VIDEO_VIEWPORT_XOFFSET + VIDEO_VIEWPORT_YOFFSET)

extern GPDRAWSURFACE screen;
