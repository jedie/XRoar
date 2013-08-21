#ifndef PORTALIB_STRINGS_H_
#define PORTALIB_STRINGS_H_

#include "config.h"

#ifdef XROAR_HAVE_STRINGS_H
#include <strings.h>
#endif

#ifdef NEED_STRCASECMP
int strcasecmp (const char *, const char *);
#endif

#endif  /* PORTALIB_STRINGS_H_ */
