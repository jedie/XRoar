/*
 * pl_glib is a set of public domain work-a-likes for some GLib functions and
 * macros.  It is not even slightly close to a complete reimplementation of
 * GLib.
 *
 * This header will simply wrap the normal GLib headers if HAVE_GLIB2 is
 * defined.
 *
 * On systems with GLib, use GLib.  For others, this lets you use some GLib
 * functions without delving into the crazy world of GLib circular
 * dependencies.
 */

#ifndef PL_GLIB_H_
#define PL_GLIB_H_

#include "config.h"

#ifdef HAVE_GLIB2
#include <glib.h>
#endif

#ifndef HAVE_GLIB2
/* Common macros, typedefs, etc. */
#include "pl_glib/gtypes.h"

/* Data types */
#include "pl_glib/gslist.h"

/* Utilities */
#include "pl_glib/galloca.h"
#include "pl_glib/gmem.h"
#include "pl_glib/gstrfuncs.h"
#endif

#endif  /* PL_GLIB_H_ */
