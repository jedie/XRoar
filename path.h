/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2010  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef __PATH_H__
#define __PATH_H__

/* Try to find regular file within one of the directories supplied.  Returns
 * allocated memory containing full path to file if found, NULL otherwise.
 * Directory separator occuring within filename just causes that one file to be
 * checked. */

char *find_in_path(const char *path, const char *filename);

#endif  /* __PATH_H__ */
