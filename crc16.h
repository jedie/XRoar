/* CRC-16-CCITT with bytes processed high bit first ("big-endian"), as used in
 * the WD279X FDC (polynomial 0x1021).  In the FDC, CRC is initialised to
 * 0xffff and NOT inverted before appending to the message. */

#ifndef XROAR_CRC16_H_
#define XROAR_CRC16_H_

#include <stdint.h>

#define CRC16_RESET (0xffff)

uint16_t crc16_byte(uint16_t crc, uint8_t value);
uint16_t crc16_block(uint16_t crc, uint8_t *block, unsigned int length);

#endif  /* XROAR_CRC16_H_ */
