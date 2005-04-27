/*
 * Chatboard driver - gp_chatboard.c
 *
 * Copyright (C) 2003,2004 Mirko Roller <mirko@mirkoroller.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Changelog:
 *
 *  21 Aug 2004 - Mirko Roller <mirko@mirkoroller.de>
 *   first release 
 *  25 Oct 2004 - Ciaran Anscomb <xroar@6809.org.uk>
 *   adapted for use with Gamepark SDK - a temporary measure until I port
 *   XRoar to use Mirko's SDK
 */


// Hardware project from: http://www.deadcoderssociety.tk 
// This file is written 2004 Mirko Roller

#include <string.h>
#include "gp32.h"
#include "keydefines.h"
#include "../types.h"
#include "gpchatboard.h"


#define CHA_KEY_NUMBER (sizeof(cha01KeyMap) / sizeof(struct threeMap))
// 13 = CR
// 10 = LF

static volatile unsigned char chatboard_buffer[256];
static volatile unsigned int  chatboard_buffer_pos=0;
static volatile unsigned char chatboard_tx[32];
static volatile int seq_ok;

static void ChatBoardSendOKIRQ(void) {
	const char *s = "OK\015\012";
	while (*s) {
		while (rUFSTAT0 & 0x100);
		rUTXH0 = *(s++);
	}
}

void UART0Rx(void) __attribute__ ((interrupt ("IRQ")));
void UART0Rx(void) {
	uint8_t chars, c, i;
	if (rUFSTAT0 & 0x100) chars = 16;
	else chars = rUFSTAT0 & 0x0F;   //  Number of data in rx fifo
	for (i = 0; i < chars; i++) {
		c = rURXH0;
		if (chatboard_buffer_pos < 255)
			chatboard_buffer[chatboard_buffer_pos++] = c;
		if (c == 13) {
			chatboard_buffer[chatboard_buffer_pos] = 0;
			seq_ok = 1;
			chatboard_buffer_pos = 0;
			ChatBoardSendOKIRQ();
		}
	}
}

// Not used yet....
void UART0Tx(void) __attribute__ ((interrupt ("IRQ")));
void UART0Tx(void) {
     /* volatile uint8_t i; */
}


int gpchatboard_init(void) {
	int SPEED = 9600;

	seq_ok = 0;
	chatboard_buffer_pos=0;
	   
	rCLKCON |= 0x100;	/* Controls PCLK into UART0 block */
	/* Set up UART line control register: */
	rULCON0 = (0<<6) |	/* Normal operation (1=IRDA mode) */
		  (0<<3) |	/* No parity (4=odd,5=even) */
		  (0<<2) |	/* 1 stop bit (1=2 stop bits) */
		  (3<<0);	/* 8 data bits (0=5, 1=6, 2=7) */
	/* Set up UART control register: */
	rUCON0  = (0<<9) |	/* IRQ as soon as Tx buffer empty */
		  (0<<8) |	/* IRQ on Rx trigger level */
		  (1<<7) |	/* Enable Rx timeout interrupt */
		  (0<<6) |	/* No error status interrupt */
		  (0<<5) |	/* No loopback mode */
		  (0<<4) |	/* Don't send a break signal */
		  (1<<2) |	/* Tx IRQ/polling mode */
		  (1<<0);	/* Rx IRQ/polling mode */
	/* Set up FIFO control register: */
	rUFCON0 = (0<<6) |	/* Tx trigger level = 0 bytes */
		  (2<<4) |	/* Rx trigger level = 12 bytes (3->16) */
		  (1<<2) |	/* Reset Tx FIFO */
		  (1<<1) |	/* Reset Rx FIFO */
		  (1<<0);	/* Enable FIFO mode */

	rUBRDIV0 = (PCLK / (SPEED * 16)) - 1;

	ARMDisableInterrupt();
	swi_install_irq(23, UART0Rx);
	swi_install_irq(28, UART0Tx);
	ARMEnableInterrupt();

	return 0;
}

void gpchatboard_shutdown(void) {
	ARMDisableInterrupt();
	swi_uninstall_irq(23, UART0Rx);
	swi_uninstall_irq(28, UART0Tx);
	ARMEnableInterrupt();
}

unsigned char gpchatboard_scan(void) {
	unsigned int x = 0, mark;
	unsigned char search[256];
	unsigned char seq[] = "AT+CKPD=\"";

	if (seq_ok) {
		seq_ok = 0;
		/* Search for seq in chatboard_buffer[] */
		for (x = 0; x < 256; x++)
			search[x]=0;
		for (x = 0; x < 250; x++) {
			search[x] = chatboard_buffer[x+9];
			if (search[x]==13) break;
		}
		if (!strncmp(seq,search,sizeof(seq))) return 0;

		mark = x;
		for (x = 0; x < CHA_KEY_NUMBER; x++) {
			if (!strncmp(cha01KeyMap[x].map1,search,mark+1))
				return (cha01KeyMap[x].key);
			if (!strncmp(cha01KeyMap[x].map2,search,mark+1))
				return (cha01KeyMap[x].key);
		}
	}
	return 0;
}

