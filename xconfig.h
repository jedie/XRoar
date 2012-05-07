/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2012  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_XCONFIG_H_
#define XROAR_XCONFIG_H_

#define XC_OPT_BOOL(o,d,...) { .type = XCONFIG_BOOL, .name = (o), .dest = (d), __VA_ARGS__ }
#define XC_OPT_BOOL0(o,d,...) { .type = XCONFIG_BOOL0, .name = (o), .dest = (d), __VA_ARGS__ }
#define XC_OPT_INT(o,d,...) { .type = XCONFIG_INT, .name = (o), .dest = (d), __VA_ARGS__ }
#define XC_OPT_INT0(o,d,...) { .type = XCONFIG_INT0, .name = (o), .dest = (d), __VA_ARGS__ }
#define XC_OPT_INT1(o,d,...) { .type = XCONFIG_INT1, .name = (o), .dest = (d), __VA_ARGS__ }
#define XC_OPT_DOUBLE(o,d,...) { .type = XCONFIG_DOUBLE, .name = (o), .dest = (d), __VA_ARGS__ }
#define XC_OPT_STRING(o,d,...) { .type = XCONFIG_STRING, .name = (o), .dest = (d), __VA_ARGS__ }
#define XC_OPT_NULL(o,...) { .type = XCONFIG_NULL, .name = (o), __VA_ARGS__ }
#define XC_OPT_ENUM(o,d,e,...) { .type = XCONFIG_ENUM, .name = (o), .dest = (d), .ref = (e), __VA_ARGS__ }

#define XC_OPT_CALL(typemac,...) typemac(__VA_ARGS__, .call = 1)

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
	XCONFIG_INT0,  /* sets an int to 0 */
	XCONFIG_INT1,  /* sets an int to 1 */
	XCONFIG_DOUBLE,
	XCONFIG_STRING,
	XCONFIG_NULL,
	XCONFIG_ENUM,
	XCONFIG_END
};

struct xconfig_option {
	enum xconfig_option_type type;
	const char *name;
	void *dest;
	void *ref;
	_Bool call;
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
