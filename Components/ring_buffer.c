#include "ring_buffer.h"

void RingBuffer_Init(RingBuffer_t *rb, uint8_t *storage, uint16_t size)
{
    rb->buffer = storage;
    rb->size = size;
    rb->head = 0;
    rb->tail = 0;
}

static uint16_t RingBuffer_Next(const RingBuffer_t *rb, uint16_t index)
{
    return (uint16_t)((index + 1) % rb->size);
}

bool RingBuffer_IsEmpty(const RingBuffer_t *rb)
{
    return rb->head == rb->tail;
}

bool RingBuffer_IsFull(const RingBuffer_t *rb)
{
    return RingBuffer_Next(rb, rb->head) == rb->tail;
}

uint16_t RingBuffer_Available(const RingBuffer_t *rb)
{
    return (uint16_t)((rb->head + rb->size - rb->tail) % rb->size);
}

uint16_t RingBuffer_Free(const RingBuffer_t *rb)
{
    /* one slot is always kept empty to tell full from empty */
    return (uint16_t)(rb->size - 1u - RingBuffer_Available(rb));
}

bool RingBuffer_Write(RingBuffer_t *rb, uint8_t data)
{
    const uint16_t next = RingBuffer_Next(rb, rb->head);

    if (next == rb->tail)
    {
        return false;
    }

    rb->buffer[rb->head] = data;
    rb->head = next;

    return true;
}

bool RingBuffer_Read(RingBuffer_t *rb, uint8_t *data)
{
    if (rb->head == rb->tail)
    {
        return false;
    }

    *data = rb->buffer[rb->tail];
    rb->tail = RingBuffer_Next(rb, rb->tail);

    return true;
}

const uint8_t *RingBuffer_PeekLinear(const RingBuffer_t *rb, uint16_t *len)
{
    if (rb->head == rb->tail)
    {
        *len = 0;
        return rb->buffer;
    }

    if (rb->head > rb->tail)
    {
        *len = (uint16_t)(rb->head - rb->tail);
        return &rb->buffer[rb->tail];
    }

    /* readable region wraps: hand out the part up to the buffer end */
    *len = (uint16_t)(rb->size - rb->tail);
    return &rb->buffer[rb->tail];
}

void RingBuffer_Consume(RingBuffer_t *rb, uint16_t count)
{
    rb->tail = (uint16_t)((rb->tail + count) % rb->size);
}
