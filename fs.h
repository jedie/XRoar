/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2004  Ciaran Anscomb
 *
 *  See COPYING for redistribution conditions. */

#ifndef __FS_H__
#define __FS_H__

#include "types.h"

/* Some file handling abstractions.  Arch-specific stuff is declared in
 * fs_*.h */

#ifdef HAVE_GP32
# include "fs_gp32.h"
#else
# include "fs_unix.h"
#endif

#define FS_READ 0x01
#define FS_WRITE 0x02

void fs_init(void);

int fs_chdir(char *path);
FS_FILE fs_open(char *filename, int flags);
ssize_t fs_read(FS_FILE fd, void *buffer, size_t size);
ssize_t fs_write(FS_FILE fd, void *buffer, size_t size);
void fs_close(FS_FILE fd);

ssize_t fs_load_file(char *filename, void *buf, size_t size);
int fs_write_byte(FS_FILE fd, uint8_t octet);
int fs_write_word16(FS_FILE fd, uint16_t word16);
int fs_write_word32(FS_FILE fd, uint32_t word32);
int fs_read_byte(FS_FILE fd, uint8_t *dest);
int fs_read_word16(FS_FILE fd, uint16_t *dest);
int fs_read_word32(FS_FILE fd, uint32_t *dest);

#endif  /* __FS_H__ */
