/* Functions taken from from musl libc, © 2005-2011 Rich Felker. */

#include "config.h"

#include <stdlib.h>
#include <ctype.h>

#include "portalib/strings.h"

#ifdef NEED_STRCASECMP
int strcasecmp(const char *_l, const char *_r) {
	const unsigned char *l=(void *)_l, *r=(void *)_r;
	for (; *l && *r && (*l == *r || tolower(*l) == tolower(*r)); l++, r++);
	return tolower(*l) - tolower(*r);
}
#endif
