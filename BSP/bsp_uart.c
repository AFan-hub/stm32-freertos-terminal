#include "bsp_uart.h"

#include "main.h"
#include "ring_buffer.h"

/* Handles are defined in main.c (CubeMX generated). */
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;

#define BSP_UART_TX_TIMEOUT_MS 100
#define BSP_UART_TX_GUARD      1000000u

/* ---------------- blocking raw path (boot / fatal) ---------------- */

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
    while (str[len] != '\0') ++len;
    return bsp_uart_send_raw(USART1, (const uint8_t *)str, len);
}

int BSP_UART2_SendBuffer(const uint8_t *buf, uint16_t len)
{
    return bsp_uart_send_raw(USART2, buf, len);
}

int BSP_UART2_Print(const char *str)
{
    uint16_t len = 0;
    while (str[len] != '\0') ++len;
    return bsp_uart_send_raw(USART2, (const uint8_t *)str, len);
}

/* ---------------- async DMA + ring buffer path ---------------- */

static uint8_t s_tx1_mem[1024];
static uint8_t s_tx2_mem[512];
static uint8_t s_rx1_mem[256];

static RingBuffer_t s_tx1;
static RingBuffer_t s_tx2;
static RingBuffer_t s_rx1;

static volatile uint8_t s_tx1_busy;
static volatile uint8_t s_tx2_busy;
static uint16_t s_tx1_chunk;
static uint16_t s_tx2_chunk;
static uint8_t  s_rx1_byte;
static uint32_t s_tx_drops;
static TaskHandle_t s_pump_task;

void BSP_UART_AsyncInit(TaskHandle_t pump_task)
{
    RingBuffer_Init(&s_tx1, s_tx1_mem, sizeof(s_tx1_mem));
    RingBuffer_Init(&s_tx2, s_tx2_mem, sizeof(s_tx2_mem));
    RingBuffer_Init(&s_rx1, s_rx1_mem, sizeof(s_rx1_mem));

    s_tx1_busy = 0;
    s_tx2_busy = 0;
    s_tx_drops = 0;
    s_pump_task = pump_task;

    /* single-byte interrupt-driven receive for the CLI */
    HAL_UART_Receive_IT(&huart1, &s_rx1_byte, 1);
}

static int async_write(RingBuffer_t *rb, const uint8_t *buf, uint16_t len)
{
    uint16_t written = 0;

    taskENTER_CRITICAL();
    for (; written < len; ++written)
    {
        if (!RingBuffer_Write(rb, buf[written]))
        {
            break;
        }
    }
    taskEXIT_CRITICAL();

    if (written < len)
    {
        ++s_tx_drops;
    }

    if (s_pump_task != NULL)
    {
        xTaskNotifyGive(s_pump_task);
    }

    return (int)written;
}

int BSP_UART1_AsyncWrite(const uint8_t *buf, uint16_t len)
{
    return async_write(&s_tx1, buf, len);
}

int BSP_UART1_AsyncPrint(const char *str)
{
    uint16_t len = 0;
    while (str[len] != '\0') ++len;
    return async_write(&s_tx1, (const uint8_t *)str, len);
}

int BSP_UART2_AsyncWrite(const uint8_t *buf, uint16_t len)
{
    return async_write(&s_tx2, buf, len);
}

void BSP_UART_Pump(void)
{
    uint16_t len = 0;
    const uint8_t *p;

    if (!s_tx1_busy)
    {
        p = RingBuffer_PeekLinear(&s_tx1, &len);

        if (len > 0)
        {
            s_tx1_chunk = len;
            s_tx1_busy = 1;

            if (HAL_UART_Transmit_DMA(&huart1, (uint8_t *)p, len) != HAL_OK)
            {
                s_tx1_busy = 0;
                s_tx1_chunk = 0;
            }
        }
    }

    if (!s_tx2_busy)
    {
        p = RingBuffer_PeekLinear(&s_tx2, &len);

        if (len > 0)
        {
            s_tx2_chunk = len;
            s_tx2_busy = 1;

            if (HAL_UART_Transmit_DMA(&huart2, (uint8_t *)p, len) != HAL_OK)
            {
                s_tx2_busy = 0;
                s_tx2_chunk = 0;
            }
        }
    }
}

int BSP_UART1_ReadByte(uint8_t *b)
{
    return RingBuffer_Read(&s_rx1, b) ? 0 : -1;
}

uint16_t BSP_UART1_TxUsed(void) { return RingBuffer_Available(&s_tx1); }
uint16_t BSP_UART1_TxFree(void) { return RingBuffer_Free(&s_tx1); }
uint32_t BSP_UART_TxDrops(void) { return s_tx_drops; }

/* ---------------- HAL callbacks (ISR context) ---------------- */

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    BaseType_t woken = pdFALSE;

    if (huart->Instance == USART1)
    {
        RingBuffer_Consume(&s_tx1, s_tx1_chunk);
        s_tx1_chunk = 0;
        s_tx1_busy = 0;

        if (s_pump_task != NULL)
        {
            vTaskNotifyGiveFromISR(s_pump_task, &woken);
        }
    }
    else if (huart->Instance == USART2)
    {
        RingBuffer_Consume(&s_tx2, s_tx2_chunk);
        s_tx2_chunk = 0;
        s_tx2_busy = 0;

        if (s_pump_task != NULL)
        {
            vTaskNotifyGiveFromISR(s_pump_task, &woken);
        }
    }

    portYIELD_FROM_ISR(woken);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        /* CLI input is best-effort: a full ring drops the byte */
        RingBuffer_Write(&s_rx1, s_rx1_byte);
        HAL_UART_Receive_IT(&huart1, &s_rx1_byte, 1);
    }
}
