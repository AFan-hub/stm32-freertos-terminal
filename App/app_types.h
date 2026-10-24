#ifndef APP_TYPES_H
#define APP_TYPES_H

#include <stdint.h>

typedef struct
{
    uint32_t timestamp_ms;
    float sensor_1;
    float sensor_2;
    uint16_t status;
} SensorData_t;

typedef struct
{
    SensorData_t raw;
    float filtered_1;
    float filtered_2;
    uint32_t alarm_flags;
} ProcessedData_t;

typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t sample_period_ms;
    float alarm_threshold;
    uint32_t uart_baudrate;
    uint16_t crc;
} SystemConfig_t;

#endif
