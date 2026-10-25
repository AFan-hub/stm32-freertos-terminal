#ifndef APP_MAIN_H
#define APP_MAIN_H

#include <stdint.h>

#include "FreeRTOS.h"
#include "event_groups.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#include "app_types.h"

typedef enum
{
    TASK_SENSOR = 0,
    TASK_PROCESS,
    TASK_COMM,
    TASK_DISPLAY,
    TASK_MONITOR,
    TASK_COUNT
} TaskId_t;

typedef enum
{
    SYS_OK = 0,
    SYS_SENSOR_TIMEOUT,
    SYS_UART_ERROR,
    SYS_QUEUE_OVERFLOW,
    SYS_TASK_STALL,
    SYS_CONFIG_CRC_ERROR
} SystemFault_t;

/* FreeRTOS objects owned by the application (created in App_Main_Init). */
extern QueueHandle_t     gSensorQueue;
extern QueueHandle_t     gDisplayQueue;
extern QueueHandle_t     gCommQueue;
extern SemaphoreHandle_t gConfigMutex;
extern SystemConfig_t    gSystemConfig;

extern volatile uint32_t gHeartbeat[TASK_COUNT];
extern volatile uint32_t gFaultFlags;   /* one bit per SystemFault_t */
extern EventGroupHandle_t gSystemEventGroup;  /* same bits, waitable */

void App_Main_Init(void);
void App_TaskHeartbeat(TaskId_t id);
void App_SetFault(SystemFault_t fault);
TaskHandle_t App_TaskHandle(TaskId_t id);

/* run-time stats time base (TIM5, 1 MHz free running) */
void vRunTimeTimerInit(void);
uint32_t ulRunTimeTimerValue(void);

#endif
