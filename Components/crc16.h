#ifndef CRC16_H
#define CRC16_H

#include <stdint.h>

/* CRC16-CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection, no xorout.
 * Check value: CRC16_CCITT("123456789", 9) == 0x29B1
 * MUST stay byte-identical to include/Crc16.hpp on the Raspberry Pi side. */
uint16_t CRC16_CCITT(const uint8_t *data, uint16_t len);

#endif
