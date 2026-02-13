#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include "../types.h"

typedef enum {
    ERR_NONE = 0,
    
    ERR_COMM_TIMEOUT = 100,
    ERR_COMM_NO_RESPONSE = 101,
    ERR_COMM_INVALID_RESPONSE = 102,
    ERR_COMM_BUFFER_OVERFLOW = 103,
    ERR_COMM_WRITE_FAILED = 104,
    ERR_COMM_READ_FAILED = 105,
    ERR_COMM_DISCONNECTED = 106,
    
    ERR_ELM_INIT_FAILED = 200,
    ERR_ELM_RESET_FAILED = 201,
    ERR_ELM_PROTOCOL_FAILED = 202,
    ERR_ELM_NO_DATA = 203,
    ERR_ELM_STOPPED = 204,
    ERR_ELM_BUFFER_FULL = 205,
    ERR_ELM_CAN_ERROR = 206,
    ERR_ELM_BUS_INIT_ERROR = 207,
    ERR_ELM_UNABLE_TO_CONNECT = 208,
    
    ERR_OBD_NO_RESPONSE = 300,
    ERR_OBD_INVALID_MODE = 301,
    ERR_OBD_INVALID_PID = 302,
    ERR_OBD_NEGATIVE_RESPONSE = 303,
    ERR_OBD_CHECKSUM_FAILED = 304,
    ERR_OBD_FRAME_ERROR = 305,
    
    ERR_STATE_INVALID_TRANSITION = 400,
    ERR_STATE_TIMEOUT = 401,
    ERR_STATE_MAX_RETRIES = 402,
    
    ERR_SANITY_OUT_OF_RANGE = 500,
    ERR_SANITY_SENSOR_STUCK = 501,
    ERR_SANITY_INVALID_DATA = 502,
    
    ERR_MEMORY_ALLOCATION = 600,
    ERR_MEMORY_OVERFLOW = 601,
    
    ERR_PARAM_NULL_POINTER = 700,
    ERR_PARAM_INVALID_VALUE = 701,
    ERR_PARAM_OUT_OF_BOUNDS = 702,
    
    ERR_SCHEDULER_QUEUE_FULL = 800,
    ERR_SCHEDULER_TASK_NOT_FOUND = 801,
    
    ERR_UNKNOWN = 999,
    ERR_MAX
} ErrorCode_t;

typedef enum {
    ERR_SEV_INFO = 0,
    ERR_SEV_WARNING = 1,
    ERR_SEV_ERROR = 2,
    ERR_SEV_CRITICAL = 3,
    ERR_SEV_MAX
} ErrorSeverity_t;

typedef struct {
    ErrorCode_t code;
    ErrorSeverity_t severity;
    u32 timestamp_ms;
    u32 line;
    const char* file;
    const char* function;
} ErrorInfo_t;

typedef void (*ErrorCallback_t)(const ErrorInfo_t* error);

typedef struct {
    ErrorCallback_t callback;
    bool initialized;
} ErrorHandler_t;

Result_t ErrorHandler_Init(ErrorHandler_t* handler, ErrorCallback_t callback);

Result_t ErrorHandler_Report(ErrorHandler_t* handler, 
                             ErrorCode_t code, 
                             ErrorSeverity_t severity,
                             const char* file,
                             u32 line,
                             const char* function);

const char* ErrorHandler_GetCodeString(ErrorCode_t code);

const char* ErrorHandler_GetSeverityString(ErrorSeverity_t severity);

bool ErrorHandler_IsRecoverable(ErrorCode_t code);

#define ERROR_REPORT(handler, code, severity) \
    ErrorHandler_Report((handler), (code), (severity), __FILE__, __LINE__, __func__)

#endif
