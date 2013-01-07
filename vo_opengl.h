/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_VO_OPENGL_H_
#define XROAR_VO_OPENGL_H_

/* OpenGL code is common to several video modules.  All the stuff that's not
 * toolkit-specific goes in here. */

#include <inttypes.h>

int vo_opengl_init(void);
void vo_opengl_shutdown(void);
void vo_opengl_alloc_colours(void);
void vo_opengl_vsync(void);
void vo_opengl_set_window_size(unsigned w, unsigned h);
void vo_opengl_render_scanline(uint8_t *scanline_data);
void vo_opengl_update_cross_colour_phase(void);

#endif  /* XROAR_VO_OPENGL_H_ */
