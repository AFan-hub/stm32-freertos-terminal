#include "task_sensor.h"

#include "app_main.h"

void SensorTask(void *argument)
{
    (void)argument;

    for (;;)
    {
        App_TaskHeartbeat(TASK_SENSOR);

        /* Day 2: read MPU6050 (I2C1) + photoresistor (ADC1_IN1),
         * fill SensorData_t and xQueueSend(gSensorQueue, ..., 0);
         * count drops into SYS_QUEUE_OVERFLOW when the queue is full.
         * Reading sample_period_ms without the mutex is fine for now:
         * it only changes from the CLI under ConfigMutex on Day 6.
         */
        vTaskDelay(pdMS_TO_TICKS(gSystemConfig.sample_period_ms));
    }
}
