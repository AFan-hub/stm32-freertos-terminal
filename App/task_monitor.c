#include "task_monitor.h"

#include <stdio.h>

#include "app_main.h"
#include "bsp_uart.h"

void MonitorTask(void *argument)
{
    (void)argument;
    uint32_t last[TASK_COUNT] = { 0 };
    uint8_t miss[TASK_COUNT] = { 0 };
    uint32_t seconds = 0;
    char line[48];

    /* Boot grace: the CH340 one-key-download circuit resets the MCU 2-3
     * times on first power-up (board manual), and tasks need a moment to
     * reach their first heartbeat. Snapshot after 2 s instead of at zero.
     */
    App_TaskHeartbeat(TASK_MONITOR);
    vTaskDelay(pdMS_TO_TICKS(2000));
    for (int i = 0; i < TASK_COUNT; ++i)
    {
        last[i] = gHeartbeat[i];
    }

    for (;;)
    {
        App_TaskHeartbeat(TASK_MONITOR);
        vTaskDelay(pdMS_TO_TICKS(1000));

        /* every 10 s: stack high-water marks (manual §14) */
        if ((++seconds % 10) == 0)
        {
            for (int i = 0; i < TASK_COUNT; ++i)
            {
                TaskHandle_t h = App_TaskHandle((TaskId_t)i);

                if (h == NULL)
                {
                    continue;
                }

                snprintf(line, sizeof(line), "[mon] %-8s hw=%lu words\r\n",
                         pcTaskGetName(h),
                         (unsigned long)uxTaskGetStackHighWaterMark(h));
                BSP_UART1_AsyncPrint(line);
            }
        }

        /* Software watchdog: a task must bump its heartbeat every period.
         * Two consecutive misses before declaring a stall, so a single
         * delayed cycle (power dip, long critical section) never latches
         * a fault.
         */
        for (int i = 0; i < TASK_COUNT; ++i)
        {
            if (i == TASK_MONITOR)
            {
                continue;
            }

            if (last[i] == gHeartbeat[i])
            {
                if (++miss[i] >= 2)
                {
                    App_SetFault(SYS_TASK_STALL);
                    snprintf(line, sizeof(line),
                             "[monitor] task %d stalled\r\n", i);
                    BSP_UART1_Print(line);
                    BSP_UART2_Print(line);
                    miss[i] = 0;
                }
            }
            else
            {
                miss[i] = 0;
            }

            last[i] = gHeartbeat[i];
        }
    }
}
