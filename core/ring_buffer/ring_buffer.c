#include "ring_buffer.h"

Result_t RingBuffer_Init(RingBuffer_t* rb, u8* storage, u16 capacity)
{
    if (rb == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (storage == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (capacity == 0U) {
        return RESULT_INVALID_PARAM;
    }
    
    rb->buffer = storage;
    rb->capacity = capacity;
    rb->head = 0U;
    rb->tail = 0U;
    rb->count = 0U;
    
    return RESULT_OK;
}

Result_t RingBuffer_Reset(RingBuffer_t* rb)
{
    if (rb == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    rb->head = 0U;
    rb->tail = 0U;
    rb->count = 0U;
    
    return RESULT_OK;
}

Result_t RingBuffer_Push(RingBuffer_t* rb, u8 byte)
{
    if (rb == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (rb->count >= rb->capacity) {
        return RESULT_BUFFER_FULL;
    }
    
    rb->buffer[rb->head] = byte;
    rb->head = (rb->head + 1U) % rb->capacity;
    rb->count++;
    
    return RESULT_OK;
}

Result_t RingBuffer_Pop(RingBuffer_t* rb, u8* byte)
{
    if (rb == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (byte == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (rb->count == 0U) {
        return RESULT_BUFFER_EMPTY;
    }
    
    *byte = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1U) % rb->capacity;
    rb->count--;
    
    return RESULT_OK;
}

Result_t RingBuffer_PeekAt(const RingBuffer_t* rb, u16 offset, u8* byte)
{
    if (rb == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (byte == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (offset >= rb->count) {
        return RESULT_INVALID_PARAM;
    }
    
    u16 idx = (rb->tail + offset) % rb->capacity;
    *byte = rb->buffer[idx];
    
    return RESULT_OK;
}

Result_t RingBuffer_PushMultiple(RingBuffer_t* rb, const u8* data, u16 length, u16* actual_pushed)
{
    if (rb == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (data == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    u16 pushed = 0U;
    
    for (u16 i = 0U; i < length; i++) {
        if (rb->count >= rb->capacity) {
            if (actual_pushed != NULL_PTR) {
                *actual_pushed = pushed;
            }
            return RESULT_BUFFER_FULL;
        }
        
        rb->buffer[rb->head] = data[i];
        rb->head = (rb->head + 1U) % rb->capacity;
        rb->count++;
        pushed++;
    }
    
    if (actual_pushed != NULL_PTR) {
        *actual_pushed = pushed;
    }
    
    return RESULT_OK;
}

Result_t RingBuffer_PopMultiple(RingBuffer_t* rb, u8* buffer, u16 max_length, u16* actual_popped)
{
    if (rb == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (buffer == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (actual_popped == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    u16 popped = 0U;
    
    while ((popped < max_length) && (rb->count > 0U)) {
        buffer[popped] = rb->buffer[rb->tail];
        rb->tail = (rb->tail + 1U) % rb->capacity;
        rb->count--;
        popped++;
    }
    
    *actual_popped = popped;
    
    return RESULT_OK;
}

bool RingBuffer_Contains(const RingBuffer_t* rb, u8 byte)
{
    if (rb == NULL_PTR) {
        return false;
    }
    
    if (rb->count == 0U) {
        return false;
    }
    
    u16 idx = rb->tail;
    for (u16 i = 0U; i < rb->count; i++) {
        if (rb->buffer[idx] == byte) {
            return true;
        }
        idx = (idx + 1U) % rb->capacity;
    }
    
    return false;
}

u16 RingBuffer_GetCount(const RingBuffer_t* rb)
{
    if (rb == NULL_PTR) {
        return 0U;
    }
    
    return rb->count;
}

u16 RingBuffer_GetFree(const RingBuffer_t* rb)
{
    if (rb == NULL_PTR) {
        return 0U;
    }
    
    return rb->capacity - rb->count;
}

bool RingBuffer_IsEmpty(const RingBuffer_t* rb)
{
    if (rb == NULL_PTR) {
        return true;
    }
    
    return (rb->count == 0U);
}

bool RingBuffer_IsFull(const RingBuffer_t* rb)
{
    if (rb == NULL_PTR) {
        return true;
    }
    
    return (rb->count >= rb->capacity);
}
