#include "bsp_uart.h"

#include "main.h"

/* Raw register-level transmit with a hardware-iteration guard.
 *
 * Deliberately NOT HAL_UART_Transmit: its timeout loop reads uwTick, which
 * was found frozen on Day 1, and its gState machine can refuse service.
 * The Day-3 DMA + Ring Buffer path replaces this; until then polling TXE/TC
 * with a bounded guard is the most dependency-free transmit possible.
 * Guard ~1M iterations ≈ 6 ms at 168 MHz, far beyond one byte time at
 * 115200 (87 us), so a healthy UART never trips it.
 */

#define BSP_UART_TX_GUARD 1000000u

static int bsp_uart_send_raw(USART_TypeDef *usart,
                             const uint8_t *buf,
                             uint16_t len)
{
    for (uint16_t i = 0; i < len; ++i)
    {
        uint32_t guard = 0;

        while (!(usart->SR & USART_SR_TXE))
        {
            if (++guard > BSP_UART_TX_GUARD)
            {
                return -1;
            }
        }

        usart->DR = buf[i];
    }

    uint32_t guard = 0;

    while (!(usart->SR & USART_SR_TC))
    {
        if (++guard > BSP_UART_TX_GUARD)
        {
            return -1;
        }
    }

    return 0;
}

int BSP_UART1_SendBuffer(const uint8_t *buf, uint16_t len)
{
    return bsp_uart_send_raw(USART1, buf, len);
}

int BSP_UART1_Print(const char *str)
{
    uint16_t len = 0;

    while (str[len] != '\0')
    {
        ++len;
    }

    return bsp_uart_send_raw(USART1, (const uint8_t *)str, len);
}

int BSP_UART2_SendBuffer(const uint8_t *buf, uint16_t len)
{
    return bsp_uart_send_raw(USART2, buf, len);
}

int BSP_UART2_Print(const char *str)
{
    uint16_t len = 0;

    while (str[len] != '\0')
    {
        ++len;
    }

    return bsp_uart_send_raw(USART2, (const uint8_t *)str, len);
}
