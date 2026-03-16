#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include "../types.h"

typedef struct {
    u8* buffer;
    u16 capacity;
    u16 head;
    u16 tail;
    u16 count;
} RingBuffer_t;


Result_t RingBuffer_Init(RingBuffer_t* rb, u8* storage, u16 capacity);

Result_t RingBuffer_Reset(RingBuffer_t* rb);

Result_t RingBuffer_Push(RingBuffer_t* rb, u8 byte);

Result_t RingBuffer_Pop(RingBuffer_t* rb, u8* byte);

Result_t RingBuffer_PeekAt(const RingBuffer_t* rb, u16 offset, u8* byte);

Result_t RingBuffer_PushMultiple(RingBuffer_t* rb, const u8* data, u16 length, u16* actual_pushed);

Result_t RingBuffer_PopMultiple(RingBuffer_t* rb, u8* buffer, u16 max_length, u16* actual_popped);

bool RingBuffer_Contains(const RingBuffer_t* rb, u8 byte);


u16 RingBuffer_GetCount(const RingBuffer_t* rb);

u16 RingBuffer_GetFree(const RingBuffer_t* rb);

bool RingBuffer_IsEmpty(const RingBuffer_t* rb);

bool RingBuffer_IsFull(const RingBuffer_t* rb);

#endif