#ifndef PORTALIB_GLIB_H_
#define PORTALIB_GLIB_H_

#include "config.h"

#ifdef HAVE_GLIB2
#include <glib.h>
#endif

#ifndef HAVE_GLIB2
/* Common macros, typedefs, etc. */
#include "pl_glib/gtypes.h"

/* Data types */
#include "pl_glib/ghash.h"
#include "pl_glib/gslist.h"

/* Utilities */
#include "pl_glib/galloca.h"
#include "pl_glib/gmem.h"
#include "pl_glib/gstrfuncs.h"
#endif

#endif  /* PORTALIB_GLIB_H_ */
