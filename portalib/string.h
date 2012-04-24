#ifndef PORTALIB_STRING_H_
#define PORTALIB_STRING_H_

#include "config.h"

#include <string.h>

#ifdef NEED_STRSEP
char *strsep(char **, const char *);
#endif

#endif  /* PORTALIB_STRING_H_ */
