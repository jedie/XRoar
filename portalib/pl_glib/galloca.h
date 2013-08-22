/*
 * pl_glib is a set of public domain work-a-likes for some GLib functions and
 * macros.  It is not even slightly close to a complete reimplementation of
 * GLib.
 *
 * This header includes either alloca.h (normal systems) or malloc.h (windows
 * systems) for the prototype of alloca(), and aliases the appropriate GLib
 * equivalents.
 */

#ifndef PL_GLIB_GALLOCA_H_
#define PL_GLIB_GALLOCA_H_

#ifndef WINDOWS32
# include <alloca.h>
#else
# include <malloc.h>
#endif

#define g_alloca(size) alloca(size)

#endif  /* def PL_GLIB_GALLOCA_H_ */
