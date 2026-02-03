#ifndef PID_MANAGER_H
#define PID_MANAGER_H

#include "../types.h"
#include "../obd2/obd2.h"
#include "../error/error_handler.h"

#define PID_SUPPORTED_BYTES 4
#define PID_MAX_COUNT 256
#define PID_PRIORITY_HIGH 0
#define PID_PRIORITY_MEDIUM 1
#define PID_PRIORITY_LOW 2
#define PID_PRIORITY_MAX 3

typedef enum {
    PID_UNIT_NONE = 0,
    PID_UNIT_PERCENT = 1,
    PID_UNIT_DEGREES_C = 2,
    PID_UNIT_KPA = 3,
    PID_UNIT_RPM = 4,
    PID_UNIT_KMH = 5,
    PID_UNIT_DEGREES = 6,
    PID_UNIT_GRAMS_SEC = 7,
    PID_UNIT_SECONDS = 8,
    PID_UNIT_KM = 9,
    PID_UNIT_VOLTS = 10,
    PID_UNIT_MINUTES = 11,
    PID_UNIT_RATIO = 12,
    PID_UNIT_COUNT = 13,
    PID_UNIT_PA = 14,
    PID_UNIT_MA = 15,
    PID_UNIT_NM = 16,
    PID_UNIT_LPH = 17,
    PID_UNIT_MAX
} PidUnit_t;

typedef enum {
    PID_DATA_U8 = 0,
    PID_DATA_U16 = 1,
    PID_DATA_U32 = 2,
    PID_DATA_I8 = 3,
    PID_DATA_I16 = 4,
    PID_DATA_FLOAT = 5,
    PID_DATA_BITFIELD = 6,
    PID_DATA_MAX
} PidDataType_t;

typedef struct {
    i32 raw_value;
    float eng_value;
    PidUnit_t unit;
    u32 timestamp_ms;
    bool valid;
} PidValue_t;

typedef struct {
    u8 pid;
    const char* name;
    const char* short_name;
    PidUnit_t unit;
    PidDataType_t data_type;
    u8 data_bytes;
    float min_value;
    float max_value;
    float scale;
    float offset;
    u8 priority;
    u16 default_rate_ms;
} PidDefinition_t;

typedef struct {
    u8 pid;
    bool supported;
    bool enabled;
    u16 rate_ms;
    u32 last_read_ms;
    PidValue_t value;
} PidEntry_t;

typedef void (*PidValueCallback_t)(u8 pid, const PidValue_t* value, void* context);

typedef struct {
    ErrorHandler_t* error_handler;
    PidValueCallback_t value_callback;
    void* callback_context;
    u32 (*get_timestamp_ms)(void);
} PidManagerConfig_t;

typedef struct {
    u8 supported_pids[32];
    PidEntry_t entries[64];
    u8 entry_count;
    bool initialized;
    ErrorHandler_t* error_handler;
    PidValueCallback_t value_callback;
    void* callback_context;
    u32 (*get_timestamp_ms)(void);
} PidManager_t;

Result_t PidManager_Init(PidManager_t* pm, const PidManagerConfig_t* config);

Result_t PidManager_SetSupported(PidManager_t* pm, const u8* supported_data, u8 start_pid);

bool PidManager_IsSupported(const PidManager_t* pm, u8 pid);

Result_t PidManager_EnablePid(PidManager_t* pm, u8 pid, u16 rate_ms);

Result_t PidManager_DisablePid(PidManager_t* pm, u8 pid);

Result_t PidManager_SetRate(PidManager_t* pm, u8 pid, u16 rate_ms);

Result_t PidManager_ProcessFrame(PidManager_t* pm, const Obd2Frame_t* frame);

Result_t PidManager_GetValue(const PidManager_t* pm, u8 pid, PidValue_t* value);

Result_t PidManager_GetNextPidToRead(const PidManager_t* pm, u8* pid);

const PidDefinition_t* PidManager_GetDefinition(u8 pid);

Result_t PidManager_ConvertRawToEng(u8 pid, const u8* raw_data, u8 data_len, PidValue_t* value);

const char* PidManager_GetUnitString(PidUnit_t unit);

u8 PidManager_GetSupportedCount(const PidManager_t* pm);

u8 PidManager_GetEnabledCount(const PidManager_t* pm);

#endif
