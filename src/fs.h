/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_FS_H_
#define XROAR_FS_H_

#include <stdio.h>

/* Some file handling convenience functions */

int fs_write_uint8(FILE *stream, int value);
int fs_write_uint16(FILE *stream, int value);
int fs_write_uint16_le(FILE *stream, int value);
int fs_read_uint8(FILE *stream);
int fs_read_uint16(FILE *stream);
int fs_read_uint16_le(FILE *stream);

#endif  /* XROAR_FS_H_ */
