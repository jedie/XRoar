/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2006  Ciaran Anscomb
 *
 *  See COPYING for redistribution conditions. */

#ifndef __FILEREQ_H__
#define __FILEREQ_H__

typedef struct FileReqModule FileReqModule;
struct FileReqModule {
	int (*init)(void);
	void (*shutdown)(void);
	char *(*load_filename)(const char **extensions);
	char *(*save_filename)(const char **extensions);
};

FileReqModule *filereq_init(void);

#endif  /* __FILEREQ_H__ */
