#include "task_process.h"

#include "app_main.h"

void ProcessTask(void *argument)
{
    (void)argument;
    SensorData_t input;
    ProcessedData_t output = { { 0 }, 0.0f, 0.0f, 0 };

    for (;;)
    {
        /* Bounded wait, not portMAX_DELAY: a task blocked forever on an
         * empty queue would look "stalled" to MonitorTask. Heartbeat runs
         * on every loop pass whether or not data arrived.
         */
        if (xQueueReceive(gSensorQueue, &input, pdMS_TO_TICKS(100)) == pdPASS)
        {
            output.raw = input;

            /* first-order IIR: y = 0.8*y + 0.2*x */
            output.filtered_1 = 0.8f * output.filtered_1 + 0.2f * input.sensor_1;
            output.filtered_2 = 0.8f * output.filtered_2 + 0.2f * input.sensor_2;

            /* threshold lives in shared config: read it under the mutex */
            xSemaphoreTake(gConfigMutex, portMAX_DELAY);
            float threshold = gSystemConfig.alarm_threshold;
            xSemaphoreGive(gConfigMutex);

            output.alarm_flags = 0;
            if (output.filtered_1 > threshold)
            {
                output.alarm_flags |= 0x01;
            }

            /* display/comm queues are best-effort mirrors: if the consumer
             * is behind we drop the newest sample rather than block.
             */
            xQueueSend(gDisplayQueue, &output, 0);
            xQueueSend(gCommQueue, &output, 0);
        }

        App_TaskHeartbeat(TASK_PROCESS);
    }
}
