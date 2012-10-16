/* Taken from musl libc v0.8.10, © 2005-2012 Rich Felker. */

/* This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#include "config.h"

#include <ctype.h>

#include "portalib/strings.h"

#ifdef NEED_STRCASECMP
int strcasecmp(const char *_l, const char *_r) {
	const unsigned char *l=(void *)_l, *r=(void *)_r;
	for (; *l && *r && (*l == *r || tolower(*l) == tolower(*r)); l++, r++);
	return tolower(*l) - tolower(*r);
}
#endif
