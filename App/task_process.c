#include "task_process.h"

#include "app_main.h"

void ProcessTask(void *argument)
{
    (void)argument;
    SensorData_t input;

    for (;;)
    {
        /* Bounded wait, not portMAX_DELAY: a task blocked forever on an
         * empty queue would look "stalled" to MonitorTask. Heartbeat runs
         * on every loop pass whether or not data arrived.
         */
        if (xQueueReceive(gSensorQueue, &input, pdMS_TO_TICKS(100)) == pdPASS)
        {
            (void)input;

            /* Day 2: filtering, threshold check and forwarding
             * to gDisplayQueue / gCommQueue. */
        }

        App_TaskHeartbeat(TASK_PROCESS);
    }
}
