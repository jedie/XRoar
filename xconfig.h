/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2011  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_XCONFIG_H_
#define XROAR_XCONFIG_H_

#define XC_OPT_BOOL(o,d) { .type = XCONFIG_BOOL, .name = (o), .dest = (d) }
#define XC_OPT_BOOL0(o,d) { .type = XCONFIG_BOOL0, .name = (o), .dest = (d) }
#define XC_OPT_INT(o,d) { .type = XCONFIG_INT, .name = (o), .dest = (d) }
#define XC_OPT_DOUBLE(o,d) { .type = XCONFIG_DOUBLE, .name = (o), .dest = (d) }
#define XC_OPT_STRING(o,d) { .type = XCONFIG_STRING, .name = (o), .dest = (d) }
#define XC_OPT_CALL_0(o,d) { .type = XCONFIG_CALL_0, .name = (o), .dest = (d) }
#define XC_OPT_CALL_1(o,d) { .type = XCONFIG_CALL_1, .name = (o), .dest = (d) }
#define XC_OPT_ENUM(o,d,e) { .type = XCONFIG_ENUM, .name = (o), .dest = (d), .ref = (e) }
#define XC_OPT_END() { .type = XCONFIG_END }

#define XC_ENUM_END() { .name = NULL }

enum xconfig_result {
	XCONFIG_OK = 0,
	XCONFIG_BAD_OPTION,
	XCONFIG_MISSING_ARG,
	XCONFIG_FILE_ERROR
};

enum xconfig_option_type {
	XCONFIG_BOOL,
	XCONFIG_BOOL0,  /* unsets a BOOL */
	XCONFIG_INT,
	XCONFIG_DOUBLE,
	XCONFIG_STRING,
	XCONFIG_STRING0,  /* unsets a STRING */
	XCONFIG_CALL_0,
	XCONFIG_CALL_1,
	XCONFIG_ENUM,
	XCONFIG_END
};

struct xconfig_option {
	enum xconfig_option_type type;
	const char *name;
	void *dest;
	void *ref;
};

struct xconfig_enum {
	int value;
	const char *name;
	const char *description;
};

/* For error reporting: */
extern char *xconfig_option;
extern int xconfig_line_number;

enum xconfig_result xconfig_parse_file(struct xconfig_option *options,
		const char *filename);

enum xconfig_result xconfig_parse_cli(struct xconfig_option *options,
		int argc, char **argv, int *argn);

#endif  /* XROAR_XCONFIG_H_ */
