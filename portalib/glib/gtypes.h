/* Portalib - replace common functions from other libraries. */

/* portalib/glib/gtypes.h - wrap glib/gtypes.h */

#ifndef PORTALIB_GLIB_GTYPES_H_
#define PORTALIB_GLIB_GTYPES_H_

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

#endif  /* def PORTALIB_GLIB_GTYPES_H_ */
