/*

Missing alloca.h header
Copyright 2014, Ciaran Anscomb

This is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version.

*/

#ifndef PL_ALLOCA_H__dJ9CTN8z1Q61w
#define PL_ALLOCA_H__dJ9CTN8z1Q61w

#include "config.h"

#if defined(HAVE_ALLOCA_ALLOCA_H)
#include <alloca.h>
#elif defined(HAVE_ALLOCA_STDLIB_H)
#include <stdlib.h>
#elif defined(HAVE_ALLOCA_MALLOC_H)
#include <malloc.h>
#endif

#endif  /* PL_ALLOCA_H__dJ9CTN8z1Q61w */
