#ifndef READINESS_H
#define READINESS_H

#include "../types.h"
#include "../error/error_handler.h"

typedef enum {
    MONITOR_MISFIRE = 0,
    MONITOR_FUEL_SYSTEM = 1,
    MONITOR_COMPONENTS = 2,
    MONITOR_CATALYST = 3,
    MONITOR_HEATED_CATALYST = 4,
    MONITOR_EVAP_SYSTEM = 5,
    MONITOR_SECONDARY_AIR = 6,
    MONITOR_AC_REFRIGERANT = 7,
    MONITOR_O2_SENSOR = 8,
    MONITOR_O2_SENSOR_HEATER = 9,
    MONITOR_EGR_VVT = 10,
    MONITOR_NMHC_CATALYST = 11,
    MONITOR_NOX_AFTERTREATMENT = 12,
    MONITOR_BOOST_PRESSURE = 13,
    MONITOR_EXHAUST_GAS_SENSOR = 14,
    MONITOR_PM_FILTER = 15,
    MONITOR_MAX
} MonitorType_t;

typedef enum {
    MONITOR_STATUS_NOT_SUPPORTED = 0,
    MONITOR_STATUS_INCOMPLETE = 1,
    MONITOR_STATUS_COMPLETE = 2,
    MONITOR_STATUS_MAX
} MonitorStatus_t;

typedef enum {
    ENGINE_TYPE_SPARK = 0,
    ENGINE_TYPE_COMPRESSION = 1,
    ENGINE_TYPE_UNKNOWN = 2,
    ENGINE_TYPE_MAX
} EngineType_t;

typedef struct {
    MonitorType_t type;
    MonitorStatus_t status;
    bool supported;
} MonitorInfo_t;

typedef struct {
    MonitorInfo_t monitors[MONITOR_MAX];
    EngineType_t engine_type;
    bool mil_on;
    u8 dtc_count;
    u32 timestamp_ms;
    bool valid;
} ReadinessData_t;

typedef void (*ReadinessCallback_t)(const ReadinessData_t* data, void* context);

typedef struct {
    ErrorHandler_t* error_handler;
    ReadinessCallback_t callback;
    void* callback_context;
    u32 (*get_timestamp_ms)(void);
} ReadinessManagerConfig_t;

typedef struct {
    ReadinessData_t data;
    bool initialized;
    ErrorHandler_t* error_handler;
    ReadinessCallback_t callback;
    void* callback_context;
    u32 (*get_timestamp_ms)(void);
} ReadinessManager_t;

Result_t ReadinessManager_Init(ReadinessManager_t* rm, const ReadinessManagerConfig_t* config);

Result_t ReadinessManager_ProcessResponse(ReadinessManager_t* rm, const u8* data, u8 length);

Result_t ReadinessManager_GetData(const ReadinessManager_t* rm, ReadinessData_t* data);

MonitorStatus_t ReadinessManager_GetMonitorStatus(const ReadinessManager_t* rm, MonitorType_t monitor);

bool ReadinessManager_IsMonitorSupported(const ReadinessManager_t* rm, MonitorType_t monitor);

u8 ReadinessManager_GetCompleteCount(const ReadinessManager_t* rm);

u8 ReadinessManager_GetIncompleteCount(const ReadinessManager_t* rm);

u8 ReadinessManager_GetSupportedCount(const ReadinessManager_t* rm);

EngineType_t ReadinessManager_GetEngineType(const ReadinessManager_t* rm);

const char* ReadinessManager_GetMonitorString(MonitorType_t monitor);

const char* ReadinessManager_GetStatusString(MonitorStatus_t status);

const char* ReadinessManager_GetEngineTypeString(EngineType_t type);

#endif
