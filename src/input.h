/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

/* An abstraction layer allowing buttons or keys to be easily mapped to certain
 * emulator actions. */

#ifndef XROAR_INPUT_H_
#define XROAR_INPUT_H_

/* Keypress commands */
#define INPUT_KEY         (8)
#define INPUT_UNICODE_KEY (9)

void input_control_press(int command, unsigned int arg);
void input_control_release(int command, unsigned int arg);

#endif  /* XROAR_INPUT_H_ */
