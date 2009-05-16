/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2008  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef __FS_H__
#define __FS_H__

#include "types.h"

/* Some file handling abstractions. */

#define FS_READ 0x01
#define FS_WRITE 0x02

void fs_init(void);

int fs_chdir(const char *path);
int fs_open(const char *filename, int flags);
ssize_t fs_read(int fd, void *buffer, size_t size);
ssize_t fs_write(int fd, const void *buffer, size_t size);
void fs_close(int fd);
ssize_t fs_size(const char *filename);
char *fs_getcwd(char *buf, size_t size);

ssize_t fs_load_file(char *filename, void *buf, size_t size);
int fs_write_uint8(int fd, int value);
int fs_write_uint16(int fd, int value);
int fs_write_uint16_le(int fd, int value);
int fs_read_uint8(int fd);
int fs_read_uint16(int fd);
int fs_read_uint16_le(int fd);

#endif  /* __FS_H__ */
