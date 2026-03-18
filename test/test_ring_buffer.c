#include "unity/unity.h"
#include "../core/ring_buffer/ring_buffer.h"

static RingBuffer_t rb;
static u8 storage[16];

void setUp(void)
{
    RingBuffer_Init(&rb, storage, sizeof(storage));
}

void tearDown(void) {}

void test_init_creates_empty_buffer(void)
{
    TEST_ASSERT_TRUE(RingBuffer_IsEmpty(&rb));
    TEST_ASSERT_FALSE(RingBuffer_IsFull(&rb));
    TEST_ASSERT_EQUAL_UINT16(0U, RingBuffer_GetCount(&rb));
    TEST_ASSERT_EQUAL_UINT16(16U, RingBuffer_GetFree(&rb));
}

void test_init_rejects_null(void)
{
    TEST_ASSERT_EQUAL(RESULT_INVALID_PARAM, RingBuffer_Init(NULL, storage, 16U));
    TEST_ASSERT_EQUAL(RESULT_INVALID_PARAM, RingBuffer_Init(&rb, NULL, 16U));
    TEST_ASSERT_EQUAL(RESULT_INVALID_PARAM, RingBuffer_Init(&rb, storage, 0U));
}

void test_push_pop_single_byte(void)
{
    TEST_ASSERT_EQUAL(RESULT_OK, RingBuffer_Push(&rb, 0xABU));
    TEST_ASSERT_EQUAL_UINT16(1U, RingBuffer_GetCount(&rb));

    u8 byte = 0U;
    TEST_ASSERT_EQUAL(RESULT_OK, RingBuffer_Pop(&rb, &byte));
    TEST_ASSERT_EQUAL_UINT8(0xABU, byte);
    TEST_ASSERT_TRUE(RingBuffer_IsEmpty(&rb));
}

void test_fifo_order(void)
{
    for (u8 i = 0U; i < 5U; i++) {
        RingBuffer_Push(&rb, i + 10U);
    }

    for (u8 i = 0U; i < 5U; i++) {
        u8 byte;
        RingBuffer_Pop(&rb, &byte);
        TEST_ASSERT_EQUAL_UINT8(i + 10U, byte);
    }
}

void test_full_buffer_rejects_push(void)
{
    for (u8 i = 0U; i < 16U; i++) {
        TEST_ASSERT_EQUAL(RESULT_OK, RingBuffer_Push(&rb, i));
    }

    TEST_ASSERT_TRUE(RingBuffer_IsFull(&rb));
    TEST_ASSERT_EQUAL(RESULT_BUFFER_FULL, RingBuffer_Push(&rb, 0xFFU));
}

void test_empty_buffer_rejects_pop(void)
{
    u8 byte;
    TEST_ASSERT_EQUAL(RESULT_BUFFER_EMPTY, RingBuffer_Pop(&rb, &byte));
}

void test_wraparound(void)
{
    for (u8 i = 0U; i < 14U; i++) {
        RingBuffer_Push(&rb, i);
    }

    u8 byte;
    for (u8 i = 0U; i < 10U; i++) {
        RingBuffer_Pop(&rb, &byte);
    }

    for (u8 i = 0U; i < 10U; i++) {
        RingBuffer_Push(&rb, 100U + i);
    }

    TEST_ASSERT_EQUAL_UINT16(14U, RingBuffer_GetCount(&rb));

    for (u8 i = 10U; i < 14U; i++) {
        RingBuffer_Pop(&rb, &byte);
        TEST_ASSERT_EQUAL_UINT8(i, byte);
    }

    for (u8 i = 0U; i < 10U; i++) {
        RingBuffer_Pop(&rb, &byte);
        TEST_ASSERT_EQUAL_UINT8(100U + i, byte);
    }
}

void test_contains(void)
{
    RingBuffer_Push(&rb, 'A');
    RingBuffer_Push(&rb, 'B');
    RingBuffer_Push(&rb, '>');

    TEST_ASSERT_TRUE(RingBuffer_Contains(&rb, '>'));
    TEST_ASSERT_TRUE(RingBuffer_Contains(&rb, 'A'));
    TEST_ASSERT_FALSE(RingBuffer_Contains(&rb, 'Z'));
}

void test_peek_at(void)
{
    RingBuffer_Push(&rb, 10U);
    RingBuffer_Push(&rb, 20U);
    RingBuffer_Push(&rb, 30U);

    u8 byte;
    TEST_ASSERT_EQUAL(RESULT_OK, RingBuffer_PeekAt(&rb, 0U, &byte));
    TEST_ASSERT_EQUAL_UINT8(10U, byte);

    TEST_ASSERT_EQUAL(RESULT_OK, RingBuffer_PeekAt(&rb, 2U, &byte));
    TEST_ASSERT_EQUAL_UINT8(30U, byte);

    TEST_ASSERT_EQUAL(RESULT_INVALID_PARAM, RingBuffer_PeekAt(&rb, 3U, &byte));

    TEST_ASSERT_EQUAL_UINT16(3U, RingBuffer_GetCount(&rb));
}

void test_push_pop_multiple(void)
{
    u8 data[] = { 1U, 2U, 3U, 4U, 5U };
    u16 pushed = 0U;
    TEST_ASSERT_EQUAL(RESULT_OK, RingBuffer_PushMultiple(&rb, data, 5U, &pushed));
    TEST_ASSERT_EQUAL_UINT16(5U, pushed);

    u8 out[8];
    u16 popped = 0U;
    TEST_ASSERT_EQUAL(RESULT_OK, RingBuffer_PopMultiple(&rb, out, 8U, &popped));
    TEST_ASSERT_EQUAL_UINT16(5U, popped);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(data, out, 5U);
}

void test_reset(void)
{
    RingBuffer_Push(&rb, 1U);
    RingBuffer_Push(&rb, 2U);

    TEST_ASSERT_EQUAL(RESULT_OK, RingBuffer_Reset(&rb));
    TEST_ASSERT_TRUE(RingBuffer_IsEmpty(&rb));
    TEST_ASSERT_EQUAL_UINT16(16U, RingBuffer_GetFree(&rb));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_init_creates_empty_buffer);
    RUN_TEST(test_init_rejects_null);
    RUN_TEST(test_push_pop_single_byte);
    RUN_TEST(test_fifo_order);
    RUN_TEST(test_full_buffer_rejects_push);
    RUN_TEST(test_empty_buffer_rejects_pop);
    RUN_TEST(test_wraparound);
    RUN_TEST(test_contains);
    RUN_TEST(test_peek_at);
    RUN_TEST(test_push_pop_multiple);
    RUN_TEST(test_reset);

    return UNITY_END();
}
