/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2009  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef __XCONFIG_H__
#define __XCONFIG_H__

#include "types.h"

enum xconfig_result {
	XCONFIG_OK = 0,
	XCONFIG_BAD_OPTION,
	XCONFIG_MISSING_ARG,
	XCONFIG_FILE_ERROR
};

enum xconfig_option_type {
	XCONFIG_BOOL,
	XCONFIG_INT,
	XCONFIG_FLOAT,
	XCONFIG_STRING,
	XCONFIG_END
};

struct xconfig_option {
	enum xconfig_option_type type;
	const char *name;
	void *dest;
};

enum xconfig_result xconfig_parse_file(struct xconfig_option *options,
		const char *filename);

enum xconfig_result xconfig_parse_cli(struct xconfig_option *options,
		int argc, char **argv, int *argn);

#endif  /* __XCONFIG_H__ */
