/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2010  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_XCONFIG_H_
#define XROAR_XCONFIG_H_

enum xconfig_result {
	XCONFIG_OK = 0,
	XCONFIG_BAD_OPTION,
	XCONFIG_MISSING_ARG,
	XCONFIG_FILE_ERROR
};

enum xconfig_option_type {
	XCONFIG_BOOL,
	XCONFIG_INT,
	XCONFIG_DOUBLE,
	XCONFIG_STRING,
	XCONFIG_CALL_0,
	XCONFIG_CALL_1,
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

#endif  /* XROAR_XCONFIG_H_ */
