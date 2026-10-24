#ifndef BSP_UART_H
#define BSP_UART_H

#include <stdint.h>

/* USART1 = on-board USB-serial = PC (CLI + log).
 * USART2 = CH340G = Raspberry Pi (binary protocol link).
 *
 * Day 1: blocking transmit only.
 * Day 3: BSP_UART_AsyncWrite() over Ring Buffer + DMA replaces these calls
 *        in every task; keep the blocking path for panic/early-boot logs.
 */

int BSP_UART1_SendBuffer(const uint8_t *buf, uint16_t len);
int BSP_UART1_Print(const char *str);

int BSP_UART2_SendBuffer(const uint8_t *buf, uint16_t len);
int BSP_UART2_Print(const char *str);

#endif
