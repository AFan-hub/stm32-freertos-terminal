#include "task_sensor.h"

#include "app_main.h"
#include "bsp_adc.h"

/* Channel plan after the MPU6050 module was declared faulty (Day-2 case):
 *   sensor_1 = photoresistor light level [%]  -> threshold/alarm channel
 *   sensor_2 = on-chip temperature [degC]     (ADC1_IN16, zero hardware)
 * The I2C bus is exercised by the SSD1306 secondary display instead.
 * MPU6050 driver stays in the tree for a future third channel.
 */
void SensorTask(void *argument)
{
    (void)argument;
    SensorData_t data = { 0 };

    for (;;)
    {
        App_TaskHeartbeat(TASK_SENSOR);

        data.sensor_1 = BSP_ADC_ReadLightPercent();
        data.sensor_2 = BSP_ADC_ReadTempC();
        data.status   = 0x03;

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
