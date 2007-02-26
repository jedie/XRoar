/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2007  Ciaran Anscomb
 *
 *  See COPYING for redistribution conditions. */

#ifndef __GP32_TYPES_H__
#define __GP32_TYPES_H__

#include <gpstdlib.h>
#include <gpstdio.h>

#define malloc gm_malloc
#define free gm_free

#define memset gm_memset
#define memcpy gm_memcpy
#define strcat gm_strcat
#define strcpy gm_strcpy

#define exit(r)
#define fprintf(...)
#define printf(...)
#define puts(...)

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef char int8_t;
typedef short int16_t;
typedef int int32_t;

typedef unsigned int uint_least8_t __attribute__ ((aligned (4)));
typedef unsigned int uint_least16_t __attribute__ ((aligned (4)));
typedef unsigned int uint_least32_t __attribute__ ((aligned (4)));
typedef int int_least8_t __attribute__ ((aligned (4)));
typedef int int_least16_t __attribute__ ((aligned (4)));
typedef int int_least32_t __attribute__ ((aligned (4)));

typedef int ssize_t;

typedef uint32_t Cycle __attribute__ ((aligned (4)));

#endif  /* __GP32_TYPES_H__ */
