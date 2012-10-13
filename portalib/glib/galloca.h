/* Portalib - replace common functions from other libraries. */

/* portalib/glib/galloca.h */

#ifndef PORTALIB_GLIB_GALLOCA_H_
#define PORTALIB_GLIB_GALLOCA_H_

#ifdef WINDOWS32

# include <malloc.h>

#else

# include <alloca.h>

#endif

#define g_alloca(size) alloca(size)

#endif  /* def PORTALIB_GLIB_GALLOCA_H_ */
