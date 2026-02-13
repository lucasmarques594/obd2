#include "error_handler.h"

static const char* const error_code_strings[] = {
    [ERR_NONE] = "ERR_NONE",
    [ERR_COMM_TIMEOUT] = "ERR_COMM_TIMEOUT",
    [ERR_COMM_NO_RESPONSE] = "ERR_COMM_NO_RESPONSE",
    [ERR_COMM_INVALID_RESPONSE] = "ERR_COMM_INVALID_RESPONSE",
    [ERR_COMM_BUFFER_OVERFLOW] = "ERR_COMM_BUFFER_OVERFLOW",
    [ERR_COMM_WRITE_FAILED] = "ERR_COMM_WRITE_FAILED",
    [ERR_COMM_READ_FAILED] = "ERR_COMM_READ_FAILED",
    [ERR_COMM_DISCONNECTED] = "ERR_COMM_DISCONNECTED",
    [ERR_ELM_INIT_FAILED] = "ERR_ELM_INIT_FAILED",
    [ERR_ELM_RESET_FAILED] = "ERR_ELM_RESET_FAILED",
    [ERR_ELM_PROTOCOL_FAILED] = "ERR_ELM_PROTOCOL_FAILED",
    [ERR_ELM_NO_DATA] = "ERR_ELM_NO_DATA",
    [ERR_ELM_STOPPED] = "ERR_ELM_STOPPED",
    [ERR_ELM_BUFFER_FULL] = "ERR_ELM_BUFFER_FULL",
    [ERR_ELM_CAN_ERROR] = "ERR_ELM_CAN_ERROR",
    [ERR_ELM_BUS_INIT_ERROR] = "ERR_ELM_BUS_INIT_ERROR",
    [ERR_ELM_UNABLE_TO_CONNECT] = "ERR_ELM_UNABLE_TO_CONNECT",
    [ERR_OBD_NO_RESPONSE] = "ERR_OBD_NO_RESPONSE",
    [ERR_OBD_INVALID_MODE] = "ERR_OBD_INVALID_MODE",
    [ERR_OBD_INVALID_PID] = "ERR_OBD_INVALID_PID",
    [ERR_OBD_NEGATIVE_RESPONSE] = "ERR_OBD_NEGATIVE_RESPONSE",
    [ERR_OBD_CHECKSUM_FAILED] = "ERR_OBD_CHECKSUM_FAILED",
    [ERR_OBD_FRAME_ERROR] = "ERR_OBD_FRAME_ERROR",
    [ERR_STATE_INVALID_TRANSITION] = "ERR_STATE_INVALID_TRANSITION",
    [ERR_STATE_TIMEOUT] = "ERR_STATE_TIMEOUT",
    [ERR_STATE_MAX_RETRIES] = "ERR_STATE_MAX_RETRIES",
    [ERR_SANITY_OUT_OF_RANGE] = "ERR_SANITY_OUT_OF_RANGE",
    [ERR_SANITY_SENSOR_STUCK] = "ERR_SANITY_SENSOR_STUCK",
    [ERR_SANITY_INVALID_DATA] = "ERR_SANITY_INVALID_DATA",
    [ERR_MEMORY_ALLOCATION] = "ERR_MEMORY_ALLOCATION",
    [ERR_MEMORY_OVERFLOW] = "ERR_MEMORY_OVERFLOW",
    [ERR_PARAM_NULL_POINTER] = "ERR_PARAM_NULL_POINTER",
    [ERR_PARAM_INVALID_VALUE] = "ERR_PARAM_INVALID_VALUE",
    [ERR_PARAM_OUT_OF_BOUNDS] = "ERR_PARAM_OUT_OF_BOUNDS",
    [ERR_SCHEDULER_QUEUE_FULL] = "ERR_SCHEDULER_QUEUE_FULL",
    [ERR_SCHEDULER_TASK_NOT_FOUND] = "ERR_SCHEDULER_TASK_NOT_FOUND",
    [ERR_UNKNOWN] = "ERR_UNKNOWN"
};

static const char* const severity_strings[] = {
    [ERR_SEV_INFO] = "INFO",
    [ERR_SEV_WARNING] = "WARNING",
    [ERR_SEV_ERROR] = "ERROR",
    [ERR_SEV_CRITICAL] = "CRITICAL"
};

static const bool recoverable_errors[] = {
    [ERR_NONE] = true,
    [ERR_COMM_TIMEOUT] = true,
    [ERR_COMM_NO_RESPONSE] = true,
    [ERR_COMM_INVALID_RESPONSE] = true,
    [ERR_COMM_BUFFER_OVERFLOW] = true,
    [ERR_COMM_WRITE_FAILED] = true,
    [ERR_COMM_READ_FAILED] = true,
    [ERR_COMM_DISCONNECTED] = false,
    [ERR_ELM_INIT_FAILED] = true,
    [ERR_ELM_RESET_FAILED] = true,
    [ERR_ELM_PROTOCOL_FAILED] = true,
    [ERR_ELM_NO_DATA] = true,
    [ERR_ELM_STOPPED] = true,
    [ERR_ELM_BUFFER_FULL] = true,
    [ERR_ELM_CAN_ERROR] = true,
    [ERR_ELM_BUS_INIT_ERROR] = true,
    [ERR_ELM_UNABLE_TO_CONNECT] = false,
    [ERR_OBD_NO_RESPONSE] = true,
    [ERR_OBD_INVALID_MODE] = false,
    [ERR_OBD_INVALID_PID] = false,
    [ERR_OBD_NEGATIVE_RESPONSE] = true,
    [ERR_OBD_CHECKSUM_FAILED] = true,
    [ERR_OBD_FRAME_ERROR] = true,
    [ERR_STATE_INVALID_TRANSITION] = false,
    [ERR_STATE_TIMEOUT] = true,
    [ERR_STATE_MAX_RETRIES] = true,
    [ERR_SANITY_OUT_OF_RANGE] = true,
    [ERR_SANITY_SENSOR_STUCK] = true,
    [ERR_SANITY_INVALID_DATA] = true,
    [ERR_MEMORY_ALLOCATION] = false,
    [ERR_MEMORY_OVERFLOW] = false,
    [ERR_PARAM_NULL_POINTER] = false,
    [ERR_PARAM_INVALID_VALUE] = false,
    [ERR_PARAM_OUT_OF_BOUNDS] = false,
    [ERR_SCHEDULER_QUEUE_FULL] = true,
    [ERR_SCHEDULER_TASK_NOT_FOUND] = false,
    [ERR_UNKNOWN] = false
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
    if (code >= ERR_MAX) {
        return "ERR_UNKNOWN";
    }
    
    if (error_code_strings[code] == NULL_PTR) {
        return "ERR_UNKNOWN";
    }
    
    return error_code_strings[code];
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
    if (code >= ERR_MAX) {
        return false;
    }
    
    return recoverable_errors[code];
}
