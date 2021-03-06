/*

C locale string functions
Copyright 2014, Ciaran Anscomb

This is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version.

Assumes ASCII.

*/

#include "config.h"

#include <stddef.h>

#include "c-ctype.h"
#include "c-strcase.h"

int c_strcasecmp(const char *s1, const char *s2) {
	if (!s1 || !s2)
		return 0;
	while (*s1 && *s2 && (*s1 == *s2 || c_tolower(*s1) == c_tolower(*s2))) {
		s1++;
		s2++;
	}
	return c_tolower(*s1) - c_tolower(*s2);
}
