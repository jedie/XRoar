/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2012  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_CRC16_H_
#define XROAR_CRC16_H_

#include "types.h"

#define CRC16_RESET (0xffff)

uint16_t crc16_byte(uint16_t crc, uint8_t value);
uint16_t crc16_block(uint16_t crc, uint8_t *block, int length);

#endif  /* XROAR_CRC16_H_ */
