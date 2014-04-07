/*

Missing string handling functions
Copyright 2012-2014, Ciaran Anscomb

This is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version.

Supplies prototypes for string utility functions that were found to be
missing in the target's libc.

It is still necessary to define the appropriate feature macro (e.g.,
_BSD_SOURCE) before inclusion to get the right set of prototypes.

*/

#ifndef PL_STRING_H__7RCESJQ89DzWE
#define PL_STRING_H__7RCESJQ89DzWE

#include "config.h"

#ifdef _BSD_SOURCE

#ifdef NEED_STRSEP
char *strsep(char **, const char *);
#endif

#endif

#endif  /* PL_STRING_H__7RCESJQ89DzWE */
