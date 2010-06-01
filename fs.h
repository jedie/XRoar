/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2010  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_FS_H_
#define XROAR_FS_H_

#include "types.h"

/* Some file handling abstractions. */

#define FS_READ 0x01
#define FS_WRITE 0x02

#define FS_SEEK_SET (0)
#define FS_SEEK_CUR (1)
#define FS_SEEK_END (2)

void fs_init(void);

int fs_chdir(const char *path);
int fs_open(const char *filename, int flags);
ssize_t fs_read(int fd, void *buffer, size_t size);
ssize_t fs_write(int fd, const void *buffer, size_t size);
off_t fs_lseek(int fd, off_t offset, int whence);
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

#endif  /* XROAR_FS_H_ */
