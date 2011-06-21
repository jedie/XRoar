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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * Changelog:
 *
 *  21 Aug 2004 - Mirko Roller <mirko@mirkoroller.de>
 *   first release 
 *  25 Oct 2004 - Ciaran Anscomb <xroar@6809.org.uk>
 *   adapted for use with Gamepark SDK - a temporary measure until I port
 *   XRoar to use Mirko's SDK
 *  5 Apr 2008 - Ciaran Anscomb
 *   update to use a state machine approximating mobile phone response
 */


/* Hardware project from: http://www.deadcoderssociety.tk  */
/* This file is written 2004 Mirko Roller */

#include <string.h>
#include "gp32.h"
#include "../types.h"
#include "gpchatboard.h"

#define BUFFER_QUEUE(c) do { \
		buffer[buffer_next] = c; \
		buffer_next = (buffer_next + 1) % sizeof(buffer); \
	} while (0)
#define BUFFER_HASQUEUE (buffer_next != buffer_cursor)
#define BUFFER_DEQUEUE(into) do { \
		into = buffer[buffer_cursor]; \
		buffer_cursor = (buffer_cursor + 1) % sizeof(buffer); \
	} while (0)
static volatile unsigned char buffer[8];
static volatile int buffer_next, buffer_cursor;

static const char *keypad_map[] = {
	"+&@/_%$[____0__\014\033",  /* 0 */
	" -?!,.:_\042'___()1",      /* 1 */
	"abc\176)?^?2",             /* 2 */
	"def=?3",                   /* 3 */
	"ghi?4",                    /* 4 */
	"jkl5",                     /* 5 */
	"mno(_(?6",                 /* 6 */
	"pqrs\0107<>",              /* 7 */
	"tuv\011\0128",             /* 8 */
	"wxyz9",                    /* 9 */
	"#*",                       /* # */
	"\010",                     /* < */
	"\011",                     /* > */
	"__\015",                   /* e */
	"\015",                     /* c */
};
static int button_select, button_index, button_char;

static const char *keypad_chars = "0123456789#<>ecs";

static char command_buf[10];
static int command_length = 0;

static void ChatBoardSendOKIRQ(void) {
	const char *s = "OK\015\012";
	while (*s) {
		while (rUFSTAT0 & (1 << 9));  /* Wait until Tx FIFO not full */
		rUTXH0 = *(s++);
	}
}

enum {
	WANT_CKPD, WANT_FIRST_BUTTON, CONTINUE_STRING, CHECK_COMMA, CONSUME_REST
};

void UART0Rx(void) __attribute__ ((interrupt ("IRQ")));
void UART0Rx(void) {
	char *tmp;
	static int state = WANT_CKPD;
	int num_chars;
	if (rUFSTAT0 & (1 << 8))
		num_chars = 16;
	else
		num_chars = rUFSTAT0 & 15;
	while (num_chars > 0) {
		char c = rURXH0;
		num_chars--;
		if (c == '\r') {
			/* \r always resets the state machine */
			if (button_select >= 0) {
				BUFFER_QUEUE(button_char);
			}
			button_select = -1;
			command_length = 0;
			state = WANT_CKPD;
			ChatBoardSendOKIRQ();
			return;
		}
		switch (state) {
			case WANT_CKPD:
				if (command_length < 9) {
					command_buf[command_length++] = c;
					if (command_length == 9) {
						command_buf[9] = 0;
						if (0 == strcmp(command_buf, "AT+CKPD=\"")) {
							state = WANT_FIRST_BUTTON;
						} else {
							state = CONSUME_REST;
						}
					}
				}
				break;
			case WANT_FIRST_BUTTON:
				tmp = strchr(keypad_chars, c);
				if (tmp == NULL) {
					button_select = -1;
					state = CONSUME_REST;
					break;
				}
				button_select = tmp - keypad_chars;
				button_index = 0;
				button_char = keypad_map[button_select][0];
				state = CONTINUE_STRING;
				break;
			case CONTINUE_STRING:
				if (c == '\"') {
					state = CHECK_COMMA;
					break;
				}
				if (c == 'c') {
					state = WANT_FIRST_BUTTON;
					break;
				}
				tmp = strchr(keypad_chars, c);
				if (tmp == NULL || button_select != (tmp - keypad_chars)) {
					button_select = -1;
					state = CONSUME_REST;
					break;
				}
				button_index++;
				if (keypad_map[button_select][button_index] == 0)
					button_index = 0;
				button_char = keypad_map[button_select][button_index];
				break;
			case CHECK_COMMA:
				if (c == ',' && button_select <= 9) {
					button_char = button_select + '0';
				} else {
					button_select = -1;
				}
				state = CONSUME_REST;
				break;
			default:
			case CONSUME_REST:
				/* Eat characters until the test for '\r' resets state machine */
				break;
		}
	}
}

/* Not used yet.... */
void UART0Tx(void) __attribute__ ((interrupt ("IRQ")));
void UART0Tx(void) {
}


int gpchatboard_init(void) {
	int SPEED = 9600;

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
		  (3<<4) |	/* Rx trigger level = 16 bytes (3->16) */
		  (1<<2) |	/* Reset Tx FIFO */
		  (1<<1) |	/* Reset Rx FIFO */
		  (1<<0);	/* Enable FIFO mode */

	rUBRDIV0 = (PCLK / (SPEED * 16)) - 1;

	buffer_next = 0;
	buffer_cursor = 0;
	button_select = -1;
	button_index = 0;
	command_length = 0;

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

unsigned int gpchatboard_scan(void) {
	unsigned char c;
	if (BUFFER_HASQUEUE) {
		BUFFER_DEQUEUE(c);
		return c;
	}
	return 0;
}
