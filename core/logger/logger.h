#ifndef LOGGER_H
#define LOGGER_H

#include "../types.h"
#include "../error/error_handler.h"

#define LOG_BUFFER_SIZE 64
#define LOG_ENTRY_CMD_SIZE 32
#define LOG_ENTRY_RESP_SIZE 64

typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO = 1,
    LOG_LEVEL_WARNING = 2,
    LOG_LEVEL_ERROR = 3, 
    LOG_LEVEL_NONE = 4,
    LOG_LEVEL_MAX
} LogLevel_t;

typedef enum {
    LOG_CAT_SYSTEM = 0,
    LOG_CAT_COMM = 1,
    LOG_CAT_ELM = 2,
    LOG_CAT_OBD = 3,
    LOG_CAT_PID = 4,
    LOG_CAT_DTC = 5,
    LOG_CAT_STATE = 6,
    LOG_CAT_SCHEDULER = 7,
    LOG_CAT_MAX
} LogCategory_t;

typedef struct {
    u32 timestamp_ms;
    LogLevel_t level;
    LogCategory_t category;
    char command[LOG_ENTRY_CMD_SIZE];
    char response[LOG_ENTRY_RESP_SIZE];
    u8 protocol_id;
    ErrorCode_t error_code;
    bool valid;
} LogEntry_t;

typedef struct {
    LogEntry_t entries[LOG_BUFFER_SIZE];
    u16 head;
    u16 tail;
    u16 count;
    LogLevel_t min_level;
    bool initialized;
    u32 (*get_timestamp_ms)(void);
} Logger_t;

typedef struct {
    LogLevel_t min_level;
    u32 (*get_timestamp_ms)(void);
} LoggerConfig_t;

Result_t Logger_Init(Logger_t* logger, const LoggerConfig_t* config);

Result_t Logger_Log(Logger_t* logger,
                    LogLevel_t level,
                    LogCategory_t category,
                    const char* command,
                    const char* response,
                    u8 protocol_id,
                    ErrorCode_t error_code);

Result_t Logger_GetEntry(Logger_t* logger, u16 index, LogEntry_t* entry);

Result_t Logger_GetLatest(Logger_t* logger, LogEntry_t* entry);

u16 Logger_GetCount(const Logger_t* logger);

Result_t Logger_Clear(Logger_t* logger);

Result_t Logger_SetMinLevel(Logger_t* logger, LogLevel_t level);

const char* Logger_GetLevelString(LogLevel_t level);

const char* Logger_GetCategoryString(LogCategory_t category);

#define LOG_DEBUG(logger, cat, cmd, resp, proto) \
    Logger_Log((logger), LOG_LEVEL_DEBUG, (cat), (cmd), (resp), (proto), ERR_NONE)

#define LOG_INFO(logger, cat, cmd, resp, proto) \
    Logger_Log((logger), LOG_LEVEL_INFO, (cat), (cmd), (resp), (proto), ERR_NONE)

#define LOG_WARNING(logger, cat, cmd, resp, proto, err) \
    Logger_Log((logger), LOG_LEVEL_WARNING, (cat), (cmd), (resp), (proto), (err))

#define LOG_ERROR(logger, cat, cmd, resp, proto, err) \
    Logger_Log((logger), LOG_LEVEL_ERROR, (cat), (cmd), (resp), (proto), (err))

#endif
