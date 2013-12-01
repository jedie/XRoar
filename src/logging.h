/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_LOGGING_H_
#define XROAR_LOGGING_H_

#include <stdint.h>

#include "config.h"

#ifndef LOGGING

#define LOG_DEBUG(...) do {} while (0)
#define LOG_WARN(...) do {} while (0)
#define LOG_ERROR(...) do {} while (0)

#else

#include <stdio.h>

/* Log levels:
 * 0 - Quiet, 1 - Info, 2 - Events, 3 - Debug */

#define LOG_DEBUG(l,...) do { if (log_level >= l) { fprintf(stderr, __VA_ARGS__); } } while (0)
#define LOG_PRINT(...) printf(__VA_ARGS__)
#define LOG_WARN(...) fprintf(stderr, "WARNING: " __VA_ARGS__)
#define LOG_ERROR(...) fprintf(stderr, "ERROR: " __VA_ARGS__)

#endif

extern int log_level;

struct log_handle;

// close any open log
void log_close(struct log_handle **);

// hexdumps - pretty print blocks of data
void log_open_hexdump(struct log_handle **, const char *prefix);
void log_hexdump_set_addr(struct log_handle *, unsigned addr);
void log_hexdump_line(struct log_handle *);
void log_hexdump_byte(struct log_handle *, uint8_t b);
void log_hexdump_flag(struct log_handle *l);

#endif  /* XROAR_LOGGING_H_ */
