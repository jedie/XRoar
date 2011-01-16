/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2011  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_TYPES_H_
#define XROAR_TYPES_H_

#include "config.h"
#include "portalib.h"

#ifdef HAVE_GP32
# include "gp32/types.h"
#endif

#ifdef HAVE_NDS
# include "nds/types.h"
#endif

#ifndef HAVE_GP32
#include <sys/types.h>
#include <inttypes.h>

typedef int32_t Cycle;
#endif

#endif  /* XROAR_TYPES_H_ */
