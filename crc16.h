/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2007  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef __CRC16_H__
#define __CRC16_H__

#include "types.h"

void crc16_reset(void);
uint16_t crc16_value(void);
void crc16_byte(uint8_t value);
void crc16_block(uint8_t *block, int length);

#endif  /* __CRC16_H__ */
