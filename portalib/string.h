#ifndef PORTALIB_STRING_H_
#define PORTALIB_STRING_H_

#include "config.h"

#include <string.h>

#if defined(NEED_STRSEP) && defined(_BSD_SOURCE)
char *strsep(char **, const char *);
#endif

#endif  /* PORTALIB_STRING_H_ */
