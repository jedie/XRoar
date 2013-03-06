/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_SDL_COMMON_H_
#define XROAR_SDL_COMMON_H_

#include "module.h"

extern VideoModule video_sdlgl_module;
extern VideoModule video_sdlyuv_module;
extern VideoModule video_sdl_module;
extern VideoModule video_null_module;
extern KeyboardModule keyboard_sdl_module;

extern struct joystick_interface sdl_js_if_physical;
extern struct joystick_interface sdl_js_if_keyboard;
extern JoystickModule sdl_js_internal;

extern VideoModule *sdl_video_module_list[];
extern KeyboardModule *sdl_keyboard_module_list[];
extern JoystickModule *sdl_js_modlist[];

void sdl_run(void);
void sdl_keypress(SDL_keysym *keysym);
void sdl_keyrelease(SDL_keysym *keysym);
void sdl_js_physical_shutdown(void);

#endif  /* XROAR_SDL_COMMON_H_ */
