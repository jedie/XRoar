/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_PATH_H_
#define XROAR_PATH_H_

/* Try to find regular file within one of the directories supplied.  Returns
 * allocated memory containing full path to file if found, NULL otherwise.
 * Directory separator occuring within filename just causes that one file to be
 * checked. */

char *find_in_path(const char *path, const char *filename);

#endif  /* XROAR_PATH_H_ */
