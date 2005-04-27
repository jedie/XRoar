/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2004  Ciaran Anscomb
 *
 *  See COPYING for redistribution conditions. */

/* This file is included by fs.h when compiling for GP32 target */

#include <gpstdlib.h>
#include <gpstdio.h>

#include "types.h"

#define FS_ATTR_DIRECTORY 16

struct dirent {
	char d_name[16];
	unsigned long attr;
	unsigned long size;
};

typedef F_HANDLE FS_FILE;

int fs_scandir(const char *dir, struct dirent ***namelist,
		int (*filter)(struct dirent *),
		int (*compar)(const void *, const void *));
void fs_freedir(struct dirent ***namelist);
