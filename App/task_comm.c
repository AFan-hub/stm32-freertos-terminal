#include "task_comm.h"

#include <stdio.h>

#include "app_main.h"
#include "bsp_delay.h"
#include "bsp_mpu6050.h"
#include "bsp_uart.h"
#include "main.h"

/* Day-1 COM-mixup insurance: mirror every log line to USART2 as well, so a
 * CH340G dongle on PA2/PA3 always shows traffic even if the on-board
 * USB-serial path (USART1) is misidentified or jumpered elsewhere. */
static void comm_print(const char *str)
{
    BSP_UART1_Print(str);
    BSP_UART2_Print(str);
}

static void comm_send(const uint8_t *buf, uint16_t len)
{
    BSP_UART1_SendBuffer(buf, len);
    BSP_UART2_SendBuffer(buf, len);
}

/* Day-1 tick forensics: the HAL timebase (TIM6) was found frozen during
 * lcd_init. This probe measures uwTick progress over a tick-independent
 * 500 ms DWT delay and dumps the TIM6/NVIC state, so the serial log alone
 * tells us whether the HAL tick is alive after the scheduler starts. */
static void comm_tick_probe(void)
{
    char line[96];
    uint32_t t1 = HAL_GetTick();
    bsp_delay_ms(500);
    uint32_t t2 = HAL_GetTick();

    int n = snprintf(line, sizeof(line),
                     "[diag] uwTick delta/500ms=%lu CR1=0x%02lX SR=0x%02lX DIER=0x%02lX NVIC=%lu\r\n",
                     (unsigned long)(t2 - t1),
                     (unsigned long)TIM6->CR1,
                     (unsigned long)TIM6->SR,
                     (unsigned long)TIM6->DIER,
                     (unsigned long)((NVIC->ISER[1] >> (TIM6_DAC_IRQn - 32)) & 1u));

    if (n > 0)
    {
        comm_send((const uint8_t *)line, (uint16_t)n);
    }
}

void CommTask(void *argument)
{
    (void)argument;
    char line[96];
    ProcessedData_t latest = { { 0 }, 0.0f, 0.0f, 0 };
    ProcessedData_t sample;

    comm_print("stm32_pi terminal boot\r\n");
    comm_tick_probe();

    for (;;)
    {
        App_TaskHeartbeat(TASK_COMM);

        while (xQueueReceive(gCommQueue, &sample, 0) == pdPASS)
        {
            latest = sample;
        }

        /* Day 3: this blocking print is replaced by the Ring Buffer +
         * DMA async path; Day 6 adds the CLI line parser on USART1 RX.
         */
        int n = snprintf(line, sizeof(line),
                         "[comm] up=%lu s t=%.1f l=%.1f alm=%lu png=%d fault=0x%02lX\r\n",
                         (unsigned long)(xTaskGetTickCount() / 1000),
                         (double)latest.filtered_1,
                         (double)latest.filtered_2,
                         (unsigned long)(latest.alarm_flags & 1u),
                         BSP_MPU6050_Ping(),
                         (unsigned long)gFaultFlags);

        if (n > 0)
        {
            comm_send((const uint8_t *)line, (uint16_t)n);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
