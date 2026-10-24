#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define PROTOCOL_HEAD1       0xAA
#define PROTOCOL_HEAD2       0x55
#define PROTOCOL_VER         0x01
#define PROTOCOL_MAX_PAYLOAD 256

typedef enum
{
    CMD_SENSOR_DATA  = 0x01,
    CMD_SYSTEM_STATE = 0x02,
    CMD_SET_CONFIG   = 0x03,
    CMD_GET_CONFIG   = 0x04,
    CMD_ACK          = 0x05,
    CMD_HEARTBEAT    = 0x06,
    CMD_ERROR        = 0x07
} ProtocolCmd_t;

/* Wire size is exactly 14 bytes (packed, little-endian).
 * Must match protocol::SENSOR_PAYLOAD_SIZE on the Raspberry Pi side. */
typedef struct __attribute__((packed))
{
    uint32_t timestamp_ms;
    float sensor_1;
    float sensor_2;
    uint16_t status;
} SensorPayload_t;

/* Frame: AA 55 VER CMD LEN_L LEN_H PAYLOAD CRC_L CRC_H
 * CRC16-CCITT-FALSE over VER .. end of PAYLOAD.
 * Returns the encoded frame length, or 0 on invalid input. */
uint16_t Protocol_Encode(uint8_t cmd,
                         const uint8_t *payload,
                         uint16_t payload_len,
                         uint8_t *out,
                         uint16_t out_size);

uint16_t Protocol_EncodeSensor(const SensorPayload_t *data,
                               uint8_t *out,
                               uint16_t out_size);

#endif
