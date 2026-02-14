#ifndef CORE_TYPES_H
#define CORE_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;

#define STATIC_ASSERT(cond, msg) typedef char static_assertion_##msg[(cond) ? 1 : -1]

#define UNUSED(x) ((void)(x))

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define NULL_PTR ((void*)0)

typedef enum {
    RESULT_OK = 0,
    RESULT_ERROR = 1,
    RESULT_TIMEOUT = 2,
    RESULT_BUSY = 3,
    RESULT_INVALID_PARAM = 4,
    RESULT_NOT_READY = 5,
    RESULT_NO_DATA = 6,
    RESULT_BUFFER_FULL = 7,
    RESULT_BUFFER_EMPTY = 8,
    RESULT_NOT_SUPPORTED = 9,
    RESULT_PROTOCOL_ERROR = 10,
    RESULT_COMM_ERROR = 11,
    RESULT_MAX
} Result_t;

typedef struct {
    u32 seconds;
    u32 milliseconds;
} Timestamp_t;

typedef void (*Callback_t)(void* context, Result_t result);

#endif
