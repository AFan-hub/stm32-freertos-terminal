#ifndef BSP_UART_H
#define BSP_UART_H

#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

/* USART1 = on-board USB-serial = PC (CLI + log).
 * USART2 = CH340G = Raspberry Pi (binary protocol link).
 *
 * Two transmit paths:
 *  - raw blocking (registers, no HAL state, no tick): boot + fatal logs
 *  - async: Ring Buffer -> DMA chunk -> TxCplt ISR -> task notification ->
 *    CommTask pumps the next chunk (manual §10.3-10.5)
 */

/* blocking / boot / fatal path */
int BSP_UART1_SendBuffer(const uint8_t *buf, uint16_t len);
int BSP_UART1_Print(const char *str);
int BSP_UART2_SendBuffer(const uint8_t *buf, uint16_t len);
int BSP_UART2_Print(const char *str);

/* async path; call once from CommTask before first use */
void BSP_UART_AsyncInit(TaskHandle_t pump_task);

int BSP_UART1_AsyncWrite(const uint8_t *buf, uint16_t len);
int BSP_UART1_AsyncPrint(const char *str);
int BSP_UART2_AsyncWrite(const uint8_t *buf, uint16_t len);

/* CommTask calls this every loop pass: starts the next DMA chunk if idle */
void BSP_UART_Pump(void);

/* CLI receive (USART1 RX interrupt -> ring) */
int BSP_UART1_ReadByte(uint8_t *b);

/* observability for the CLI status command */
uint16_t BSP_UART1_TxUsed(void);
uint16_t BSP_UART1_TxFree(void);
uint32_t BSP_UART_TxDrops(void);

#endif
