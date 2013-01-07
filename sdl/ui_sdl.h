
/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_SDL_UI_SDL_H_
#define XROAR_SDL_UI_SDL_H_

#include "module.h"

extern VideoModule video_sdlgl_module;
extern VideoModule video_sdlyuv_module;
extern VideoModule video_sdl_module;
extern VideoModule video_null_module;
extern KeyboardModule keyboard_sdl_module;

void sdl_run(void);
void sdl_keypress(SDL_keysym *keysym);
void sdl_keyrelease(SDL_keysym *keysym);

#endif  /* XROAR_SDL_UI_SDL_H_ */
