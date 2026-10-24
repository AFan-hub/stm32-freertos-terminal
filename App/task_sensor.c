#include "task_sensor.h"

#include <stdio.h>

#include "app_main.h"
#include "bsp_adc.h"
#include "bsp_mpu6050.h"
#include "bsp_uart.h"

void SensorTask(void *argument)
{
    (void)argument;
    SensorData_t data = { 0 };

    /* sensor_1 = MPU6050 die temperature [°C]  (status bit 0 = MPU healthy)
     * sensor_2 = photoresistor voltage  [%]    (status bit 1 = ADC read ok)
     */
    int mpu_ok;

    BSP_I2C1_BridgeTest();          /* pin bridge/short matrix */
    BSP_I2C1_ScanReport();          /* wiring diagnostic, once at boot */
    BSP_I2C1_ScanReportAlt();       /* off-by-one-row detector (P9 row 1) */
    mpu_ok = BSP_MPU6050_Init();

    {
        /* -1 WHO_AM_I, -2 reset write, -3 wake write, -4 config writes,
         * 0 = ok. Reads failing later show up as fault bit1 instead. */
        char mline[40];
        int nn = snprintf(mline, sizeof(mline), "[mpu] init=%d\r\n", mpu_ok);
        BSP_UART1_SendBuffer((const uint8_t *)mline, (uint16_t)nn);
        BSP_UART2_SendBuffer((const uint8_t *)mline, (uint16_t)nn);
    }

    if (mpu_ok != 0)
    {
        App_SetFault(SYS_SENSOR_TIMEOUT);
    }

    for (;;)
    {
        App_TaskHeartbeat(TASK_SENSOR);

        float ax, ay, az, temp_c;

        if (mpu_ok == 0 && BSP_MPU6050_Read(&ax, &ay, &az, &temp_c) == 0)
        {
            data.sensor_1 = temp_c;
            data.status |= 0x01;
            (void)ax; (void)ay; (void)az;   /* Day 18: use accel for stats */
        }
        else
        {
            data.status &= (uint16_t)~0x01u;
            App_SetFault(SYS_SENSOR_TIMEOUT);
        }

        data.sensor_2 = BSP_ADC_ReadLightPercent();
        data.status |= 0x02;

        data.timestamp_ms = xTaskGetTickCount();   /* 1 kHz tick = ms */

        /* Non-blocking send: a full queue means the consumer is behind;
         * we drop the sample and raise SYS_QUEUE_OVERFLOW instead of
         * blocking the acquisition cycle.
         */
        if (xQueueSend(gSensorQueue, &data, 0) != pdPASS)
        {
            App_SetFault(SYS_QUEUE_OVERFLOW);
        }

        vTaskDelay(pdMS_TO_TICKS(gSystemConfig.sample_period_ms));
    }
}
