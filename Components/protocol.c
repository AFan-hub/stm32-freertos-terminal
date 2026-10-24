#include "protocol.h"

#include <string.h>

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "protocol v1 assumes a little-endian target"
#endif

#include "crc16.h"

uint16_t Protocol_Encode(uint8_t cmd,
                         const uint8_t *payload,
                         uint16_t payload_len,
                         uint8_t *out,
                         uint16_t out_size)
{
    const uint16_t total = (uint16_t)(6u + payload_len + 2u);

    if (payload_len > PROTOCOL_MAX_PAYLOAD || out_size < total)
    {
        return 0;
    }

    out[0] = PROTOCOL_HEAD1;
    out[1] = PROTOCOL_HEAD2;
    out[2] = PROTOCOL_VER;
    out[3] = cmd;
    out[4] = (uint8_t)(payload_len & 0xFF);
    out[5] = (uint8_t)(payload_len >> 8);

    if (payload_len > 0)
    {
        memcpy(out + 6, payload, payload_len);
    }

    const uint16_t crc = CRC16_CCITT(out + 2, (uint16_t)(4u + payload_len));
    out[total - 2] = (uint8_t)(crc & 0xFF);
    out[total - 1] = (uint8_t)(crc >> 8);

    return total;
}

uint16_t Protocol_EncodeSensor(const SensorPayload_t *data,
                               uint8_t *out,
                               uint16_t out_size)
{
    /* SensorPayload_t is packed and the target is little-endian, so the
     * struct image IS the wire payload. Verified by the Pi-side unit test
     * against the same check vectors. */
    return Protocol_Encode(CMD_SENSOR_DATA,
                           (const uint8_t *)data,
                           (uint16_t)sizeof(SensorPayload_t),
                           out,
                           out_size);
}
