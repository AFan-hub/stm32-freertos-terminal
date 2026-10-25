#include "cli.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_main.h"
#include "bsp_flash.h"
#include "bsp_uart.h"
#include "main.h"

static void cli_print(const char *s)
{
    BSP_UART1_AsyncPrint(s);
}

static void cli_help(void)
{
    cli_print("help | status | task | config | set period <ms> | "
              "set threshold <f> | save | reboot\r\n");
}

static void cli_status(void)
{
    char buf[128];

    snprintf(buf, sizeof(buf),
             "uptime: %lu s\r\nsensor queue: %lu/16\r\n"
             "uart tx buffer: %u/%u\r\ntx drops: %lu\r\nfault: 0x%02lX\r\n",
             (unsigned long)(xTaskGetTickCount() / 1000),
             (unsigned long)uxQueueMessagesWaiting(gSensorQueue),
             (unsigned)BSP_UART1_TxUsed(),
             (unsigned)(BSP_UART1_TxUsed() + BSP_UART1_TxFree()),
             (unsigned long)BSP_UART_TxDrops(),
             (unsigned long)gFaultFlags);
    cli_print(buf);
}

static void cli_task(void)
{
    char buf[256];

    for (int i = 0; i < TASK_COUNT; ++i)
    {
        TaskHandle_t h = App_TaskHandle((TaskId_t)i);

        if (h == NULL)
        {
            continue;
        }

        snprintf(buf, sizeof(buf), "%-8s hb=%-6lu hw=%-4lu\r\n",
                 pcTaskGetName(h),
                 (unsigned long)gHeartbeat[i],
                 (unsigned long)uxTaskGetStackHighWaterMark(h));
        cli_print(buf);
    }

    /* FreeRTOS run-time stats (TIM5 1 MHz free-running counter) */
    static char stats[512];
    /* FreeRTOS 10.x single-argument API; buffer sized for 8 tasks */
    vTaskGetRunTimeStats(stats);
    cli_print(stats);
}

static void cli_config(void)
{
    char buf[128];

    xSemaphoreTake(gConfigMutex, portMAX_DELAY);
    snprintf(buf, sizeof(buf),
             "magic=0x%08lX version=%u period=%u ms threshold=%.2f "
             "baud=%lu\r\n",
             (unsigned long)gSystemConfig.magic,
             (unsigned)gSystemConfig.version,
             (unsigned)gSystemConfig.sample_period_ms,
             (double)gSystemConfig.alarm_threshold,
             (unsigned long)gSystemConfig.uart_baudrate);
    xSemaphoreGive(gConfigMutex);

    cli_print(buf);
}

static void cli_set(char *line)
{
    char *what = strtok(NULL, " \t");
    char *val  = strtok(NULL, " \t");
    char buf[64];

    if (what == NULL || val == NULL)
    {
        cli_print("usage: set period <ms> | set threshold <f>\r\n");
        return;
    }

    xSemaphoreTake(gConfigMutex, portMAX_DELAY);

    if (strcmp(what, "period") == 0)
    {
        long v = atol(val);
        if (v >= 10 && v <= 5000)
        {
            gSystemConfig.sample_period_ms = (uint16_t)v;
            snprintf(buf, sizeof(buf), "period=%ld ms\r\n", v);
        }
        else
        {
            snprintf(buf, sizeof(buf), "period out of range 10..5000\r\n");
        }
    }
    else if (strcmp(what, "threshold") == 0)
    {
        float v = strtof(val, NULL);
        gSystemConfig.alarm_threshold = v;
        snprintf(buf, sizeof(buf), "threshold=%.2f\r\n", (double)v);
    }
    else
    {
        snprintf(buf, sizeof(buf), "unknown key: %s\r\n", what);
    }

    xSemaphoreGive(gConfigMutex);
    cli_print(buf);
}

void CLI_ProcessLine(char *line)
{
    char *cmd = strtok(line, " \t");

    if (cmd == NULL)
    {
        return;
    }

    if (strcmp(cmd, "help") == 0)        cli_help();
    else if (strcmp(cmd, "status") == 0) cli_status();
    else if (strcmp(cmd, "task") == 0)   cli_task();
    else if (strcmp(cmd, "config") == 0) cli_config();
    else if (strcmp(cmd, "set") == 0)    cli_set(line);
    else if (strcmp(cmd, "save") == 0)
    {
        xSemaphoreTake(gConfigMutex, portMAX_DELAY);
        int rc = BSP_Flash_SaveConfig(&gSystemConfig);
        xSemaphoreGive(gConfigMutex);
        cli_print(rc == 0 ? "config saved to flash\r\n"
                          : "flash save FAILED\r\n");
    }
    else if (strcmp(cmd, "reboot") == 0)
    {
        cli_print("rebooting...\r\n");
        vTaskDelay(pdMS_TO_TICKS(50));   /* let the DMA drain the line */
        NVIC_SystemReset();
    }
    else
    {
        cli_print("unknown command, try: help\r\n");
    }
}
