#include "crc16.h"

uint16_t CRC16_CCITT(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;

    for (uint16_t i = 0; i < len; ++i)
    {
        crc ^= (uint16_t)data[i] << 8;

        for (int bit = 0; bit < 8; ++bit)
        {
            if (crc & 0x8000)
            {
                crc = (uint16_t)((crc << 1) ^ 0x1021);
            }
            else
            {
                crc = (uint16_t)(crc << 1);
            }
        }
    }

    return crc;
}
