#include "task_monitor.h"

#include <stdio.h>

#include "app_main.h"
#include "bsp_uart.h"

void MonitorTask(void *argument)
{
    (void)argument;
    uint32_t last[TASK_COUNT] = { 0 };
    char line[48];

    for (;;)
    {
        App_TaskHeartbeat(TASK_MONITOR);
        vTaskDelay(pdMS_TO_TICKS(1000));

        /* Software watchdog: a task that fails to bump its heartbeat
         * within one monitor period is considered stalled. Day 5 adds
         * uxTaskGetStackHighWaterMark() reporting and recovery policy.
         */
        for (int i = 0; i < TASK_COUNT; ++i)
        {
            if (i == TASK_MONITOR)
            {
                continue;
            }

            if (last[i] == gHeartbeat[i])
            {
                App_SetFault(SYS_TASK_STALL);
                snprintf(line, sizeof(line),
                         "[monitor] task %d stalled\r\n", i);
                BSP_UART1_Print(line);
            }

            last[i] = gHeartbeat[i];
        }
    }
}
