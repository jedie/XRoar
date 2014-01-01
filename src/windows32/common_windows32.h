/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2014  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_COMMON_WINDOWS32_H_
#define XROAR_COMMON_WINDOWS32_H_

#include <windows.h>

extern HWND windows32_main_hwnd;

int windows32_init(void);
void windows32_shutdown(void);

#endif  /* XROAR_COMMON_WINDOWS32_H_ */
