#include "readiness.h"

static const char* const monitor_strings[] = {
    [MONITOR_MISFIRE] = "Misfire",
    [MONITOR_FUEL_SYSTEM] = "Fuel System",
    [MONITOR_COMPONENTS] = "Components",
    [MONITOR_CATALYST] = "Catalyst",
    [MONITOR_HEATED_CATALYST] = "Heated Catalyst",
    [MONITOR_EVAP_SYSTEM] = "EVAP System",
    [MONITOR_SECONDARY_AIR] = "Secondary Air",
    [MONITOR_AC_REFRIGERANT] = "A/C Refrigerant",
    [MONITOR_O2_SENSOR] = "O2 Sensor",
    [MONITOR_O2_SENSOR_HEATER] = "O2 Sensor Heater",
    [MONITOR_EGR_VVT] = "EGR/VVT",
    [MONITOR_NMHC_CATALYST] = "NMHC Catalyst",
    [MONITOR_NOX_AFTERTREATMENT] = "NOx Aftertreatment",
    [MONITOR_BOOST_PRESSURE] = "Boost Pressure",
    [MONITOR_EXHAUST_GAS_SENSOR] = "Exhaust Gas Sensor",
    [MONITOR_PM_FILTER] = "PM Filter"
};

static const char* const status_strings[] = {
    [MONITOR_STATUS_NOT_SUPPORTED] = "Not Supported",
    [MONITOR_STATUS_INCOMPLETE] = "Incomplete",
    [MONITOR_STATUS_COMPLETE] = "Complete"
};

static const char* const engine_type_strings[] = {
    [ENGINE_TYPE_SPARK] = "Spark Ignition",
    [ENGINE_TYPE_COMPRESSION] = "Compression Ignition",
    [ENGINE_TYPE_UNKNOWN] = "Unknown"
};

static void set_monitor_status(ReadinessData_t* data, MonitorType_t monitor, bool supported, bool complete)
{
    if (monitor >= MONITOR_MAX) {
        return;
    }
    
    data->monitors[monitor].type = monitor;
    data->monitors[monitor].supported = supported;
    
    if (supported == false) {
        data->monitors[monitor].status = MONITOR_STATUS_NOT_SUPPORTED;
    } else if (complete == true) {
        data->monitors[monitor].status = MONITOR_STATUS_COMPLETE;
    } else {
        data->monitors[monitor].status = MONITOR_STATUS_INCOMPLETE;
    }
}

Result_t ReadinessManager_Init(ReadinessManager_t* rm, const ReadinessManagerConfig_t* config)
{
    if (rm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (config == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    rm->data.valid = false;
    rm->data.mil_on = false;
    rm->data.dtc_count = 0U;
    rm->data.engine_type = ENGINE_TYPE_UNKNOWN;
    rm->data.timestamp_ms = 0U;
    
    for (u8 i = 0U; i < MONITOR_MAX; i++) {
        rm->data.monitors[i].type = (MonitorType_t)i;
        rm->data.monitors[i].status = MONITOR_STATUS_NOT_SUPPORTED;
        rm->data.monitors[i].supported = false;
    }
    
    rm->error_handler = config->error_handler;
    rm->callback = config->callback;
    rm->callback_context = config->callback_context;
    rm->get_timestamp_ms = config->get_timestamp_ms;
    rm->initialized = true;
    
    return RESULT_OK;
}

Result_t ReadinessManager_ProcessResponse(ReadinessManager_t* rm, const u8* data, u8 length)
{
    if (rm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (data == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (rm->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    if (length < 4U) {
        return RESULT_ERROR;
    }
    
    u8 byte_a = data[0];
    u8 byte_b = data[1];
    u8 byte_c = data[2];
    u8 byte_d = data[3];
    
    rm->data.mil_on = ((byte_a & 0x80U) != 0U);
    rm->data.dtc_count = byte_a & 0x7FU;
    
    bool is_compression = ((byte_b & 0x08U) != 0U);
    rm->data.engine_type = is_compression ? ENGINE_TYPE_COMPRESSION : ENGINE_TYPE_SPARK;
    
    set_monitor_status(&rm->data, MONITOR_MISFIRE, 
                       ((byte_b & 0x01U) != 0U), 
                       ((byte_b & 0x10U) == 0U));
    
    set_monitor_status(&rm->data, MONITOR_FUEL_SYSTEM, 
                       ((byte_b & 0x02U) != 0U), 
                       ((byte_b & 0x20U) == 0U));
    
    set_monitor_status(&rm->data, MONITOR_COMPONENTS, 
                       ((byte_b & 0x04U) != 0U), 
                       ((byte_b & 0x40U) == 0U));
    
    if (is_compression == false) {
        set_monitor_status(&rm->data, MONITOR_CATALYST, 
                           ((byte_c & 0x01U) != 0U), 
                           ((byte_d & 0x01U) == 0U));
        
        set_monitor_status(&rm->data, MONITOR_HEATED_CATALYST, 
                           ((byte_c & 0x02U) != 0U), 
                           ((byte_d & 0x02U) == 0U));
        
        set_monitor_status(&rm->data, MONITOR_EVAP_SYSTEM, 
                           ((byte_c & 0x04U) != 0U), 
                           ((byte_d & 0x04U) == 0U));
        
        set_monitor_status(&rm->data, MONITOR_SECONDARY_AIR, 
                           ((byte_c & 0x08U) != 0U), 
                           ((byte_d & 0x08U) == 0U));
        
        set_monitor_status(&rm->data, MONITOR_AC_REFRIGERANT, 
                           ((byte_c & 0x10U) != 0U), 
                           ((byte_d & 0x10U) == 0U));
        
        set_monitor_status(&rm->data, MONITOR_O2_SENSOR, 
                           ((byte_c & 0x20U) != 0U), 
                           ((byte_d & 0x20U) == 0U));
        
        set_monitor_status(&rm->data, MONITOR_O2_SENSOR_HEATER, 
                           ((byte_c & 0x40U) != 0U), 
                           ((byte_d & 0x40U) == 0U));
        
        set_monitor_status(&rm->data, MONITOR_EGR_VVT, 
                           ((byte_c & 0x80U) != 0U), 
                           ((byte_d & 0x80U) == 0U));
    } else {
        set_monitor_status(&rm->data, MONITOR_NMHC_CATALYST, 
                           ((byte_c & 0x01U) != 0U), 
                           ((byte_d & 0x01U) == 0U));
        
        set_monitor_status(&rm->data, MONITOR_NOX_AFTERTREATMENT, 
                           ((byte_c & 0x02U) != 0U), 
                           ((byte_d & 0x02U) == 0U));
        
        set_monitor_status(&rm->data, MONITOR_BOOST_PRESSURE, 
                           ((byte_c & 0x08U) != 0U), 
                           ((byte_d & 0x08U) == 0U));
        
        set_monitor_status(&rm->data, MONITOR_EXHAUST_GAS_SENSOR, 
                           ((byte_c & 0x20U) != 0U), 
                           ((byte_d & 0x20U) == 0U));
        
        set_monitor_status(&rm->data, MONITOR_PM_FILTER, 
                           ((byte_c & 0x40U) != 0U), 
                           ((byte_d & 0x40U) == 0U));
        
        set_monitor_status(&rm->data, MONITOR_EGR_VVT, 
                           ((byte_c & 0x80U) != 0U), 
                           ((byte_d & 0x80U) == 0U));
    }
    
    if (rm->get_timestamp_ms != NULL_PTR) {
        rm->data.timestamp_ms = rm->get_timestamp_ms();
    }
    
    rm->data.valid = true;
    
    if (rm->callback != NULL_PTR) {
        rm->callback(&rm->data, rm->callback_context);
    }
    
    return RESULT_OK;
}

Result_t ReadinessManager_GetData(const ReadinessManager_t* rm, ReadinessData_t* data)
{
    if (rm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (data == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (rm->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    *data = rm->data;
    
    return RESULT_OK;
}

MonitorStatus_t ReadinessManager_GetMonitorStatus(const ReadinessManager_t* rm, MonitorType_t monitor)
{
    if (rm == NULL_PTR) {
        return MONITOR_STATUS_NOT_SUPPORTED;
    }
    
    if (rm->initialized == false) {
        return MONITOR_STATUS_NOT_SUPPORTED;
    }
    
    if (monitor >= MONITOR_MAX) {
        return MONITOR_STATUS_NOT_SUPPORTED;
    }
    
    return rm->data.monitors[monitor].status;
}

bool ReadinessManager_IsMonitorSupported(const ReadinessManager_t* rm, MonitorType_t monitor)
{
    if (rm == NULL_PTR) {
        return false;
    }
    
    if (rm->initialized == false) {
        return false;
    }
    
    if (monitor >= MONITOR_MAX) {
        return false;
    }
    
    return rm->data.monitors[monitor].supported;
}

u8 ReadinessManager_GetCompleteCount(const ReadinessManager_t* rm)
{
    if (rm == NULL_PTR) {
        return 0U;
    }
    
    if (rm->initialized == false) {
        return 0U;
    }
    
    u8 count = 0U;
    
    for (u8 i = 0U; i < MONITOR_MAX; i++) {
        if (rm->data.monitors[i].status == MONITOR_STATUS_COMPLETE) {
            count++;
        }
    }
    
    return count;
}

u8 ReadinessManager_GetIncompleteCount(const ReadinessManager_t* rm)
{
    if (rm == NULL_PTR) {
        return 0U;
    }
    
    if (rm->initialized == false) {
        return 0U;
    }
    
    u8 count = 0U;
    
    for (u8 i = 0U; i < MONITOR_MAX; i++) {
        if (rm->data.monitors[i].status == MONITOR_STATUS_INCOMPLETE) {
            count++;
        }
    }
    
    return count;
}

u8 ReadinessManager_GetSupportedCount(const ReadinessManager_t* rm)
{
    if (rm == NULL_PTR) {
        return 0U;
    }
    
    if (rm->initialized == false) {
        return 0U;
    }
    
    u8 count = 0U;
    
    for (u8 i = 0U; i < MONITOR_MAX; i++) {
        if (rm->data.monitors[i].supported == true) {
            count++;
        }
    }
    
    return count;
}

EngineType_t ReadinessManager_GetEngineType(const ReadinessManager_t* rm)
{
    if (rm == NULL_PTR) {
        return ENGINE_TYPE_UNKNOWN;
    }
    
    if (rm->initialized == false) {
        return ENGINE_TYPE_UNKNOWN;
    }
    
    return rm->data.engine_type;
}

const char* ReadinessManager_GetMonitorString(MonitorType_t monitor)
{
    if (monitor >= MONITOR_MAX) {
        return "Unknown";
    }
    
    return monitor_strings[monitor];
}

const char* ReadinessManager_GetStatusString(MonitorStatus_t status)
{
    if (status >= MONITOR_STATUS_MAX) {
        return "Unknown";
    }
    
    return status_strings[status];
}

const char* ReadinessManager_GetEngineTypeString(EngineType_t type)
{
    if (type >= ENGINE_TYPE_MAX) {
        return "Unknown";
    }
    
    return engine_type_strings[type];
}
