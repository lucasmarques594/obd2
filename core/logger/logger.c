#include "logger.h"
#include <string.h>

static const char* const level_strings[] = {
    [LOG_LEVEL_DEBUG] = "DEBUG",
    [LOG_LEVEL_INFO] = "INFO",
    [LOG_LEVEL_WARNING] = "WARNING",
    [LOG_LEVEL_ERROR] = "ERROR",
    [LOG_LEVEL_NONE] = "NONE"
};

static const char* const category_strings[] = {
    [LOG_CAT_SYSTEM] = "SYSTEM",
    [LOG_CAT_COMM] = "COMM",
    [LOG_CAT_ELM] = "ELM",
    [LOG_CAT_OBD] = "OBD",
    [LOG_CAT_PID] = "PID",
    [LOG_CAT_DTC] = "DTC",
    [LOG_CAT_STATE] = "STATE",
    [LOG_CAT_SCHEDULER] = "SCHEDULER"
};

static void copy_string_safe(char* dest, const char* src, size_t max_len)
{
    if ((dest == NULL_PTR) || (max_len == 0U)) {
        return;
    }
    
    if (src == NULL_PTR) {
        dest[0] = '\0';
        return;
    }
    
    size_t i = 0U;
    while ((i < (max_len - 1U)) && (src[i] != '\0')) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

Result_t Logger_Init(Logger_t* logger, const LoggerConfig_t* config)
{
    if (logger == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (config == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    logger->head = 0U;
    logger->tail = 0U;
    logger->count = 0U;
    logger->min_level = config->min_level;
    logger->get_timestamp_ms = config->get_timestamp_ms;
    
    for (u16 i = 0U; i < LOG_BUFFER_SIZE; i++) {
        logger->entries[i].valid = false;
    }
    
    logger->initialized = true;
    
    return RESULT_OK;
}

Result_t Logger_Log(Logger_t* logger,
                    LogLevel_t level,
                    LogCategory_t category,
                    const char* command,
                    const char* response,
                    u8 protocol_id,
                    ErrorCode_t error_code)
{
    if (logger == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (logger->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    if (level < logger->min_level) {
        return RESULT_OK;
    }
    
    if (level >= LOG_LEVEL_MAX) {
        return RESULT_INVALID_PARAM;
    }
    
    if (category >= LOG_CAT_MAX) {
        return RESULT_INVALID_PARAM;
    }
    
    LogEntry_t* entry = &logger->entries[logger->head];
    
    if (logger->get_timestamp_ms != NULL_PTR) {
        entry->timestamp_ms = logger->get_timestamp_ms();
    } else {
        entry->timestamp_ms = 0U;
    }
    
    entry->level = level;
    entry->category = category;
    entry->protocol_id = protocol_id;
    entry->error_code = error_code;
    entry->valid = true;
    
    copy_string_safe(entry->command, command, LOG_ENTRY_CMD_SIZE);
    copy_string_safe(entry->response, response, LOG_ENTRY_RESP_SIZE);
    
    logger->head = (logger->head + 1U) % LOG_BUFFER_SIZE;
    
    if (logger->count < LOG_BUFFER_SIZE) {
        logger->count++;
    } else {
        logger->tail = (logger->tail + 1U) % LOG_BUFFER_SIZE;
    }
    
    return RESULT_OK;
}

Result_t Logger_GetEntry(Logger_t* logger, u16 index, LogEntry_t* entry)
{
    if ((logger == NULL_PTR) || (entry == NULL_PTR)) {
        return RESULT_INVALID_PARAM;
    }
    
    if (logger->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    if (index >= logger->count) {
        return RESULT_INVALID_PARAM;
    }
    
    u16 actual_index = (logger->tail + index) % LOG_BUFFER_SIZE;
    
    *entry = logger->entries[actual_index];
    
    return RESULT_OK;
}

Result_t Logger_GetLatest(Logger_t* logger, LogEntry_t* entry)
{
    if ((logger == NULL_PTR) || (entry == NULL_PTR)) {
        return RESULT_INVALID_PARAM;
    }
    
    if (logger->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    if (logger->count == 0U) {
        return RESULT_BUFFER_EMPTY;
    }
    
    u16 latest_index;
    if (logger->head == 0U) {
        latest_index = LOG_BUFFER_SIZE - 1U;
    } else {
        latest_index = logger->head - 1U;
    }
    
    *entry = logger->entries[latest_index];
    
    return RESULT_OK;
}

u16 Logger_GetCount(const Logger_t* logger)
{
    if (logger == NULL_PTR) {
        return 0U;
    }
    
    if (logger->initialized == false) {
        return 0U;
    }
    
    return logger->count;
}

Result_t Logger_Clear(Logger_t* logger)
{
    if (logger == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (logger->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    logger->head = 0U;
    logger->tail = 0U;
    logger->count = 0U;
    
    for (u16 i = 0U; i < LOG_BUFFER_SIZE; i++) {
        logger->entries[i].valid = false;
    }
    
    return RESULT_OK;
}

Result_t Logger_SetMinLevel(Logger_t* logger, LogLevel_t level)
{
    if (logger == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (level >= LOG_LEVEL_MAX) {
        return RESULT_INVALID_PARAM;
    }
    
    logger->min_level = level;
    
    return RESULT_OK;
}

const char* Logger_GetLevelString(LogLevel_t level)
{
    if (level >= LOG_LEVEL_MAX) {
        return "UNKNOWN";
    }
    
    return level_strings[level];
}

const char* Logger_GetCategoryString(LogCategory_t category)
{
    if (category >= LOG_CAT_MAX) {
        return "UNKNOWN";
    }
    
    return category_strings[category];
}
