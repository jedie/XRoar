/* CRC-32 implementation.  If zlib is available, this just wraps its crc32()
 * function, otherwise a table-based implementation is built. */

#ifndef XROAR_CRC32_H_
#define XROAR_CRC32_H_

#include "types.h"

#define CRC32_RESET (0)

uint32_t crc32_block(uint32_t crc, uint8_t *block, unsigned int length);

#endif  /* XROAR_CRC32_H_ */
