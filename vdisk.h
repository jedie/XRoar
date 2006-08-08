/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2006  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef __VDISK_H__
#define __VDISK_H__

int vdisk_load(const char *filename, unsigned int drive);
int vdisk_load_vdk(const char *filename, unsigned int drive);
int vdisk_load_jvc(const char *filename, unsigned int drive);

#endif  /* __VDISK_H__ */
