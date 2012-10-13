#ifndef PORTALIB_GLIB_H_
#define PORTALIB_GLIB_H_

#include "config.h"

#ifdef HAVE_GLIB2
#include <glib.h>
#endif

#ifndef HAVE_GLIB2
#include "portalib/glib/gmacros.h"
#include "portalib/glib/gtypes.h"
#include "portalib/glib/galloca.h"
#include "portalib/glib/gmem.h"
#include "portalib/glib/gstrfuncs.h"
#include "portalib/glib/ghash.h"
#include "portalib/glib/gslist.h"
#endif

#endif  /* PORTALIB_GLIB_H_ */
