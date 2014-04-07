/*

C locale string functions
Copyright 2014, Ciaran Anscomb

This is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version.

String functions that act as if the locale were 'C'.  See Gnulib for
a far more complete implementation that also handles edge cases like
non-ASCII-compatible chars in the execution environment.

*/

#ifndef C_STRCASE_H__vD9hqIMCxQQDM
#define C_STRCASE_H__vD9hqIMCxQQDM

#include <stddef.h>

int c_strcasecmp(const char *s1, const char *s2);
int c_strncasecmp(const char *s1, const char *s2, size_t n);

#endif  /* C_STRCASE_H__vD9hqIMCxQQDM */
