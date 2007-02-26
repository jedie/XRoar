/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2007  Ciaran Anscomb
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
int fs_write_byte(int fd, uint8_t octet);
int fs_write_word16(int fd, uint16_t word16);
int fs_write_word32(int fd, uint32_t word32);
int fs_read_byte(int fd, uint8_t *dest);
int fs_read_word16(int fd, uint16_t *dest);
int fs_read_word32(int fd, uint32_t *dest);

#endif  /* __FS_H__ */
