/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2008  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef __LOGGING_H__
#define __LOGGING_H__

#ifdef HAVE_GP32

#define LOG_DEBUG(...)
#define LOG_WARN(...)
#define LOG_ERROR(...)

#else

#include <stdio.h>

/* 0 - Silent, 1 - Title, 2 - Info, 3 - Details, 4 - Verbose, 5 - Silly */
/* Normally 3 */
#ifndef DEBUG_LEVEL
# define DEBUG_LEVEL 3
#endif

#define LOG_DEBUG(l,...) do { if (DEBUG_LEVEL >= l) { fprintf(stderr, __VA_ARGS__); } } while (0)
#define LOG_WARN(...) fprintf(stderr, "WARNING: " __VA_ARGS__)
#define LOG_ERROR(...) fprintf(stderr, "ERROR: " __VA_ARGS__)

#endif

#endif  /* __LOGGING_H__ */
