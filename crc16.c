/* CRC-16-CCITT with bytes processed high bit first ("big-endian"), as used in
 * the WD279X FDC (polynomial 0x1021).  In the FDC, CRC is initialised to
 * 0xffff and NOT inverted before appending to the message. */

/* This implementation uses some clever observations about which bits of the
 * message and old CRC affect each other.  Credits:
 * Ashley Roll   - www.digitalnemesis.com
 * Scott Dattalo - www.dattalo.com
 */

#include <inttypes.h>

#include "crc16.h"

uint16_t crc16_byte(uint16_t crc, uint8_t value) {
	uint16_t x = (crc >> 8) ^ value;
	x ^= (x >> 4);
	return (crc << 8) ^ (x << 12) ^ (x << 5) ^ x;
}

uint16_t crc16_block(uint16_t crc, uint8_t *block, unsigned int length) {
	for (; length; length--)
		crc = crc16_byte(crc, *(block++));
	return crc;
}
