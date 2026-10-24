#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdbool.h>
#include <stdint.h>

/* Single-producer / single-consumer ring buffer.
 *
 * One slot is always kept empty so that head == tail means empty and
 * next(head) == tail means full. head is written only by the producer,
 * tail only by the consumer; both are volatile because each is read from
 * the other execution context (task vs. DMA-complete path).
 */
typedef struct
{
    uint8_t *buffer;
    uint16_t size;
    volatile uint16_t head;
    volatile uint16_t tail;
} RingBuffer_t;

void RingBuffer_Init(RingBuffer_t *rb, uint8_t *storage, uint16_t size);

bool RingBuffer_Write(RingBuffer_t *rb, uint8_t data);
bool RingBuffer_Read(RingBuffer_t *rb, uint8_t *data);

uint16_t RingBuffer_Available(const RingBuffer_t *rb);
uint16_t RingBuffer_Free(const RingBuffer_t *rb);
bool RingBuffer_IsFull(const RingBuffer_t *rb);
bool RingBuffer_IsEmpty(const RingBuffer_t *rb);

/* Longest contiguous readable region, for one HAL_UART_Transmit_DMA() call
 * (a DMA transfer cannot wrap around the end of the buffer). */
const uint8_t *RingBuffer_PeekLinear(const RingBuffer_t *rb, uint16_t *len);

/* Advance tail after the DMA transfer of a PeekLinear() region completed. */
void RingBuffer_Consume(RingBuffer_t *rb, uint16_t count);

#endif
