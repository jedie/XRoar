/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_LOGGING_H_
#define XROAR_LOGGING_H_

#include "config.h"

#ifndef LOGGING

#define LOG_DEBUG(...) do {} while (0)
#define LOG_WARN(...) do {} while (0)
#define LOG_ERROR(...) do {} while (0)

#else

#include <stdio.h>

/* 0 - Silent, 1 - Title, 2 - Info, 3 - Details, 4 - Verbose, 5 - Silly */
/* Normally 3 */
#ifndef DEBUG_LEVEL
# define DEBUG_LEVEL 2
#endif

#define LOG_DEBUG(l,...) do { if (DEBUG_LEVEL >= l) { fprintf(stderr, __VA_ARGS__); } } while (0)
#define LOG_WARN(...) fprintf(stderr, "WARNING: " __VA_ARGS__)
#define LOG_ERROR(...) fprintf(stderr, "ERROR: " __VA_ARGS__)

#endif

#endif  /* XROAR_LOGGING_H_ */
