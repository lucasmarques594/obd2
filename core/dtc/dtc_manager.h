#ifndef DTC_MANAGER_H
#define DTC_MANAGER_H

#include "../types.h"
#include "../obd2/obd2.h"
#include "../error/error_handler.h"

#define DTC_MAX_COUNT 32
#define DTC_CODE_STRING_LEN 6

typedef enum {
    DTC_TYPE_CURRENT = 0,
    DTC_TYPE_PENDING = 1,
    DTC_TYPE_PERMANENT = 2,
    DTC_TYPE_STORED = 3,
    DTC_TYPE_MAX
} DtcType_t;

typedef enum {
    DTC_SYSTEM_POWERTRAIN = 0,
    DTC_SYSTEM_CHASSIS = 1,
    DTC_SYSTEM_BODY = 2,
    DTC_SYSTEM_NETWORK = 3,
    DTC_SYSTEM_MAX
} DtcSystem_t;

typedef struct {
    u16 raw_code;
    char code_string[DTC_CODE_STRING_LEN];
    DtcType_t type;
    DtcSystem_t system;
    bool valid;
    u32 timestamp_ms;
} Dtc_t;

typedef struct {
    Dtc_t current[DTC_MAX_COUNT];
    u8 current_count;
    Dtc_t pending[DTC_MAX_COUNT];
    u8 pending_count;
    Dtc_t permanent[DTC_MAX_COUNT];
    u8 permanent_count;
} DtcStorage_t;

typedef void (*DtcCallback_t)(const Dtc_t* dtc, void* context);
typedef void (*DtcClearedCallback_t)(void* context);

typedef struct {
    ErrorHandler_t* error_handler;
    DtcCallback_t dtc_callback;
    DtcClearedCallback_t cleared_callback;
    void* callback_context;
    u32 (*get_timestamp_ms)(void);
} DtcManagerConfig_t;

typedef struct {
    DtcStorage_t storage;
    bool mil_status;
    u8 dtc_count_ecu;
    bool initialized;
    ErrorHandler_t* error_handler;
    DtcCallback_t dtc_callback;
    DtcClearedCallback_t cleared_callback;
    void* callback_context;
    u32 (*get_timestamp_ms)(void);
} DtcManager_t;

Result_t DtcManager_Init(DtcManager_t* dm, const DtcManagerConfig_t* config);

Result_t DtcManager_ProcessResponse(DtcManager_t* dm, const u8* data, u16 length, DtcType_t type);

Result_t DtcManager_ParseDtc(u16 raw_code, Dtc_t* dtc);

Result_t DtcManager_Clear(DtcManager_t* dm);

u8 DtcManager_GetCount(const DtcManager_t* dm, DtcType_t type);

u8 DtcManager_GetTotalCount(const DtcManager_t* dm);

Result_t DtcManager_GetDtc(const DtcManager_t* dm, DtcType_t type, u8 index, Dtc_t* dtc);

Result_t DtcManager_GetAllDtcs(const DtcManager_t* dm, DtcType_t type, Dtc_t* dtcs, u8 max_count, u8* actual_count);

bool DtcManager_GetMilStatus(const DtcManager_t* dm);

Result_t DtcManager_SetMilStatus(DtcManager_t* dm, bool status, u8 dtc_count);

const char* DtcManager_GetTypeString(DtcType_t type);

const char* DtcManager_GetSystemString(DtcSystem_t system);

Result_t DtcManager_CodeToString(u16 raw_code, char* buffer, u8 buffer_len);

#endif
