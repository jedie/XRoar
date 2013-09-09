/*
 * pl_glib is a set of public domain work-a-likes for some GLib functions and
 * macros.  It is not even slightly close to a complete reimplementation of
 * GLib.
 *
 * This header defines types and macros used throughout.  Most GLib types are
 * just aliases to standard C types.
 */

#ifndef PL_GLIB_GTYPES_H_
#define PL_GLIB_GTYPES_H_

#ifndef FALSE
#define FALSE (0)
#endif

#ifndef TRUE
#define TRUE (!FALSE)
#endif

#define G_N_ELEMENTS(arr) (sizeof(arr)/sizeof((arr)[0]))

typedef char gchar;
typedef short gshort;
typedef long glong;
typedef int gint;
typedef int gboolean;

typedef unsigned char guchar;
typedef unsigned short gushort;
typedef unsigned long gulong;
typedef unsigned int guint;

typedef void *gpointer;
typedef const void *gconstpointer;

typedef unsigned long gsize;

typedef gint (*GCompareFunc)(gconstpointer a, gconstpointer b);
typedef gboolean (*GEqualFunc)(gconstpointer a, gconstpointer b);
typedef void (*GFunc)(gpointer data, gpointer user_data);
typedef guint (*GHashFunc)(gconstpointer key);
typedef void (*GHFunc)(gpointer key, gpointer value, gpointer user_data);

#endif  /* def PL_GLIB_GTYPES_H_ */
