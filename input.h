/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

/* An abstraction layer allowing buttons or keys to be easily mapped to certain
 * emulator actions. */

#ifndef XROAR_INPUT_H_
#define XROAR_INPUT_H_

/* Set axis commands */
#define INPUT_JOY_RIGHT_X (0)
#define INPUT_JOY_RIGHT_Y (1)
#define INPUT_JOY_LEFT_X  (2)
#define INPUT_JOY_LEFT_Y  (3)
/* Firebutton commands */
#define INPUT_JOY_RIGHT_FIRE (4)
#define INPUT_JOY_LEFT_FIRE  (6)
/* Keypress commands */
#define INPUT_KEY         (8)
#define INPUT_UNICODE_KEY (9)
/* Input config commands */
#define INPUT_SWAP_JOYSTICKS (10)

extern unsigned int input_firebutton_mask;

void input_control_press(int command, unsigned int arg);
void input_control_release(int command, unsigned int arg);

#endif  /* XROAR_INPUT_H_ */
