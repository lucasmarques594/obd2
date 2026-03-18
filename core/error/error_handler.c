#include "error_handler.h"

typedef struct {
    ErrorCode_t code;
    const char* string;
    bool recoverable;
} ErrorEntry_t;

static const ErrorEntry_t error_table[] = {
    { ERR_NONE,                     "ERR_NONE",                     true  },
    { ERR_COMM_TIMEOUT,             "ERR_COMM_TIMEOUT",             true  },
    { ERR_COMM_NO_RESPONSE,         "ERR_COMM_NO_RESPONSE",         true  },
    { ERR_COMM_INVALID_RESPONSE,    "ERR_COMM_INVALID_RESPONSE",    true  },
    { ERR_COMM_BUFFER_OVERFLOW,     "ERR_COMM_BUFFER_OVERFLOW",     true  },
    { ERR_COMM_WRITE_FAILED,        "ERR_COMM_WRITE_FAILED",        true  },
    { ERR_COMM_READ_FAILED,         "ERR_COMM_READ_FAILED",         true  },
    { ERR_COMM_DISCONNECTED,        "ERR_COMM_DISCONNECTED",        false },
    { ERR_ELM_INIT_FAILED,          "ERR_ELM_INIT_FAILED",          true  },
    { ERR_ELM_RESET_FAILED,         "ERR_ELM_RESET_FAILED",         true  },
    { ERR_ELM_PROTOCOL_FAILED,      "ERR_ELM_PROTOCOL_FAILED",      true  },
    { ERR_ELM_NO_DATA,              "ERR_ELM_NO_DATA",              true  },
    { ERR_ELM_STOPPED,              "ERR_ELM_STOPPED",              true  },
    { ERR_ELM_BUFFER_FULL,          "ERR_ELM_BUFFER_FULL",          true  },
    { ERR_ELM_CAN_ERROR,            "ERR_ELM_CAN_ERROR",            true  },
    { ERR_ELM_BUS_INIT_ERROR,       "ERR_ELM_BUS_INIT_ERROR",       true  },
    { ERR_ELM_UNABLE_TO_CONNECT,    "ERR_ELM_UNABLE_TO_CONNECT",    false },
    { ERR_OBD_NO_RESPONSE,          "ERR_OBD_NO_RESPONSE",          true  },
    { ERR_OBD_INVALID_MODE,         "ERR_OBD_INVALID_MODE",         false },
    { ERR_OBD_INVALID_PID,          "ERR_OBD_INVALID_PID",          false },
    { ERR_OBD_NEGATIVE_RESPONSE,    "ERR_OBD_NEGATIVE_RESPONSE",    true  },
    { ERR_OBD_CHECKSUM_FAILED,      "ERR_OBD_CHECKSUM_FAILED",      true  },
    { ERR_OBD_FRAME_ERROR,          "ERR_OBD_FRAME_ERROR",          true  },
    { ERR_STATE_INVALID_TRANSITION, "ERR_STATE_INVALID_TRANSITION", false },
    { ERR_STATE_TIMEOUT,            "ERR_STATE_TIMEOUT",            true  },
    { ERR_STATE_MAX_RETRIES,        "ERR_STATE_MAX_RETRIES",        true  },
    { ERR_SANITY_OUT_OF_RANGE,      "ERR_SANITY_OUT_OF_RANGE",      true  },
    { ERR_SANITY_SENSOR_STUCK,      "ERR_SANITY_SENSOR_STUCK",      true  },
    { ERR_SANITY_INVALID_DATA,      "ERR_SANITY_INVALID_DATA",      true  },
    { ERR_MEMORY_ALLOCATION,        "ERR_MEMORY_ALLOCATION",        false },
    { ERR_MEMORY_OVERFLOW,          "ERR_MEMORY_OVERFLOW",          false },
    { ERR_PARAM_NULL_POINTER,       "ERR_PARAM_NULL_POINTER",       false },
    { ERR_PARAM_INVALID_VALUE,      "ERR_PARAM_INVALID_VALUE",      false },
    { ERR_PARAM_OUT_OF_BOUNDS,      "ERR_PARAM_OUT_OF_BOUNDS",      false },
    { ERR_SCHEDULER_QUEUE_FULL,     "ERR_SCHEDULER_QUEUE_FULL",     true  },
    { ERR_SCHEDULER_TASK_NOT_FOUND, "ERR_SCHEDULER_TASK_NOT_FOUND", false },
    { ERR_UNKNOWN,                  "ERR_UNKNOWN",                  false }
};

#define ERROR_TABLE_SIZE (sizeof(error_table) / sizeof(error_table[0]))

static const ErrorEntry_t* find_error_entry(ErrorCode_t code)
{
    for (u32 i = 0U; i < ERROR_TABLE_SIZE; i++) {
        if (error_table[i].code == code) {
            return &error_table[i];
        }
    }
    return NULL_PTR;
}

static const char* const severity_strings[] = {
    [ERR_SEV_INFO] = "INFO",
    [ERR_SEV_WARNING] = "WARNING",
    [ERR_SEV_ERROR] = "ERROR",
    [ERR_SEV_CRITICAL] = "CRITICAL"
};

Result_t ErrorHandler_Init(ErrorHandler_t* handler, ErrorCallback_t callback)
{
    if (handler == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }

    handler->callback = callback;
    handler->initialized = true;

    return RESULT_OK;
}

Result_t ErrorHandler_Report(ErrorHandler_t* handler,
                             ErrorCode_t code,
                             ErrorSeverity_t severity,
                             const char* file,
                             u32 line,
                             const char* function)
{
    if (handler == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }

    if (handler->initialized == false) {
        return RESULT_NOT_READY;
    }

    if (code >= ERR_MAX) {
        code = ERR_UNKNOWN;
    }

    if (severity >= ERR_SEV_MAX) {
        severity = ERR_SEV_ERROR;
    }

    if (handler->callback != NULL_PTR) {
        ErrorInfo_t info;
        info.code = code;
        info.severity = severity;
        info.timestamp_ms = 0;
        info.line = line;
        info.file = file;
        info.function = function;

        handler->callback(&info);
    }

    return RESULT_OK;
}

const char* ErrorHandler_GetCodeString(ErrorCode_t code)
{
    const ErrorEntry_t* entry = find_error_entry(code);

    if (entry != NULL_PTR) {
        return entry->string;
    }

    return "ERR_UNKNOWN";
}

const char* ErrorHandler_GetSeverityString(ErrorSeverity_t severity)
{
    if (severity >= ERR_SEV_MAX) {
        return "UNKNOWN";
    }

    return severity_strings[severity];
}

bool ErrorHandler_IsRecoverable(ErrorCode_t code)
{
    const ErrorEntry_t* entry = find_error_entry(code);

    if (entry != NULL_PTR) {
        return entry->recoverable;
    }

    return false;
}

