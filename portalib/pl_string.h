/*
 * pl_string.h supplies prototypes for string utility functions that were found
 * not to be supported by the target's libc.
 *
 * It is still necessary to define the appropriate feature macro (e.g.,
 * _BSD_SOURCE) before inclusion to get the right set of prototypes.
 *
 * Of course there's only one function here so far, so that doesn't mean much.
 */

#ifndef PL_STRING_H_
#define PL_STRING_H_

#include "config.h"

#ifdef _BSD_SOURCE

#ifdef NEED_STRSEP
char *strsep(char **, const char *);
#endif

#endif

#endif  /* PL_STRING_H_ */
