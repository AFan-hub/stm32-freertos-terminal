#include "app_main.h"

#include <string.h>

#include "bsp_delay.h"
#include "bsp_uart.h"
#include "lcd.h"
#include "task_comm.h"
#include "task_display.h"
#include "task_monitor.h"
#include "task_process.h"
#include "task_sensor.h"

QueueHandle_t     gSensorQueue;
QueueHandle_t     gDisplayQueue;
QueueHandle_t     gCommQueue;
SemaphoreHandle_t gConfigMutex;
SystemConfig_t    gSystemConfig;

volatile uint32_t gHeartbeat[TASK_COUNT];
volatile uint32_t gFaultFlags;

static const SystemConfig_t sDefaultConfig =
{
    .magic            = 0xA55AA55A,
    .version          = 1,
    .sample_period_ms = 100,
    .alarm_threshold  = 30.0f,
    .uart_baudrate    = 115200,
    .crc              = 0
};

/* words; CommTask needs headroom for snprintf */
#define STACK_SENSOR  256
#define STACK_PROCESS 384
#define STACK_COMM    512
#define STACK_DISPLAY 384
#define STACK_MONITOR 384

/* priorities: high tasks stay short (see docs/cubemx_setup.md) */
#define PRIO_SENSOR   4
#define PRIO_PROCESS  3
#define PRIO_COMM     2
#define PRIO_DISPLAY  1
#define PRIO_MONITOR  4

static TaskHandle_t sTaskHandles[TASK_COUNT];

void App_TaskHeartbeat(TaskId_t id)
{
    gHeartbeat[id]++;
}

void App_SetFault(SystemFault_t fault)
{
    gFaultFlags |= (1u << (uint32_t)fault);
}

void App_Main_Init(void)
{
    /* Day 6 replaces this with Flash load + magic/version/CRC check. */
    gSystemConfig = sDefaultConfig;
    gFaultFlags = 0;
    memset((void *)gHeartbeat, 0, sizeof(gHeartbeat));

    gSensorQueue = xQueueCreate(16, sizeof(SensorData_t));
    gDisplayQueue = xQueueCreate(8, sizeof(ProcessedData_t));
    gCommQueue = xQueueCreate(8, sizeof(ProcessedData_t));
    gConfigMutex = xSemaphoreCreateMutex();

    configASSERT(gSensorQueue != NULL);
    configASSERT(gDisplayQueue != NULL);
    configASSERT(gCommQueue != NULL);
    configASSERT(gConfigMutex != NULL);

    /* 4.3" TFT over FSMC (Alientek driver). Safe before the scheduler:
     * its delays are DWT-based (bsp_delay), not HAL tick based. */
    bsp_delay_init();
    lcd_init();
    lcd_display_dir(0);   /* panel is mounted portrait on the Explorer */
    LCD_BL(1);
    g_back_color = BLACK;
    lcd_clear(BLACK);

    configASSERT(xTaskCreate(SensorTask,  "sensor",  STACK_SENSOR,  NULL,
                             PRIO_SENSOR,  &sTaskHandles[TASK_SENSOR]) == pdPASS);
    configASSERT(xTaskCreate(ProcessTask, "process", STACK_PROCESS, NULL,
                             PRIO_PROCESS, &sTaskHandles[TASK_PROCESS]) == pdPASS);
    configASSERT(xTaskCreate(CommTask,    "comm",    STACK_COMM,    NULL,
                             PRIO_COMM,    &sTaskHandles[TASK_COMM]) == pdPASS);
    configASSERT(xTaskCreate(DisplayTask, "display", STACK_DISPLAY, NULL,
                             PRIO_DISPLAY, &sTaskHandles[TASK_DISPLAY]) == pdPASS);
    configASSERT(xTaskCreate(MonitorTask, "monitor", STACK_MONITOR, NULL,
                             PRIO_MONITOR, &sTaskHandles[TASK_MONITOR]) == pdPASS);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;

    /* Scheduler is in an undefined state: blocking TX is acceptable here. */
    BSP_UART1_Print("\r\n[fatal] stack overflow in task: ");
    BSP_UART1_Print(pcTaskName);
    BSP_UART1_Print("\r\n");

    taskDISABLE_INTERRUPTS();
    for (;;)
    {
    }
}
