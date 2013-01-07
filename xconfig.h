/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_XCONFIG_H_
#define XROAR_XCONFIG_H_

#define XC_SET_BOOL(o,d) { .type = XCONFIG_BOOL, .name = (o), .dest.object = (d) }
#define XC_SET_BOOL0(o,d) { .type = XCONFIG_BOOL0, .name = (o), .dest.object = (d) }
#define XC_SET_INT(o,d) { .type = XCONFIG_INT, .name = (o), .dest.object = (d) }
#define XC_SET_INT0(o,d) { .type = XCONFIG_INT0, .name = (o), .dest.object = (d) }
#define XC_SET_INT1(o,d) { .type = XCONFIG_INT1, .name = (o), .dest.object = (d) }
#define XC_SET_DOUBLE(o,d) { .type = XCONFIG_DOUBLE, .name = (o), .dest.object = (d) }
#define XC_SET_STRING(o,d) { .type = XCONFIG_STRING, .name = (o), .dest.object = (d) }
#define XC_SET_ENUM(o,d,e) { .type = XCONFIG_ENUM, .name = (o), .ref = (e), .dest.object = (d) }

#define XC_CALL_BOOL(o,d) { .type = XCONFIG_BOOL, .name = (o), .dest.func_bool = (d), .call = 1 }
#define XC_CALL_BOOL0(o,d) { .type = XCONFIG_BOOL0, .name = (o), .dest.func_bool = (d), .call = 1 }
#define XC_CALL_INT(o,d) { .type = XCONFIG_INT, .name = (o), .dest.func_int = (d), .call = 1 }
#define XC_CALL_INT0(o,d) { .type = XCONFIG_INT0, .name = (o), .dest.func_int = (d), .call = 1 }
#define XC_CALL_INT1(o,d) { .type = XCONFIG_INT1, .name = (o), .dest.func_int = (d), .call = 1 }
#define XC_CALL_DOUBLE(o) { .type = XCONFIG_DOUBLE, .name = (o), .dest.func_double = (d), .call = 1 }
#define XC_CALL_STRING(o,d) { .type = XCONFIG_STRING, .name = (o), .dest.func_string = (xconfig_func_string)(d), .call = 1 }
#define XC_CALL_NULL(o,d) { .type = XCONFIG_NULL, .name = (o), .dest.func_null = (d), .call = 1 }
#define XC_CALL_ENUM(o,d,e) { .type = XCONFIG_ENUM, .name = (o), .ref = (e), .dest.func_int = (d), .call = 1 }

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

typedef void (*xconfig_func_bool)(_Bool);
typedef void (*xconfig_func_int)(int);
typedef void (*xconfig_func_double)(double);
typedef void (*xconfig_func_string)(char *);
typedef void (*xconfig_func_null)(void);

struct xconfig_option {
	enum xconfig_option_type type;
	const char *name;
	union {
		void *object;
		xconfig_func_bool func_bool;
		xconfig_func_int func_int;
		xconfig_func_double func_double;
		xconfig_func_string func_string;
		xconfig_func_null func_null;
	} dest;
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
