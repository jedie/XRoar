/*
 * Chatboard driver - gp_chatboard.c
 *
 * Copyright (C) 2003,2004 Mirko Roller <mirko@mirkoroller.de>
 *
 * Changelog:
 *
 *  21 Aug 2004 - Mirko Roller <mirko@mirkoroller.de>
 *   first release 
 *  25 Oct 2004 - Ciaran Anscomb <xroar@6809.org.uk>
 *   adapted for use with Gamepark SDK - a temporary measure until I port
 *   XRoar to use Mirko's SDK
 */

// later...  volatile int gpchatboard_present;

#ifndef XROAR_GP32_GPCHATBOARD_H_
#define XROAR_GP32_GPCHATBOARD_H_

int gpchatboard_init(void);
void gpchatboard_shutdown(void);
unsigned int gpchatboard_scan(void);

#endif  /* XROAR_GP32_GPCHATBOARD_H_ */
