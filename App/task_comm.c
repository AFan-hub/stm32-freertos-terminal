#include "task_comm.h"

#include <stdio.h>

#include "app_main.h"
#include "bsp_delay.h"
#include "bsp_mpu6050.h"
#include "bsp_uart.h"
#include "cli.h"
#include "main.h"
#include "protocol.h"

/* CommTask responsibilities (manual §10.5 / §17):
 *  - pump the UART1/UART2 DMA chunks (woken by TxCplt notifications)
 *  - drain gCommQueue: human log line to PC + binary frame to the Pi
 *  - assemble CLI lines from the USART1 RX ring and dispatch them
 */

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
        BSP_UART1_SendBuffer((const uint8_t *)line, (uint16_t)n);
    }
}

void CommTask(void *argument)
{
    (void)argument;
    char line[128];
    char cmdbuf[64];
    uint16_t cmdlen = 0;
    uint32_t loops = 0;
    ProcessedData_t sample;

    comm_tick_probe();                      /* raw path, pre-async */
    BSP_UART_AsyncInit(xTaskGetCurrentTaskHandle());
    BSP_UART1_AsyncPrint("stm32_pi terminal boot [fw D3c]\r\n> ");

    for (;;)
    {
        App_TaskHeartbeat(TASK_COMM);

        /* wake on DMA-complete notification, else every 100 ms */
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
        BSP_UART_Pump();

        while (xQueueReceive(gCommQueue, &sample, 0) == pdPASS)
        {
            int n = snprintf(line, sizeof(line),
                             "[comm] up=%lu s l=%.1f t=%.1f alm=%lu fault=0x%02lX\r\n> ",
                             (unsigned long)(xTaskGetTickCount() / 1000),
                             (double)sample.filtered_1,
                             (double)sample.filtered_2,
                             (unsigned long)(sample.alarm_flags & 1u),
                             (unsigned long)gFaultFlags);

            if (n > 0)
            {
                BSP_UART1_AsyncPrint(line);
            }

            /* binary protocol frame to the Raspberry Pi on USART2 */
            SensorPayload_t pl;
            pl.timestamp_ms = sample.raw.timestamp_ms;
            pl.sensor_1     = sample.filtered_1;
            pl.sensor_2     = sample.filtered_2;
            pl.status       = (uint16_t)(sample.alarm_flags & 0xFFFFu);

            uint8_t frame[32];
            uint16_t flen = Protocol_EncodeSensor(&pl, frame, sizeof(frame));

            if (flen > 0)
            {
                BSP_UART2_AsyncWrite(frame, flen);
            }
        }

        /* CLI line assembly from the RX ring */
        uint8_t b;
        while (BSP_UART1_ReadByte(&b) == 0)
        {
            if (b == '\r' || b == '\n')
            {
                if (cmdlen > 0)
                {
                    cmdbuf[cmdlen] = '\0';
                    CLI_ProcessLine(cmdbuf);
                    cmdlen = 0;
                    BSP_UART1_AsyncPrint("> ");
                }
            }
            else if (cmdlen < sizeof(cmdbuf) - 1)
            {
                cmdbuf[cmdlen++] = (char)b;
            }
        }

        (void)++loops;
    }
}
